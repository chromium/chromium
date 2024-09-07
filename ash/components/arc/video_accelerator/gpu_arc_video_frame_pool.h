// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_FRAME_POOL_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_FRAME_POOL_H_

#include <optional>
#include <vector>

#include "ash/components/arc/mojom/video_frame_pool.mojom.h"
#include "ash/components/arc/video_accelerator/video_frame_plane.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/status.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/vda_video_frame_pool.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace arc {

class ProtectedBufferManager;

// Arc video frame pool mojo service, managing a set of video frames for a
// remote mojo client. The client is responsible for creating and adding video
// frames to the pool, so they can be used by the associated video decoder.
// Note: The video frame pool is currently implemented as a client of the
//       VdaVideoFramePool. Once the wrapper-based solution used in the
//       GpuArcVideoDecodeAccelerator has been deprecated the VdaVideoFramePool
//       can be rolled into this class.
class GpuArcVideoFramePool : public mojom::VideoFramePool,
                             public media::VdaVideoFramePool::VdaDelegate {
 public:
  GpuArcVideoFramePool(
      mojo::PendingAssociatedReceiver<mojom::VideoFramePool> video_frame_pool,
      base::RepeatingClosure release_client_video_frames_cb,
      scoped_refptr<ProtectedBufferManager> protected_buffer_manager);
  ~GpuArcVideoFramePool() override;

  GpuArcVideoFramePool(const GpuArcVideoFramePool&) = delete;
  GpuArcVideoFramePool& operator=(const GpuArcVideoFramePool&) = delete;
  GpuArcVideoFramePool(GpuArcVideoFramePool&&) = delete;
  GpuArcVideoFramePool& operator=(GpuArcVideoFramePool&&) = delete;

  // Implementation of mojom::VideoFramePool.
  void Initialize(mojo::PendingAssociatedRemote<mojom::VideoFramePoolClient>
                      client) override;
  void AddVideoFrame(mojom::VideoFramePtr video_frame,
                     AddVideoFrameCallback callback) override;

  // The GpuArcVideoDecoder should call this method when it's about to call
  // Reset() on the underlying VideoDecoder. This is useful to interrupt a
  // request for frames if a reset request comes in the middle of it.
  void WillResetDecoder();

  // The GpuArcVideoDecoder should call this method when a VideoDecoder::Reset()
  // request has been completed. This is needed to start handling requests for
  // frames after a WillResetDecoder() call.
  void OnDecoderResetDone();

  // Implementation of VdaVideoFramePool::VdaDelegate.
  // RequestFrames will be called upon initialization and resolution changes.
  // The request is forwarded to the mojo client using RequestVideoFrames(). The
  // client then calls AddVideoFrame() N times to populate the pool.
  void RequestFrames(const media::Fourcc& fourcc,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     size_t max_num_frames,
                     NotifyLayoutChangedCb notify_layout_changed_cb,
                     ImportFrameCb import_frame_cb) override;
  media::VideoFrame::StorageType GetFrameStorageType() const override;

  // Get the mojo frame ID associated with the specified |video_frame|.
  std::optional<int32_t> GetVideoFrameId(const media::VideoFrame* video_frame);

  // Called when all references to a video frame have been dropped.
  void OnFrameReleased(scoped_refptr<media::FrameResource> origin_frame);

  // Get a weak reference to the video frame pool.
  base::WeakPtr<GpuArcVideoFramePool> WeakThis() { return weak_this_; }

 private:
  // Create a GPU memory handle from the specified |fd|.
  gfx::GpuMemoryBufferHandle CreateGpuMemoryHandle(
      base::ScopedFD fd,
      const std::vector<VideoFramePlane>& planes,
      media::VideoPixelFormat pixel_format,
      uint64_t modifier);

  // Create a frame from the specified |gmb_handle|.
  scoped_refptr<media::FrameResource> CreateFrame(
      gfx::GpuMemoryBufferHandle gmb_handle,
      media::VideoPixelFormat pixel_format) const;

  // Submits a RequestFrames() call to |pool_client_|.
  void HandleRequestFrames(const media::Fourcc& fourcc,
                           const gfx::Size& coded_size,
                           const gfx::Rect& visible_rect,
                           size_t max_num_frames);

  // Checks if it's time to submit the next previously deferred RequestFrames()
  // call to the remote end.
  void CallPendingRequestFrames();

  void OnRequestVideoFramesDone();

  void Stop();

  // The local video frame pool mojo service.
  mojo::AssociatedReceiver<mojom::VideoFramePool> video_frame_pool_receiver_;
  // The remote video frame pool mojo client.
  mojo::AssociatedRemote<mojom::VideoFramePoolClient> pool_client_;
  std::optional<uint32_t> pool_client_version_;

  // callback used to notify the video frame pool of new video frame formats,
  // used when the pool requests new frames using RequestFrames().
  NotifyLayoutChangedCb notify_layout_changed_cb_;
  // Callback used to insert new frames in the video frame pool.
  ImportFrameCb import_frame_cb_;
  // Callback used to notify the video decoder that it should release its
  // references to the client video frames as the pool will no longer track
  // them.
  base::RepeatingClosure release_client_video_frames_cb_;

  // The coded size currently used for video frames.
  gfx::Size coded_size_;

  // Map of video frame buffer ids to the associated video frame ids.
  std::map<gfx::GenericSharedMemoryId, int32_t> buffer_id_to_video_frame_id_;

  // The protected buffer manager, used when decoding an encrypted video.
  scoped_refptr<ProtectedBufferManager> protected_buffer_manager_;
  // Whether we're decoding an encrypted video.
  std::optional<bool> secure_mode_;

  // If true, this pool is waiting from ACK message from the client after
  // calling mojom::VideoFramePoolClient::RequestVideoFrames().
  bool awaiting_request_frames_ack_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  static constexpr uint32_t kMinVersionForRequestFramesAck = 1;

  // This queue tracks calls to RequestFrames() that can't be submitted to the
  // remote end yet because we're still waiting for an ACK for a previous
  // |pool_client_|->RequestVideoFrames() call.
  //
  // The reason to hold off calling RequestVideoFrames() while waiting on an ACK
  // for a previous call is a specific corner case: suppose we get a Reset()
  // while waiting for an ACK. In that case, we'll unblock the underlying
  // VdaVideoFramePool by calling |notify_layout_changed_cb_| with
  // kResetRequired. Then the decoder might produce another request for frames.
  // However, we are still waiting for an incoming ACK for the first request
  // (the one that was aborted). Until then, it's expected that we don't submit
  // the second request. This expectation was inferred from
  // https://crrev.com/c/3070325 which does something analogous but for the
  // legacy VDA stack.
  //
  // This queue is intended to be used together with
  // |awaiting_request_frames_ack_|.
  std::queue<base::OnceClosure> pending_frame_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // When |decoder_is_resetting_| is true, the decoder is in the middle of a
  // Reset() so we should reject requests for frames.
  bool decoder_is_resetting_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  bool has_error_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The client task runner and its sequence checker. All methods should be run
  // on this task runner.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<GpuArcVideoFramePool> weak_this_;
  base::WeakPtrFactory<GpuArcVideoFramePool> weak_this_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_FRAME_POOL_H_
