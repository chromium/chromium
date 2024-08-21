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
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace arc {

namespace {

// Helper thunk called when all references to a video frame have been dropped.
// The thunk reschedules the OnFrameReleased callback on the correct task runner
// as frames can be destroyed on any thread. Note that the WeakPtr is wrapped in
// an std::optional, as a WeakPtr should only be dereferenced on the thread it
// was created on. If we don't wrap the WeakPtr the task runner will dereference
// the WeakPtr before calling this function causing an assert.
void OnFrameReleasedThunk(
    std::optional<base::WeakPtr<GpuArcVideoFramePool>> weak_this,
    base::SequencedTaskRunner* task_runner,
    scoped_refptr<media::FrameResource> origin_frame) {
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
    base::RepeatingClosure release_client_video_frames_cb,
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager)
    : video_frame_pool_receiver_(this),
      release_client_video_frames_cb_(
          std::move(release_client_video_frames_cb)),
      protected_buffer_manager_(std::move(protected_buffer_manager)) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  video_frame_pool_receiver_.Bind(std::move(video_frame_pool));

  weak_this_ = weak_this_factory_.GetWeakPtr();

  video_frame_pool_receiver_.set_disconnect_handler(
      base::BindOnce(&GpuArcVideoFramePool::Stop, weak_this_));

  client_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
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

  if (has_error_) {
    return;
  }

  if (pool_client_) {
    DVLOGF(3) << "Attempting to call GpuArcVideoFramePool::Initialize() when "
                 "it is already initialized";
    return;
  }

  pool_client_version_ = client.version();

  pool_client_.Bind(std::move(client));

  pool_client_.set_disconnect_handler(
      base::BindOnce(&GpuArcVideoFramePool::Stop, weak_this_));
}

void GpuArcVideoFramePool::AddVideoFrame(mojom::VideoFramePtr video_frame,
                                         AddVideoFrameCallback callback) {
  DVLOGF(3) << "id: " << video_frame->id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_) {
    std::move(callback).Run(false);
    return;
  }

  if (!pool_client_version_) {
    DVLOGF(3) << "Unknown pool client version. Discarding video frame.";
    std::move(callback).Run(false);
    return;
  }

  if (!import_frame_cb_) {
    // This can happen if the remote end calls AddVideoFrame() before we call
    // RequestFrames() on it (a badly behaved remote end) or if due to
    // unfortunate timing, AddVideoFrame() is received after having interrupted
    // a request for frames because of a Reset(). In either case, we should
    // ignore the frame.
    //
    // Note that we call |callback| with true. This is to prevent
    // GpuVdContext::OnAddVideoFrameDone() on the libvda side from dispatching
    // an error. This is consistent with how the legacy VDA stack handles the
    // absence of an import callback: see
    // VdVideoDecodeAccelerator::ImportBufferForPicture() which simply returns
    // early without reporting an error to the client.
    DVLOGF(3) << "Can't import the frame because there's no import callback";
    std::move(callback).Run(true);
    return;
  }

  // Discard frame because ACK from the client for the last RequestVideoFrames()
  // has not been received yet.
  if (awaiting_request_frames_ack_) {
    CHECK_GE(pool_client_version_.value(), kMinVersionForRequestFramesAck);
    DVLOGF(3) << "ACK from client not received after calling "
                 "Client::RequestVideoFrames(). "
              << "Discarding video frame.";
    std::move(callback).Run(true);
    return;
  }

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
  // may need to notify the VdaVideoFramePool that the layout changed.
  if (notify_layout_changed_cb_) {
    const uint64_t layout_modifier =
        (gmb_handle.type == gfx::NATIVE_PIXMAP)
            ? gmb_handle.native_pixmap_handle.modifier
            : gfx::NativePixmapHandle::kNoModifier;
    std::vector<media::ColorPlaneLayout> color_planes;
    for (const auto& plane : gmb_handle.native_pixmap_handle.planes) {
      color_planes.emplace_back(plane.stride, plane.offset, plane.size);
    }
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

  scoped_refptr<media::FrameResource> origin_frame =
      CreateFrame(std::move(gmb_handle), pixel_format);

  if (!origin_frame) {
    VLOGF(1) << "Failed to create frame from fd";
    std::move(callback).Run(false);
    return;
  }

  // This passes because GetFrameStorageType() is hard coded to match
  // the storage type of frames produced by CreateFrame().
  CHECK_EQ(origin_frame->storage_type(), GetFrameStorageType());

  auto it = buffer_id_to_video_frame_id_.emplace(
      origin_frame->GetSharedMemoryId(), video_frame->id);
  DCHECK(it.second);

  // Wrap the video frame and attach a destruction observer so we're notified
  // when all references to the frame have been dropped.
  scoped_refptr<media::FrameResource> wrapped_frame =
      origin_frame->CreateWrappingFrame();

  wrapped_frame->AddDestructionObserver(base::BindOnce(
      &OnFrameReleasedThunk, weak_this_, base::RetainedRef(client_task_runner_),
      std::move(origin_frame)));

  // Add the frame to the underlying frame pool.
  import_frame_cb_.Run(std::move(wrapped_frame));

  std::move(callback).Run(true);
}

void GpuArcVideoFramePool::WillResetDecoder() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_) {
    return;
  }

  // GpuArcVideoDecoder should ensure WillResetDecoder() is not called while a
  // reset is in progress.
  CHECK(!decoder_is_resetting_);
  decoder_is_resetting_ = true;

  if (notify_layout_changed_cb_) {
    // If we're here, a Reset() has occurred in the the middle of a request for
    // frames. That means that the VdaVideoFramePool is blocked waiting for a
    // call to |notify_layout_changed_cb_|. Here, we unblock it by calling
    // |notify_layout_changed_cb_| with kResetRequired thus aborting the request
    // for frames.
    std::move(notify_layout_changed_cb_)
        .Run(media::CroStatus::Codes::kResetRequired);
    import_frame_cb_.Reset();
  }
}

