// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/gpu_arc_video_decoder.h"

#include <utility>
#include <vector>

#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_frame_pool.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/macros.h"

namespace arc {

namespace {

// Heuristically chosen maximum number of concurrent decoder instances, as
// system resources are limited.
constexpr size_t kMaxConcurrentInstances = 8;

}  // namespace

// static
size_t GpuArcVideoDecoder::num_instances_ = 0;

GpuArcVideoDecoder::GpuArcVideoDecoder(
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager)
    : protected_buffer_manager_(std::move(protected_buffer_manager)) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

GpuArcVideoDecoder::~GpuArcVideoDecoder() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate all weak pointers to stop incoming callbacks.
  weak_this_factory_.InvalidateWeakPtrs();

  if (decoder_) {
    // Destroy |decoder_| now in case it needs to use *|this| during tear-down.
    decoder_.reset();

    // The number of active instances should always be larger than zero. But if
    // a bug causes an underflow we will permanently be unable to create new
    // decoders, so an extra check is performed here (see b/173700103).
    if (num_instances_ > 0) {
      num_instances_--;
    }
  }

  client_video_frames_.clear();
  video_frame_pool_.reset();
}

void GpuArcVideoDecoder::Initialize(
    arc::mojom::VideoDecoderConfigPtr config,
    mojo::PendingRemote<mojom::VideoDecoderClient> client,
    mojo::PendingAssociatedReceiver<mojom::VideoFramePool> video_frame_pool,
    InitializeCallback callback) {
  VLOGF(2) << "profile: " << config->profile;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!error_state_);
  DCHECK(!client_ && !init_callback_ && !video_frame_pool_);

  client_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  client_.Bind(std::move(client));
  init_callback_ = std::move(callback);
  video_frame_pool_ = std::make_unique<GpuArcVideoFramePool>(
      std::move(video_frame_pool),
      base::BindRepeating(&GpuArcVideoDecoder::ReleaseClientVideoFrames,
                          weak_this_),
      protected_buffer_manager_);

  if (decoder_) {
    VLOGF(1) << "Re-initialization is not allowed";
    OnInitializeDone(media::DecoderStatus::Codes::kFailed);
    return;
  }

  if (num_instances_ >= kMaxConcurrentInstances) {
    VLOGF(1) << "Maximum concurrent instances reached: " << num_instances_;
    OnInitializeDone(media::DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  decoder_ = media::VideoDecoderPipeline::Create(
      // TODO(b/238684141): Wire a meaningful GpuDriverBugWorkarounds or remove
      // its use.
      gpu::GpuDriverBugWorkarounds(), client_task_runner_,
      std::make_unique<media::VdaVideoFramePool>(video_frame_pool_->WeakThis(),
                                                 client_task_runner_),
      /*frame_converter=*/nullptr,
      media::VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
      std::make_unique<media::NullMediaLog>(),
      /*oop_video_decoder=*/{},
      // TODO(b/195769334): Set this to true once OOP-VD is enabled for ARC.
      /*in_video_decoder_process=*/false);

  if (!decoder_) {
    VLOGF(1) << "Failed to create video decoder";
    OnInitializeDone(media::DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }
  num_instances_++;

  gfx::Size coded_size(config->coded_size.width(), config->coded_size.height());
  media::VideoDecoderConfig vd_config(
      media::VideoCodecProfileToVideoCodec(config->profile), config->profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation, coded_size, gfx::Rect(coded_size), coded_size,
      std::vector<uint8_t>(), media::EncryptionScheme::kUnencrypted);
  auto init_cb =
      base::BindOnce(&GpuArcVideoDecoder::OnInitializeDone, weak_this_);
  auto output_cb =
      base::BindRepeating(&GpuArcVideoDecoder::OnFrameReady, weak_this_);

  // Decoded video frames are sent "quickly" (i.e. without much buffering)
  // to SurfaceFlinger, so we consider it a |low_delay| pipeline.
  decoder_->Initialize(std::move(vd_config), true /* low_delay */, nullptr,
                       std::move(init_cb), std::move(output_cb),
                       media::WaitingCB());
  VLOGF(2) << "Number of concurrent decoder instances: " << num_instances_;
}

void GpuArcVideoDecoder::Decode(arc::mojom::DecoderBufferPtr buffer,
                                DecodeCallback callback) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error_state_) {
    return;
  }

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }

  // Create an EOS buffer to flush the decoder if requested. All requests will
  // be handled before shutting down, so using unretained is safe here.
  if (buffer->is_end_of_stream()) {
    scoped_refptr<media::DecoderBuffer> eos_buffer =
        media::DecoderBuffer::CreateEOSBuffer();
    HandleRequest(base::BindOnce(&GpuArcVideoDecoder::HandleDecodeRequest,
                                 base::Unretained(this), std::move(eos_buffer),
                                 std::move(callback)));
    return;
  }

  // Get the buffer fd from the mojo decoder buffer.
  LOG_ASSERT(buffer->is_buffer());
  mojom::BufferPtr& buffer_ptr = buffer->get_buffer();
  base::ScopedFD fd = buffer_ptr->handle_fd.TakeFD();
  if (!fd.is_valid()) {
    OnError(media::DecoderStatus::Codes::kInvalidArgument);
    return;
  }
  DVLOGF(4) << "timestamp: " << buffer_ptr->timestamp << ", fd: " << fd.get();

  // If this is the first input buffer, determine if the playback is secure.
  if (!secure_mode_.has_value()) {
    if (protected_buffer_manager_) {
      secure_mode_ = IsBufferSecure(protected_buffer_manager_.get(), fd);
      VLOGF(2) << "First input buffer secure: " << *secure_mode_;
    } else {
      secure_mode_ = false;
      DVLOGF(3) << "No protected buffer manager, treating playback as normal";
    }
  }

  // Create a decoder buffer. The CreateDecoderBuffer() function performs
  // various checks to make sure the fd and provided parameters are valid.
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      CreateDecoderBuffer(std::move(fd), buffer_ptr->offset, buffer_ptr->size);
  if (!decoder_buffer) {
    VLOGF(1) << "Failed to create decoder buffer from fd";
    OnError(media::DecoderStatus::Codes::kInvalidArgument);
    return;
  }

  decoder_buffer->set_timestamp(
      base::TimeDelta(base::Milliseconds(buffer_ptr->timestamp)));

  // Using unretained is safe here, all callbacks are guaranteed to be executed
  // before the decoder is destroyed
  HandleRequest(base::BindOnce(&GpuArcVideoDecoder::HandleDecodeRequest,
                               base::Unretained(this),
                               std::move(decoder_buffer), std::move(callback)));
}

