// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser_impl.h"

#include <tuple>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/services/media_gallery_util/public/cpp/local_media_data_source_factory.h"
#include "content/public/browser/android/gpu_video_accelerator_factories_provider.h"
#include "content/public/browser/media_service.h"
#include "media/base/overlay_info.h"
#include "media/base/video_thumbnail_decoder.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// The maximum duration to parse media file.
const base::TimeDelta kTimeOut = base::Seconds(8);

// Returns if the mime type is video or audio.
bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return base::StartsWith(mime_type, "audio/",
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(mime_type, "video/",
                          base::CompareCase::INSENSITIVE_ASCII);
}

void OnRequestOverlayInfo(bool decoder_requires_restart_for_overlay,
                          media::ProvideOverlayInfoCB overlay_info_cb) {
  // No android overlay associated with video thumbnail.
  if (overlay_info_cb)
    std::move(overlay_info_cb).Run(media::OverlayInfo());
}

int64_t GetFileSize(const base::FilePath& file_path) {
  int64_t size = 0;
  if (!base::GetFileSize(file_path, &size))
    return -1;
  return size;
}

}  // namespace

ThumbnailMediaParserImpl::ThumbnailMediaParserImpl(
    const std::string& mime_type,
    const base::FilePath& file_path)
    : mime_type_(mime_type),
      file_path_(file_path),
      file_task_runner_(
          base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()})),
      decode_done_(false) {}

ThumbnailMediaParserImpl::~ThumbnailMediaParserImpl() = default;

void ThumbnailMediaParserImpl::Start(ParseCompleteCB parse_complete_cb) {
  RecordMediaParserEvent(MediaParserEvent::kInitialize);
  parse_complete_cb_ = std::move(parse_complete_cb);
  timer_.Start(
      FROM_HERE, kTimeOut,
      base::BindOnce(&ThumbnailMediaParserImpl::OnError,
                     weak_factory_.GetWeakPtr(), MediaParserEvent::kTimeout));

  // Only process media mime types.
  if (!IsSupportedMediaMimeType(mime_type_)) {
    OnError(MediaParserEvent::kUnsupportedMimeType);
    return;
  }

  // Get the size of the file if needed.
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetFileSize, file_path_),
      base::BindOnce(&ThumbnailMediaParserImpl::OnReadFileSize,
                     weak_factory_.GetWeakPtr()));
}

void ThumbnailMediaParserImpl::OnReadFileSize(int64_t file_size) {
  if (file_size < 0) {
    OnError(MediaParserEvent::kReadFileError);
    return;
  }

  size_ = file_size;
  RetrieveMediaParser();
}

void ThumbnailMediaParserImpl::OnMediaParserCreated() {
  auto media_source_factory = std::make_unique<LocalMediaDataSourceFactory>(
      file_path_, file_task_runner_);
  mojo::PendingRemote<chrome::mojom::MediaDataSource> source;
  media_data_source_ = media_source_factory->CreateMediaDataSource(
      source.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&ThumbnailMediaParserImpl::OnMediaDataReady,
                          weak_factory_.GetWeakPtr()));

  RecordMediaMetadataEvent(MediaMetadataEvent::kMetadataStart);
  media_parser()->ParseMediaMetadata(
      mime_type_, size_, false /* get_attached_images */, std::move(source),
      base::BindOnce(&ThumbnailMediaParserImpl::OnMediaMetadataParsed,
                     weak_factory_.GetWeakPtr()));
}

void ThumbnailMediaParserImpl::OnConnectionError() {
  OnError(MediaParserEvent::kUtilityConnectionError);
}

void ThumbnailMediaParserImpl::OnMediaMetadataParsed(
    bool parse_success,
    chrome::mojom::MediaMetadataPtr metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  if (!parse_success) {
    RecordMediaMetadataEvent(MediaMetadataEvent::kMetadataFailed);
    OnError(MediaParserEvent::kMetadataFailed);
    return;
  }
  metadata_ = std::move(metadata);
  DCHECK(metadata_);
  RecordMediaMetadataEvent(MediaMetadataEvent::kMetadataComplete);

  // For audio file, we only need metadata and poster.
  if (base::StartsWith(mime_type_, "audio/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    NotifyComplete(SkBitmap());
    return;
  }

  DCHECK(base::StartsWith(mime_type_, "video/",
                          base::CompareCase::INSENSITIVE_ASCII));

  // Start to retrieve video thumbnail.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailMediaParserImpl::RetrieveEncodedVideoFrame,
                     weak_factory_.GetWeakPtr()));
}

void ThumbnailMediaParserImpl::RetrieveEncodedVideoFrame() {
  RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoThumbnailStart);
  media_data_source_.reset();

  auto media_source_factory = std::make_unique<LocalMediaDataSourceFactory>(
      file_path_, file_task_runner_);
  mojo::PendingRemote<chrome::mojom::MediaDataSource> source;
  media_data_source_ = media_source_factory->CreateMediaDataSource(
      source.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&ThumbnailMediaParserImpl::OnMediaDataReady,
                          weak_factory_.GetWeakPtr()));

  media_parser()->ExtractVideoFrame(
      mime_type_, base::saturated_cast<uint32_t>(size_), std::move(source),
      base::BindOnce(&ThumbnailMediaParserImpl::OnVideoFrameRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void ThumbnailMediaParserImpl::OnVideoFrameRetrieved(
    chrome::mojom::ExtractVideoFrameResultPtr result) {
  if (!result) {
    RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoFrameExtractionFailed);
    OnError(MediaParserEvent::kVideoThumbnailFailed);
    return;
  }

  video_frame_data_ = std::move(result->frame_data);
  config_ = result->config;

  // For vp8, vp9 codec, we directly do software decoding in utility process.
  // Render now.
  if (video_frame_data_->which() ==
      chrome::mojom::VideoFrameData::Tag::kDecodedFrame) {
    decode_done_ = true;
    RenderVideoFrame(std::move(video_frame_data_->get_decoded_frame()));
    return;
  }

  // For other codec, the encoded frame is retrieved in utility process, send
  // the data to GPU process to do hardware decoding.
  if (video_frame_data_->get_encoded_data().empty()) {
    RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoFrameExtractionFailed);
    OnError(MediaParserEvent::kVideoThumbnailFailed);
    return;
  }

  // Starts to decode with MojoVideoDecoder.
  content::CreateGpuVideoAcceleratorFactories(base::BindRepeating(
      &ThumbnailMediaParserImpl::OnGpuVideoAcceleratorFactoriesReady,
      weak_factory_.GetWeakPtr()));
}

