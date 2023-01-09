// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/gpu_arc_video_frame_pool.h"

#include <utility>

#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "base/functional/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/decoder_status.h"
#include "media/base/format_utils.h"
#include "media/base/video_types.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/macros.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace arc {

namespace {

// Helper thunk called when all references to a video frame have been dropped.
// The thunk reschedules the OnFrameReleased callback on the correct task runner
// as frames can be destroyed on any thread. Note that the WeakPtr is wrapped in
// an absl::optional, as a WeakPtr should only be dereferenced on the thread it
// was created on. If we don't wrap the WeakPtr the task runner will dereference
// the WeakPtr before calling this function causing an assert.
void OnFrameReleasedThunk(
    absl::optional<base::WeakPtr<GpuArcVideoFramePool>> weak_this,
    base::SequencedTaskRunner* task_runner,
    scoped_refptr<media::VideoFrame> origin_frame) {
  DCHECK(weak_this);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&GpuArcVideoFramePool::OnFrameReleased,
                                       *weak_this, std::move(origin_frame)));
}

// Get the video pixel format associated with the specified mojo pixel |format|.
media::VideoPixelFormat GetPixelFormat(mojom::HalPixelFormat format) {
  switch (format) {
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_NV12:
      return media::PIXEL_FORMAT_NV12;
    default:
      return media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN;
  }
}

}  // namespace

GpuArcVideoFramePool::GpuArcVideoFramePool(
    mojo::PendingAssociatedReceiver<mojom::VideoFramePool> video_frame_pool,
    base::RepeatingClosure request_frames_cb,
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager)
    : video_frame_pool_receiver_(this),
      request_frames_cb_(std::move(request_frames_cb)),
      protected_buffer_manager_(std::move(protected_buffer_manager)) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  video_frame_pool_receiver_.Bind(std::move(video_frame_pool));

  weak_this_ = weak_this_factory_.GetWeakPtr();

  client_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  vda_video_frame_pool_ = std::make_unique<media::VdaVideoFramePool>(
      weak_this_, client_task_runner_);
}

GpuArcVideoFramePool::~GpuArcVideoFramePool() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate all weak pointers to stop incoming callbacks.
  weak_this_factory_.InvalidateWeakPtrs();

  // The VdaVideoFramePool is blocked until this callback is executed, so we
  // need to make sure to call it before destroying.
  if (notify_layout_changed_cb_) {
    std::move(notify_layout_changed_cb_)
        .Run(media::CroStatus::Codes::kFailedToGetFrameLayout);
  }
}

void GpuArcVideoFramePool::Initialize(
    mojo::PendingAssociatedRemote<mojom::VideoFramePoolClient> client) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pool_client_);

  pool_client_.Bind(std::move(client));
}