void GpuArcVideoDecoder::ReleaseVideoFrame(int32_t video_frame_id) {
  DVLOGF(4) << "id: " << video_frame_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }

  auto it = client_video_frames_.find(video_frame_id);
  if (it == client_video_frames_.end()) {
    // This might happen when new video frames were requested, but a release
    // video frame request was already scheduled. We can safely ignore it here.
    DVLOGF(1) << "Video frame with id " << video_frame_id
              << " has already been dismissed, ignoring";
    return;
  }

  // Reduce the video frame's use count. If the use count reaches zero we can
  // release our reference to the video frame, returning it to the pool.
  size_t& use_count = it->second.second;
  DCHECK_NE(use_count, 0u);
  if (--use_count == 0) {
    client_video_frames_.erase(it);
  }
}

void GpuArcVideoDecoder::Reset(ResetCallback callback) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Using unretained is safe here, all callbacks are guaranteed to be executed
  // before the decoder is destroyed
  HandleRequest(base::BindOnce(&GpuArcVideoDecoder::HandleResetRequest,
                               base::Unretained(this), std::move(callback)));
}

void GpuArcVideoDecoder::OnInitializeDone(media::DecoderStatus status) {
  DVLOGF(4) << "status: " << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(init_callback_).Run(status);
}

void GpuArcVideoDecoder::OnDecodeDone(DecodeCallback callback,
                                      media::DecoderStatus status) {
  DVLOGF(4) << "status: " << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok() && status != media::DecoderStatus::Codes::kAborted) {
    std::move(callback).Run(status.code());
    return;
  }

  std::move(callback).Run(media::DecoderStatus::Codes::kOk);
}

void GpuArcVideoDecoder::OnFrameReady(scoped_refptr<media::VideoFrame> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame);

  std::optional<int32_t> video_frame_id =
      video_frame_pool_->GetVideoFrameId(frame.get());
  if (!video_frame_id) {
    VLOGF(1) << "Failed to get video frame id.";
    OnError(media::DecoderStatus::Codes::kInvalidArgument);
    return;
  }

  gfx::Rect visible_rect = frame->visible_rect();
  int64_t timestamp = frame->timestamp().InMilliseconds();

  // Add frame to the list of video frames sent to the client so it won't be
  // returned to the pool while the client is using it. If the video frame is
  // already sent to the client (VP9 show_existing_frame feature), increase its
  // use count.
  auto frame_it = client_video_frames_.find(*video_frame_id);
  if (frame_it == client_video_frames_.end()) {
    client_video_frames_.emplace(*video_frame_id,
                                 std::make_pair(std::move(frame), 1));
  } else {
    frame_it->second.second++;
  }

  client_->OnVideoFrameDecoded(*video_frame_id, visible_rect, timestamp);
}