void ThumbnailMediaParserImpl::OnGpuVideoAcceleratorFactoriesReady(
    std::unique_ptr<media::GpuVideoAcceleratorFactories> factories) {
  gpu_factories_ = std::move(factories);
  DecodeVideoFrame();
}

void ThumbnailMediaParserImpl::DecodeVideoFrame() {
  mojo::PendingRemote<media::mojom::VideoDecoder> video_decoder_remote;

  // Out-of-process video decoding is not intended for Android, so we don't
  // provide a valid |dst_video_decoder|.
  GetMediaInterfaceFactory()->CreateVideoDecoder(
      video_decoder_remote.InitWithNewPipeAndPassReceiver(),
      /*dst_video_decoder=*/{});

  // Build and config the decoder.
  DCHECK(gpu_factories_);
  auto mojo_decoder = std::make_unique<media::MojoVideoDecoder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(), gpu_factories_.get(),
      this, std::move(video_decoder_remote),
      base::BindRepeating(&OnRequestOverlayInfo), gfx::ColorSpace());

  decoder_ = std::make_unique<media::VideoThumbnailDecoder>(
      std::move(mojo_decoder), config_,
      std::move(video_frame_data_->get_encoded_data()));

  decoder_->Start(base::BindOnce(&ThumbnailMediaParserImpl::OnVideoFrameDecoded,
                                 weak_factory_.GetWeakPtr()));
  video_frame_data_.reset();
}

void ThumbnailMediaParserImpl::OnVideoFrameDecoded(
    scoped_refptr<media::VideoFrame> frame) {
  if (!frame) {
    RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoDecodeFailed);
    OnError(MediaParserEvent::kVideoThumbnailFailed);
    return;
  }

  DCHECK(frame->HasSharedImage());
  decode_done_ = true;

  RenderVideoFrame(std::move(frame));
}

void ThumbnailMediaParserImpl::RenderVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  auto* context_provider =
      gpu_factories_ ? gpu_factories_->GetMediaContextProvider() : nullptr;

  media::PaintCanvasVideoRenderer renderer;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(video_frame->visible_rect().width(),
                        video_frame->visible_rect().height());

  // Draw the video frame to |bitmap|.
  cc::SkiaPaintCanvas canvas(bitmap);
  renderer.Copy(video_frame, &canvas, context_provider);

  RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoThumbnailComplete);
  NotifyComplete(std::move(bitmap));
}

media::mojom::InterfaceFactory*
ThumbnailMediaParserImpl::GetMediaInterfaceFactory() {
  if (!media_interface_factory_) {
    // No need to provide any remote services to the media service because the
    // ThumbnailMediaParser does not use them, but the Mojo argument is
    // currently marked as required so pass a remote but drop the other end.
    mojo::PendingRemote<media::mojom::FrameInterfaceFactory> interfaces;
    std::ignore = interfaces.InitWithNewPipeAndPassReceiver();
    content::GetMediaService().CreateInterfaceFactory(
        media_interface_factory_.BindNewPipeAndPassReceiver(),
        std::move(interfaces));
    media_interface_factory_.set_disconnect_handler(
        base::BindOnce(&ThumbnailMediaParserImpl::OnDecoderConnectionError,
                       base::Unretained(this)));
  }

  return media_interface_factory_.get();
}

void ThumbnailMediaParserImpl::OnDecoderConnectionError() {
  OnError(MediaParserEvent::kGpuConnectionError);
}

void ThumbnailMediaParserImpl::OnMediaDataReady(
    chrome::mojom::MediaDataSource::ReadCallback callback,
    std::string data) {
  // TODO(xingliu): Change media_parser.mojom to move the data instead of copy.
  if (media_parser()) {
    std::move(callback).Run(std::vector<uint8_t>(data.begin(), data.end()));
  }
}

void ThumbnailMediaParserImpl::NotifyComplete(SkBitmap bitmap) {
  DCHECK(metadata_);
  DCHECK(parse_complete_cb_);
  RecordMediaParserEvent(MediaParserEvent::kSuccess);
  if (parse_complete_cb_) {
    std::move(parse_complete_cb_)
        .Run(true, std::move(metadata_), std::move(bitmap));
  }
}

void ThumbnailMediaParserImpl::OnError(MediaParserEvent event) {
  DCHECK(parse_complete_cb_);
  RecordMediaParserEvent(MediaParserEvent::kFailure);
  RecordMediaParserEvent(event);
  if (parse_complete_cb_) {
    std::move(parse_complete_cb_)
        .Run(false, chrome::mojom::MediaMetadata::New(), SkBitmap());
  }
}