void GpuArcVideoFramePool::OnDecoderResetDone() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_) {
    return;
  }

  decoder_is_resetting_ = false;
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

  // Notify the GpuArcVideoDecoder that we're no longer tracking frames
  // previously added to the pool.
  release_client_video_frames_cb_.Run();

  if (has_error_ || !pool_client_version_) {
    std::move(notify_layout_changed_cb)
        .Run(media::CroStatus::Codes::kFailedToGetFrameLayout);
    return;
  }

  notify_layout_changed_cb_ = std::move(notify_layout_changed_cb);
  import_frame_cb_ = std::move(import_frame_cb);

  if (decoder_is_resetting_) {
    // If we're here, it means that due to unfortunate timing, a Reset() request
    // was received before this RequestFrames(). The VdaVideoFramePool is
    // blocked waiting on a call to |notify_layout_changed_cb_|. Therefore, we
    // call it here with kResetRequired thus aborting the request for frames.
    std::move(notify_layout_changed_cb_)
        .Run(media::CroStatus::Codes::kResetRequired);
    import_frame_cb_.Reset();
    return;
  }

  pending_frame_requests_.push(
      base::BindOnce(&GpuArcVideoFramePool::HandleRequestFrames, weak_this_,
                     fourcc, coded_size, visible_rect, max_num_frames));
  CallPendingRequestFrames();
}

void GpuArcVideoFramePool::HandleRequestFrames(const media::Fourcc& fourcc,
                                               const gfx::Size& coded_size,
                                               const gfx::Rect& visible_rect,
                                               size_t max_num_frames) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Pending calls to HandleRequestFrames() are cleared when an error is
  // detected (see logic in Stop()) and new ones are not scheduled after that
  // (see logic in RequestFrames()).
  CHECK(!has_error_);

  // Calls to HandleRequestFrames() are scheduled only if the client version is
  // known (see the logic in RequestFrames()). If the |pool_client_version_|
  // becomes nullopt (see Stop()), pending calls are dropped.
  CHECK(pool_client_version_);

  // HandleRequestFrames() should only be called if we're not waiting for an ACK
  // for a previous request for frames.
  CHECK_GE(pool_client_version_.value(), kMinVersionForRequestFramesAck);
  CHECK(!awaiting_request_frames_ack_);
  awaiting_request_frames_ack_ = true;

  coded_size_ = coded_size;

  pool_client_->RequestVideoFrames(
      /*format=*/fourcc.ToVideoPixelFormat(), coded_size, visible_rect,
      max_num_frames,
      base::BindOnce(&GpuArcVideoFramePool::OnRequestVideoFramesDone,
                     weak_this_));
}