void GpuArcVideoDecoder::OnResetDone() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!reset_callback_) {
    VLOGF(1) << "Unexpected OnResetDone() callback received from VD";
    OnError(media::DecoderStatus::Codes::kInvalidArgument);
    return;
  }

  CHECK(video_frame_pool_);
  video_frame_pool_->OnDecoderResetDone();
  std::move(reset_callback_).Run();
  HandleRequests();
}

void GpuArcVideoDecoder::ReleaseClientVideoFrames() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_video_frames_.clear();
}

void GpuArcVideoDecoder::OnError(media::DecoderStatus status) {
  VLOGF(1) << "error: " << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error_state_) {
    return;
  }

  if (reset_callback_) {
    CHECK(video_frame_pool_);
    video_frame_pool_->OnDecoderResetDone();
    std::move(reset_callback_).Run();
  }

  error_state_ = true;
  if (client_) {
    client_->OnError(std::move(status));
  }

  // Abort all pending requests.
  HandleRequests();
}

void GpuArcVideoDecoder::HandleRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!reset_callback_ && !requests_.empty()) {
    HandleRequest(std::move(requests_.front()));
    requests_.pop();
  }
}

void GpuArcVideoDecoder::HandleRequest(Request request) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Postpone all requests if we are currently resetting the decoder. Note that
  // there is no need to postpone requests while flushing. Calling reset while
  // flushing is allowed, and multiple ongoing flush calls are also valid.
  if (reset_callback_) {
    requests_.emplace(std::move(request));
    return;
  }

  std::move(request).Run();
}

void GpuArcVideoDecoder::HandleDecodeRequest(
    scoped_refptr<media::DecoderBuffer> buffer,
    DecodeCallback callback) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error_state_) {
    std::move(callback).Run(media::DecoderStatus::Codes::kFailed);
    return;
  }
  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }

  decoder_->Decode(std::move(buffer),
                   base::BindOnce(&GpuArcVideoDecoder::OnDecodeDone, weak_this_,
                                  std::move(callback)));
}

void GpuArcVideoDecoder::HandleResetRequest(ResetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  if (error_state_) {
    return;
  }

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    OnError(media::DecoderStatus::Codes::kInvalidArgument);
    return;
  }

  // HandleResetRequest() doesn't run if there's an unfinished Reset(). See
  // HandleRequests().
  CHECK(!reset_callback_);
  CHECK(video_frame_pool_);
  video_frame_pool_->WillResetDecoder();

  reset_callback_ = std::move(std::move(callback));
  decoder_->Reset(base::BindOnce(&GpuArcVideoDecoder::OnResetDone, weak_this_));
}

scoped_refptr<media::DecoderBuffer> GpuArcVideoDecoder::CreateDecoderBuffer(
    base::ScopedFD fd,
    uint32_t offset,
    uint32_t bytes_used) {
  // TODO(b/189278506) integrate additional memory buffer verification for
  // protected buffers (see crrev.com/3306795).
  base::UnsafeSharedMemoryRegion shm_region;
  if (*secure_mode_) {
    // Use protected shared memory associated with the given file descriptor.
    shm_region = protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
        std::move(fd));
    if (!shm_region.IsValid()) {
      VLOGF(1) << "No protected shared memory found for handle";
      return nullptr;
    }
  } else {
    size_t size;
    if (!media::GetFileSize(fd.get(), &size)) {
      VLOGF(1) << "Failed to get size for fd";
      return nullptr;
    }
    shm_region = base::UnsafeSharedMemoryRegion::Deserialize(
        base::subtle::PlatformSharedMemoryRegion::Take(
            std::move(fd),
            base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, size,
            base::UnguessableToken::Create()));
    if (!shm_region.IsValid()) {
      VLOGF(1) << "Cannot take file descriptor based shared memory";
      return nullptr;
    }
  }

  // Create a decoder buffer from the shared memory region, will perform
  // validation of the provided parameters.
  return media::DecoderBuffer::FromSharedMemoryRegion(std::move(shm_region),
                                                      offset, bytes_used);
}

}  // namespace arc
