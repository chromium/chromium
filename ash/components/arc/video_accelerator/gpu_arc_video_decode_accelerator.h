// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODE_ACCELERATOR_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODE_ACCELERATOR_H_

#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/video_decode_accelerator.mojom.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_checker.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class DecoderProtectedBufferManager;

// GpuArcVideoDecodeAccelerator is executed in the GPU process.
// It takes decoding requests from ARC via IPC channels and translates and
// sends those requests to an implementation of media::VideoDecodeAccelerator.
// It also calls ARC client functions in media::VideoDecodeAccelerator
// callbacks, e.g., PictureReady, which returns the decoded frames back to the
// ARC side. This class manages Reset and Flush requests and life-cycle of
// passed callback for them. They would be processed in FIFO order.

// For each creation request from GpuArcVideoDecodeAcceleratorHost,
// GpuArcVideoDecodeAccelerator will create a new IPC channel.
class GpuArcVideoDecodeAccelerator
    : public mojom::VideoDecodeAccelerator,
      public media::VideoDecodeAccelerator::Client {
 public:
  GpuArcVideoDecodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      scoped_refptr<DecoderProtectedBufferManager> protected_buffer_manager);

  GpuArcVideoDecodeAccelerator(const GpuArcVideoDecodeAccelerator&) = delete;
  GpuArcVideoDecodeAccelerator& operator=(const GpuArcVideoDecodeAccelerator&) =
      delete;

  ~GpuArcVideoDecodeAccelerator() override;

  // Implementation of media::VideoDecodeAccelerator::Client interface.
  void NotifyInitializationComplete(media::DecoderStatus status) override;
  void ProvidePictureBuffersWithVisibleRect(
      uint32_t requested_num_of_buffers,
      media::VideoPixelFormat format,
      const gfx::Size& dimensions,
      const gfx::Rect& visible_rect) override;
  void PictureReady(const media::Picture& picture) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(media::VideoDecodeAccelerator::Error error) override;

  // mojom::VideoDecodeAccelerator implementation.
  void Initialize(mojom::VideoDecodeAcceleratorConfigPtr config,
                  mojo::PendingRemote<mojom::VideoDecodeClient> client,
                  InitializeCallback callback) override;
  void Decode(mojom::BitstreamBufferPtr bitstream_buffer) override;
  void AssignPictureBuffers(uint32_t count) override;
  void ImportBufferForPicture(int32_t picture_buffer_id,
                              mojom::HalPixelFormat format,
                              mojo::ScopedHandle handle,
                              std::vector<VideoFramePlane> planes,
                              mojom::BufferModifierPtr modifier) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush(FlushCallback callback) override;
  void Reset(ResetCallback callback) override;

 private:
  using PendingCallback =
      base::OnceCallback<void(mojom::VideoDecodeAccelerator::Result)>;
  static_assert(std::is_same<ResetCallback, PendingCallback>::value,
                "The type of PendingCallback must match ResetCallback");
  static_assert(std::is_same<FlushCallback, PendingCallback>::value,
                "The type of PendingCallback must match FlushCallback");
  static_assert(std::is_same<InitializeCallback, PendingCallback>::value,
                "The type of PendingCallback must match InitializeCallback");
  using PendingRequest =
      base::OnceCallback<void(PendingCallback, media::VideoDecodeAccelerator*)>;

  // Initialize GpuArcVDA and create VDA. OnInitializeDone() will be called with
  // the result of the initialization.
  void InitializeTask(mojom::VideoDecodeAcceleratorConfigPtr config);
  // Called when initialization is done.
  void OnInitializeDone(mojom::VideoDecodeAccelerator::Result result);
  // Proxy callback for re-initialize in encrypted mode in the case of error.
  void OnReinitializeDone(mojom::VideoDecodeAccelerator::Result result);

  // Called after getting the input shared memory region from the
  // |protected_buffer_manager_|, if required. Otherwise, Decode() calls this
  // directly.
  void ContinueDecode(mojom::BitstreamBufferPtr bitstream_buffer,
                      base::ScopedFD handle_fd,
                      base::UnsafeSharedMemoryRegion shm_region);

  // Posted as a task after getting the result of the first query to the
  // |protected_buffer_manager_| in order to resume decode tasks that were
  // waiting for that result.
  void ResumeDecodingAfterFirstSecureBuffer();

  // Called after getting the output native pixmap handle from the
  // |protected_buffer_manager_|, if required. Otherwise,
  // ImportBufferForPicture() calls this directly.
  void ContinueImportBufferForPicture(
      int32_t picture_buffer_id,
      media::VideoPixelFormat pixel_format,
      gfx::NativePixmapHandle native_pixmap_handle);

  // Execute all pending requests until a VDA::Reset() request is encountered.
  // When that happens, we need to explicitly wait for NotifyResetDone().
  // before we continue executing subsequent requests.
  void RunPendingRequests();

  // When |pending_reset_callback_| isn't null, GAVDA is awaiting a preceding
  // Reset() to be finished. When |pending_init_callback_| isn't null, GAVDA is
  // awaiting a re-init to move the decoder into secure mode. In both cases
  // |request| is pended by queueing in |pending_requests_|. Otherwise, the
  // requested VDA operation is executed. In the case of Flush request, the
  // callback is queued to |pending_flush_callbacks_|. In the case of Reset
  // request, the callback is set |pending_reset_callback_|.
  void ExecuteRequest(std::pair<PendingRequest, PendingCallback> request);

  // Requested VDA methods are executed in these functions.
  void FlushRequest(PendingCallback cb, media::VideoDecodeAccelerator* vda);
  void ResetRequest(PendingCallback cb, media::VideoDecodeAccelerator* vda);
  void DecodeRequest(media::BitstreamBuffer bitstream_buffer,
                     PendingCallback cb,
                     media::VideoDecodeAccelerator* vda);

  // Call the ProvidePictureBuffers() to the client.
  void HandleProvidePictureBuffers(uint32_t requested_num_of_buffers,
                                   const gfx::Size& dimensions,
                                   const gfx::Rect& visible_rect);
  // Call the pending ProvidePictureBuffers() to the client if needed.
  bool CallPendingProvidePictureBuffers();
  // Called when the AssignPictureBuffers() is called by the client.
  void OnAssignPictureBuffersCalled(const gfx::Size& dimensions,
                                    uint32_t count);

  // Global counter that keeps track of the number of concurrent
  // GpuArcVideoDecodeAccelerator instances.
  // Since this class only works on the same thread, it's safe to access
  // |instance_count_| without lock.
  static int instance_count_;

  // Similar to |instance_count_| but only counts the number of concurrent
  // initialized GpuArcVideoDecodeAccelerator instances (i.e., instances that
  // have a |vda_|.
  // Since this class only works on the same thread, it's safe to access
  // |initialized_instance_count_| without lock.
  static int initialized_instance_count_;

  // |error_state_| is true, if GAVDA gets an error from VDA.
  // All the pending functions are cancelled and the callbacks are
  // executed with an error state.
  bool error_state_ = false;

  // The variables for managing callbacks.
  // VDA::Decode(), VDA::Flush() and VDA::Reset() should not be posted to VDA
  // while the previous Reset() hasn't been finished yet (i.e. before
  // NotifyResetDone() is invoked). Same thing goes for when we are
  // re-initializing to enter secure mode. Those requests will be queued in
  // |pending_requests_| in a FIFO manner, and will be executed once all the
  // preceding Reset() or Initialize() have been finished.
  // |pending_flush_callbacks_| stores all the callbacks corresponding to
  // currently executing Flush()es in VDA. |pending_reset_callback_| is a
  // callback of the currently executing Reset() in VDA.
  // |pending_init_callback_| is a callback of the currently executing Create()
  // or re-execution of Initialize() for the decoder.
  // If |pending_flush_callbacks_| is not empty in NotifyResetDone(),
  // as Flush()es may be cancelled by Reset() in VDA, they are called with
  // CANCELLED.
  // In |pending_requests_|, PendingRequest is Reset/Flush/DecodeRequest().
  // PendingCallback is null in the case of Decode().
  // Otherwise, it isn't nullptr and will have to be called eventually.
  InitializeCallback pending_init_callback_;
  std::queue<std::pair<PendingRequest, PendingCallback>> pending_requests_;
  std::queue<FlushCallback> pending_flush_callbacks_;
  ResetCallback pending_reset_callback_;

  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  std::unique_ptr<media::VideoDecodeAccelerator> vda_;
  mojo::Remote<mojom::VideoDecodeClient> client_;

  gfx::Size coded_size_;
  gfx::Size pending_coded_size_;
  media::VideoCodecProfile profile_ = media::VIDEO_CODEC_PROFILE_UNKNOWN;

  scoped_refptr<DecoderProtectedBufferManager> protected_buffer_manager_;

  std::optional<bool> secure_mode_ = std::nullopt;
  size_t output_buffer_count_ = 0;

  // When the client resets VDA during requesting new buffers, then VDA will
  // request new buffers again. These variables are used to handle multiple
  // ProvidePictureBuffers() requests.
  // The pending ProvidePictureBuffers() requests.
  std::queue<base::OnceClosure> pending_provide_picture_buffers_requests_;
  // The callback of the current ProvidePictureBuffers() requests.
  base::OnceCallback<void(uint32_t)> current_provide_picture_buffers_cb_;
  // Set to true when the last ProvidePictureBuffers() is replied.
  bool awaiting_first_import_ = false;

  // |first_input_waiting_on_secure_buffer_| is set when we're waiting for the
  // |protected_buffer_manager_| to reply to the first query for the shared
  // memory region corresponding to a dummy FD. When set, its value is the
  // bitstream buffer ID of the input buffer that caused us to query the
  // |protected_buffer_manager_|. Also, when set, we queue incoming Decode()
  // requests in |decode_requests_waiting_for_first_secure_buffer_| for later
  // use.
  std::optional<int32_t> first_input_waiting_on_secure_buffer_;
  std::queue<std::pair<int32_t, base::OnceClosure>>
      decode_requests_waiting_for_first_secure_buffer_;

  THREAD_CHECKER(thread_checker_);

  // The one for input buffers lives until ResetRequest().
  base::WeakPtrFactory<GpuArcVideoDecodeAccelerator>
      weak_ptr_factory_for_querying_protected_input_buffers_{this};
  // This one lives for the lifetime of the object.
  base::WeakPtrFactory<GpuArcVideoDecodeAccelerator> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODE_ACCELERATOR_H_