void GpuArcVideoFramePool::AddVideoFrame(mojom::VideoFramePtr video_frame,
                                         AddVideoFrameCallback callback) {
  DVLOGF(3) << "id: " << video_frame->id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Frames with the old coded size can still be added after resolution changes.
  if (video_frame->coded_size != coded_size_) {
    DVLOGF(3) << "Discarding video frame with old coded size";
    std::move(callback).Run(true);
    return;
  }

  media::VideoPixelFormat pixel_format = GetPixelFormat(video_frame->format);
  if (pixel_format == media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << "Unsupported format: " << video_frame->format;
    std::move(callback).Run(false);
    return;
  }

  // Convert the Mojo buffer fd to a GpuMemoryBufferHandle.
  base::ScopedFD fd = video_frame->handle_fd.TakeFD();
  if (!fd.is_valid()) {
    VLOGF(1) << "Invalid buffer handle";
    std::move(callback).Run(false);
    return;
  }
  gfx::GpuMemoryBufferHandle gmb_handle = CreateGpuMemoryHandle(
      std::move(fd), video_frame->planes, pixel_format, video_frame->modifier);
  if (gmb_handle.is_null()) {
    VLOGF(1) << "Failed to create GPU memory handle from fd";
    std::move(callback).Run(false);
    return;
  }

  // If this is the first video frame added after requesting new video frames we
  // need to update the video frame layout.
  if (notify_layout_changed_cb_) {
    const uint64_t layout_modifier =
        (gmb_handle.type == gfx::NATIVE_PIXMAP)
            ? gmb_handle.native_pixmap_handle.modifier
            : gfx::NativePixmapHandle::kNoModifier;
    std::vector<media::ColorPlaneLayout> color_planes;
    for (const auto& plane : gmb_handle.native_pixmap_handle.planes) {
      color_planes.emplace_back(plane.stride, plane.offset, plane.size);
    }
    video_frame_layout_ = media::VideoFrameLayout::CreateWithPlanes(
        pixel_format, coded_size_, color_planes,
        media::VideoFrameLayout::kBufferAddressAlignment, layout_modifier);
    if (!video_frame_layout_) {
      VLOGF(1) << "Failed to create VideoFrameLayout";
      std::move(callback).Run(false);
      return;
    }

    // Notify the VdaVideoFramePool that the layout changed.
    auto fourcc = media::Fourcc::FromVideoPixelFormat(pixel_format);
    if (!fourcc) {
      VLOGF(1) << "Failed to convert to fourcc";
      std::move(callback).Run(false);
      return;
    }

    auto gb_layout = media::GpuBufferLayout::Create(
        *fourcc, coded_size_, color_planes, layout_modifier);
    if (!gb_layout) {
      VLOGF(1) << "Failed to create GpuBufferLayout";
      std::move(notify_layout_changed_cb_)
          .Run(media::CroStatus::Codes::kFailedToChangeResolution);
      std::move(callback).Run(false);
      return;
    }
    std::move(notify_layout_changed_cb_).Run(*gb_layout);
  }

  scoped_refptr<media::VideoFrame> origin_frame =
      CreateVideoFrame(std::move(gmb_handle), pixel_format);
  if (!origin_frame) {
    VLOGF(1) << "Failed to create video frame from fd";
    std::move(callback).Run(false);
    return;
  }

  const gfx::GpuMemoryBufferId buffer_id =
      origin_frame->GetGpuMemoryBuffer()->GetId();
  auto it = buffer_id_to_video_frame_id_.emplace(buffer_id, video_frame->id);
  DCHECK(it.second);

  // Wrap the video frame and attach a destruction observer so we're notified
  // when all references to the video frame have been dropped.
  scoped_refptr<media::VideoFrame> wrapped_frame =
      media::VideoFrame::WrapVideoFrame(origin_frame, origin_frame->format(),
                                        origin_frame->visible_rect(),
                                        origin_frame->natural_size());
  wrapped_frame->AddDestructionObserver(base::BindOnce(
      &OnFrameReleasedThunk, weak_this_, base::RetainedRef(client_task_runner_),
      std::move(origin_frame)));

  // Add the frame to the underlying video frame pool.
  DCHECK(import_frame_cb_);
  import_frame_cb_.Run(std::move(wrapped_frame));

  std::move(callback).Run(true);
}

void GpuArcVideoFramePool::RequestFrames(
    const media::Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    size_t max_num_frames,
    NotifyLayoutChangedCb notify_layout_changed_cb,
    ImportFrameCb import_frame_cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!notify_layout_changed_cb_);

  coded_size_ = coded_size;

  notify_layout_changed_cb_ = std::move(notify_layout_changed_cb);
  import_frame_cb_ = std::move(import_frame_cb);

  // Send a request for new video frames to our mojo client.
  media::VideoPixelFormat format = fourcc.ToVideoPixelFormat();
  pool_client_->RequestVideoFrames(format, coded_size, visible_rect,
                                   max_num_frames);

  // Let the owner of the video frame pool know new frames were requested.
  request_frames_cb_.Run();
}

absl::optional<int32_t> GpuArcVideoFramePool::GetVideoFrameId(
    const media::VideoFrame* video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = buffer_id_to_video_frame_id_.find(
      video_frame->GetGpuMemoryBuffer()->GetId());
  return it != buffer_id_to_video_frame_id_.end()
             ? absl::optional<int32_t>(it->second)
             : absl::nullopt;
}