void GpuArcVideoFramePool::CallPendingRequestFrames() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!has_error_);

  if (awaiting_request_frames_ack_ || pending_frame_requests_.empty()) {
    // Either we're still waiting for an ACK for a previous call to
    // |pool_client_|->RequestVideoFrames() or there are no more RequestFrames()
    // to be submitted to the remote end.
    return;
  }

  std::move(pending_frame_requests_.front()).Run();
  pending_frame_requests_.pop();
}

void GpuArcVideoFramePool::OnRequestVideoFramesDone() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_) {
    return;
  }

  CHECK(pool_client_version_.has_value());
  CHECK_GE(pool_client_version_.value(), kMinVersionForRequestFramesAck);

  CHECK(awaiting_request_frames_ack_);
  awaiting_request_frames_ack_ = false;

  // Continue any queued requests for video frames that were made while waiting
  // for the current request for video frames to complete.
  CallPendingRequestFrames();
}

media::VideoFrame::StorageType GpuArcVideoFramePool::GetFrameStorageType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is validated at runtime to be in sync with the frame storage type.
  return media::VideoFrame::STORAGE_DMABUFS;
}

std::optional<int32_t> GpuArcVideoFramePool::GetVideoFrameId(
    const media::VideoFrame* video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(video_frame);

  if (has_error_) {
    return std::nullopt;
  }

  auto it =
      buffer_id_to_video_frame_id_.find(media::GetSharedMemoryId(*video_frame));
  return it != buffer_id_to_video_frame_id_.end()
             ? std::optional<int32_t>(it->second)
             : std::nullopt;
}

void GpuArcVideoFramePool::OnFrameReleased(
    scoped_refptr<media::FrameResource> origin_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_) {
    return;
  }

  auto it =
      buffer_id_to_video_frame_id_.find(origin_frame->GetSharedMemoryId());
  DCHECK(it != buffer_id_to_video_frame_id_.end());
  buffer_id_to_video_frame_id_.erase(it);
}

gfx::GpuMemoryBufferHandle GpuArcVideoFramePool::CreateGpuMemoryHandle(
    base::ScopedFD fd,
    const std::vector<VideoFramePlane>& planes,
    media::VideoPixelFormat pixel_format,
    uint64_t modifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!has_error_);

  // Check whether we need to use protected buffers.
  if (!secure_mode_.has_value()) {
    base::ScopedFD dup_fd(HANDLE_EINTR(dup(fd.get())));
    secure_mode_ = protected_buffer_manager_ &&
                   protected_buffer_manager_->IsProtectedNativePixmapHandle(
                       std::move(dup_fd));
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

scoped_refptr<media::FrameResource> GpuArcVideoFramePool::CreateFrame(
    gfx::GpuMemoryBufferHandle gmb_handle,
    media::VideoPixelFormat pixel_format) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!has_error_);

  auto buffer_format = media::VideoPixelFormatToGfxBufferFormat(pixel_format);
  CHECK(buffer_format);
  // Usage is SCANOUT_CPU_READ_WRITE because we may need to map the buffer in
  // order to use the LibYUVImageProcessorBackend.
  return media::NativePixmapFrameResource::Create(
      gfx::Rect(coded_size_), coded_size_, base::TimeDelta(),
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
          coded_size_, *buffer_format,
          std::move(gmb_handle.native_pixmap_handle)));
}

void GpuArcVideoFramePool::Stop() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_) {
    return;
  }

  has_error_ = true;

  weak_this_factory_.InvalidateWeakPtrs();

  pool_client_version_.reset();

  pending_frame_requests_ = {};

  if (notify_layout_changed_cb_) {
    std::move(notify_layout_changed_cb_)
        .Run(media::CroStatus::Codes::kFailedToGetFrameLayout);
  }
}

}  // namespace arc
