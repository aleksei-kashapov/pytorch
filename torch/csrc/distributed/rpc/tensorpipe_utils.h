#pragma once

#ifdef USE_TENSORPIPE

#include <torch/csrc/distributed/rpc/macros.h>
#include <torch/csrc/distributed/rpc/utils.h>

#ifdef USE_CUDA_NOT_ROCM
#include <c10/cuda/CUDAStream.h>
#endif

namespace tensorpipe {
class Message;
} // namespace tensorpipe

namespace torch {
namespace distributed {
namespace rpc {

// A struct that holds pointers that keep alive all the memory that will be
// accessed by TensorPipe during a write operation.
struct TensorpipeWriteBuffers {
  // Allocate on heap so pointers stay valid as we move the holder.
  std::unique_ptr<MessageType> type;
  std::unique_ptr<int64_t> id;
  std::vector<char> payload;
  std::vector<char> pickle;
  // This contains the original tensors and the clones of the sparse tensors.
  std::vector<torch::Tensor> tensors;
  // This contains the copies of the data of the tensors that didn't own their
  // memory, e.g., the ones created from torch::from_blob() with no deleter.
  std::vector<std::vector<char>> copiedTensors;
};

// A struct that holds pointers that keep alive all the memory that will be
// accessed by TensorPipe during a read operation.
struct TensorpipeReadBuffers {
  // Allocate on heap so pointers stay valid as we move the holder.
  std::unique_ptr<MessageType> type;
  std::unique_ptr<int64_t> id;
  std::vector<char> payload;
  std::vector<char> pickle;
  std::vector<c10::DataPtr> tensors;
};

inline std::shared_ptr<LazyStreamContext> createLazyStreamContext() {
  return createLazyStreamContext(
#ifdef USE_CUDA_NOT_ROCM
      c10::DeviceType::CUDA,
      [](c10::DeviceIndex index) {
        return at::cuda::getStreamFromPool(
            /* isHighPriority */ false, /* device */ index);
      },
      [](c10::DeviceIndex index) {
        return at::cuda::getCurrentCUDAStream(index);
      }
#else
      c10::DeviceType::CPU, nullptr, nullptr
#endif
  );
}

// Convert an RPC message into a TensorPipe message, plus a holder to all the
// data that must be kept alive while the write is performed asynchronously.
TORCH_API std::tuple<tensorpipe::Message, TensorpipeWriteBuffers>
tensorpipeSerialize(
    Message&& rpcMessage,
    std::vector<c10::DeviceIndex> devices = {},
    const std::shared_ptr<LazyStreamContext>& = createLazyStreamContext());

// Allocate the buffers that will hold the incoming data. They will be managed
// by the returned holder, which must be kept alive until the asynchronous read
// has finished. Pointers to these buffers will be stored in-place in the
// TensorPipe message.
TORCH_API TensorpipeReadBuffers tensorpipeAllocate(
    tensorpipe::Message& tpMessage,
    const std::shared_ptr<LazyStreamContext>& ctx = createLazyStreamContext());

// Convert a TensorPipe message back into an RPC message. This requires the data
// to be available and can thus only be performed once the asynchronous read has
// completed. The holder can be destroyed once this function returns.
TORCH_API Message tensorpipeDeserialize(
    tensorpipe::Message&& tpMessage,
    TensorpipeReadBuffers&& holder);

} // namespace rpc
} // namespace distributed
} // namespace torch

#endif // USE_TENSORPIPE