void GpuArcVideoFramePool::OnFrameReleased(
    scoped_refptr<media::VideoFrame> origin_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = buffer_id_to_video_frame_id_.find(
      origin_frame->GetGpuMemoryBuffer()->GetId());
  DCHECK(it != buffer_id_to_video_frame_id_.end());
  buffer_id_to_video_frame_id_.erase(it);
}

gfx::GpuMemoryBufferHandle GpuArcVideoFramePool::CreateGpuMemoryHandle(
    base::ScopedFD fd,
    const std::vector<VideoFramePlane>& planes,
    media::VideoPixelFormat pixel_format,
    uint64_t modifier) {
  // Check whether we need to use protected buffers.
  if (!secure_mode_.has_value()) {
    secure_mode_ = protected_buffer_manager_ &&
                   IsBufferSecure(protected_buffer_manager_.get(), fd);
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  if (*secure_mode_) {
    // Get the protected buffer associated with the |fd|.
    gfx::NativePixmapHandle protected_native_pixmap =
        protected_buffer_manager_->GetProtectedNativePixmapHandleFor(
            std::move(fd));
    if (protected_native_pixmap.planes.size() == 0) {
      VLOGF(1) << "No protected native pixmap found for handle";
      return gfx::GpuMemoryBufferHandle();
    }
    gmb_handle.type = gfx::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle = std::move(protected_native_pixmap);

    // Explicitly verify the GPU Memory Buffer Handle here. Note that we do not
    // do this for non-protected content because the verification happens on
    // creation in that path.
    if (!media::VerifyGpuMemoryBufferHandle(pixel_format, coded_size_,
                                            gmb_handle)) {
      VLOGF(1) << "Invalid GpuMemoryBufferHandle for protected content";
      return gfx::GpuMemoryBufferHandle();
    }
  } else {
    std::vector<base::ScopedFD> fds = DuplicateFD(std::move(fd), planes.size());
    if (fds.empty()) {
      VLOGF(1) << "Failed to duplicate fd";
      return gfx::GpuMemoryBufferHandle();
    }

    // Verification of the GPU Memory Buffer Handle is handled under the hood in
    // this call.
    auto handle = CreateGpuMemoryBufferHandle(
        pixel_format, modifier, coded_size_, std::move(fds), planes);
    if (!handle) {
      VLOGF(1) << "Failed to create GpuMemoryBufferHandle";
      return gfx::GpuMemoryBufferHandle();
    }
    gmb_handle = std::move(handle).value();
  }
  gmb_handle.id = media::GetNextGpuMemoryBufferId();

  return gmb_handle;
}

scoped_refptr<media::VideoFrame> GpuArcVideoFramePool::CreateVideoFrame(
    gfx::GpuMemoryBufferHandle gmb_handle,
    media::VideoPixelFormat pixel_format) const {
  auto buffer_format = media::VideoPixelFormatToGfxBufferFormat(pixel_format);
  CHECK(buffer_format);
  // Usage is SCANOUT_VDA_WRITE because we are just wrapping the dmabuf in a
  // GpuMemoryBuffer. This buffer is just for decoding purposes, so having
  // the dmabufs mmapped is not necessary.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu::GpuMemoryBufferSupport().CreateGpuMemoryBufferImplFromHandle(
          std::move(gmb_handle), coded_size_, *buffer_format,
          gfx::BufferUsage::SCANOUT_VDA_WRITE, base::NullCallback());
  if (!gpu_memory_buffer) {
    VLOGF(1) << "Failed to create GpuMemoryBuffer. format: "
             << gfx::BufferFormatToString(*buffer_format)
             << ", coded_size: " << coded_size_.ToString();
    return nullptr;
  }

  const gpu::MailboxHolder mailbox_holder[media::VideoFrame::kMaxPlanes] = {};
  return media::VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(coded_size_), coded_size_, std::move(gpu_memory_buffer),
      mailbox_holder, base::NullCallback(), base::TimeDelta());
}

}  // namespace arc
