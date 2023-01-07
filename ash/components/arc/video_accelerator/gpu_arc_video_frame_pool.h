// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_FRAME_POOL_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_FRAME_POOL_H_

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
#include "third_party/abseil-cpp/absl/types/optional.h"
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
      base::RepeatingClosure request_frames_cb,
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

  // Get the id associated with the specified |video_frame|.
  absl::optional<int32_t> GetVideoFrameId(const media::VideoFrame* video_frame);

  // Called when all references to a video frame have been dropped.
  void OnFrameReleased(scoped_refptr<media::VideoFrame> origin_frame);

  // Get a weak reference to the video frame pool.
  base::WeakPtr<GpuArcVideoFramePool> WeakThis() { return weak_this_; }

 private:
  // Create a GPU memory handle from the specified |fd|.
  gfx::GpuMemoryBufferHandle CreateGpuMemoryHandle(
      base::ScopedFD fd,
      const std::vector<VideoFramePlane>& planes,
      media::VideoPixelFormat pixel_format,
      uint64_t modifier);

  // Create a video frame from the specified |gmb_handle|.
  scoped_refptr<media::VideoFrame> CreateVideoFrame(
      gfx::GpuMemoryBufferHandle gmb_handle,
      media::VideoPixelFormat pixel_format) const;

  // The local video frame pool mojo service.
  mojo::AssociatedReceiver<mojom::VideoFramePool> video_frame_pool_receiver_;
  // The remote video frame pool mojo client.
  mojo::AssociatedRemote<mojom::VideoFramePoolClient> pool_client_;

  // The DMABuf video frame pool used as a backend.
  std::unique_ptr<media::VdaVideoFramePool> vda_video_frame_pool_;

  // callback used to notify the video frame pool of new video frame formats,
  // used when the pool requests new frames using RequestFrames().
  NotifyLayoutChangedCb notify_layout_changed_cb_;
  // Callback used to insert new frames in the video frame pool.
  ImportFrameCb import_frame_cb_;
  // Callback used to notify the video decoder when new frames are requested.
  base::RepeatingClosure request_frames_cb_;

  // The coded size currently used for video frames.
  gfx::Size coded_size_;
  // The current video frame layout.
  absl::optional<media::VideoFrameLayout> video_frame_layout_;

  // Map of video frame buffer ids to the associated video frame ids.
  std::map<gfx::GpuMemoryBufferId, int32_t> buffer_id_to_video_frame_id_;

  // The protected buffer manager, used when decoding an encrypted video.
  scoped_refptr<ProtectedBufferManager> protected_buffer_manager_;
  // Whether we're decoding an encrypted video.
  absl::optional<bool> secure_mode_;

  // The client task runner and its sequence checker. All methods should be run
  // on this task runner.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<GpuArcVideoFramePool> weak_this_;
  base::WeakPtrFactory<GpuArcVideoFramePool> weak_this_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_FRAME_POOL_H_
