// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_media_parser.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/download/android/local_media_data_source_factory.h"
#include "content/public/browser/android/gpu_video_accelerator_factories_provider.h"
#include "content/public/browser/system_connector.h"
#include "media/base/overlay_info.h"
#include "media/base/video_thumbnail_decoder.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/constants.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// The maximum duration to parse media file.
const base::TimeDelta kTimeOut = base::TimeDelta::FromSeconds(8);

// Returns if the mime type is video or audio.
bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return base::StartsWith(mime_type, "audio/",
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(mime_type, "video/",
                          base::CompareCase::INSENSITIVE_ASCII);
}

void OnRequestOverlayInfo(bool decoder_requires_restart_for_overlay,
                          const media::ProvideOverlayInfoCB& overlay_info_cb) {
  // No android overlay associated with video thumbnail.
  if (overlay_info_cb)
    overlay_info_cb.Run(media::OverlayInfo());
}

int64_t GetFileSize(const base::FilePath& file_path) {
  int64_t size = 0;
  if (!base::GetFileSize(file_path, &size))
    return -1;
  return size;
}

}  // namespace

DownloadMediaParser::DownloadMediaParser(const std::string& mime_type,
                                         const base::FilePath& file_path)
    : mime_type_(mime_type),
      file_path_(file_path),
      file_task_runner_(base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::MayBlock()})),
      decode_done_(false) {}

DownloadMediaParser::~DownloadMediaParser() = default;

void DownloadMediaParser::Start(ParseCompleteCB parse_complete_cb) {
  RecordMediaParserEvent(MediaParserEvent::kInitialize);
  parse_complete_cb_ = std::move(parse_complete_cb);
  timer_.Start(
      FROM_HERE, kTimeOut,
      base::BindOnce(&DownloadMediaParser::OnError, weak_factory_.GetWeakPtr(),
                     MediaParserEvent::kTimeout));

  // Only process media mime types.
  if (!IsSupportedMediaMimeType(mime_type_)) {
    OnError(MediaParserEvent::kUnsupportedMimeType);
    return;
  }

  // Get the size of the file if needed.
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&GetFileSize, file_path_),
      base::BindOnce(&DownloadMediaParser::OnReadFileSize,
                     weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnReadFileSize(int64_t file_size) {
  if (file_size < 0) {
    OnError(MediaParserEvent::kReadFileError);
    return;
  }

  size_ = file_size;
  RetrieveMediaParser();
}

void DownloadMediaParser::OnMediaParserCreated() {
  auto media_source_factory = std::make_unique<LocalMediaDataSourceFactory>(
      file_path_, file_task_runner_);
  mojo::PendingRemote<chrome::mojom::MediaDataSource> source;
  media_data_source_ = media_source_factory->CreateMediaDataSource(
      source.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&DownloadMediaParser::OnMediaDataReady,
                          weak_factory_.GetWeakPtr()));

  RecordMediaMetadataEvent(MediaMetadataEvent::kMetadataStart);
  media_parser()->ParseMediaMetadata(
      mime_type_, size_, false /* get_attached_images */, std::move(source),
      base::BindOnce(&DownloadMediaParser::OnMediaMetadataParsed,
                     weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnConnectionError() {
  OnError(MediaParserEvent::kUtilityConnectionError);
}

void DownloadMediaParser::OnMediaMetadataParsed(
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadMediaParser::RetrieveEncodedVideoFrame,
                                weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::RetrieveEncodedVideoFrame() {
  RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoThumbnailStart);
  media_data_source_.reset();

  auto media_source_factory = std::make_unique<LocalMediaDataSourceFactory>(
      file_path_, file_task_runner_);
  mojo::PendingRemote<chrome::mojom::MediaDataSource> source;
  media_data_source_ = media_source_factory->CreateMediaDataSource(
      source.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&DownloadMediaParser::OnMediaDataReady,
                          weak_factory_.GetWeakPtr()));

  media_parser()->ExtractVideoFrame(
      mime_type_, base::saturated_cast<uint32_t>(size_), std::move(source),
      base::BindOnce(&DownloadMediaParser::OnVideoFrameRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnVideoFrameRetrieved(
    bool success,
    chrome::mojom::VideoFrameDataPtr video_frame_data,
    const base::Optional<media::VideoDecoderConfig>& config) {
  if (!success) {
    RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoFrameExtractionFailed);
    OnError(MediaParserEvent::kVideoThumbnailFailed);
    return;
  }

  video_frame_data_ = std::move(video_frame_data);
  DCHECK(config.has_value());
  config_ = config.value();

  // For vp8, vp9 codec, we directly do software decoding in utility process.
  // Render now.
  if (video_frame_data_->which() ==
      chrome::mojom::VideoFrameData::Tag::DECODED_FRAME) {
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
      &DownloadMediaParser::OnGpuVideoAcceleratorFactoriesReady,
      weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnGpuVideoAcceleratorFactoriesReady(
    std::unique_ptr<media::GpuVideoAcceleratorFactories> factories) {
  gpu_factories_ = std::move(factories);
  DecodeVideoFrame();
}

void DownloadMediaParser::DecodeVideoFrame() {
  mojo::PendingRemote<media::mojom::VideoDecoder> video_decoder_remote;
  GetMediaInterfaceFactory()->CreateVideoDecoder(
      video_decoder_remote.InitWithNewPipeAndPassReceiver());

  // Build and config the decoder.
  DCHECK(gpu_factories_);
  auto mojo_decoder = std::make_unique<media::MojoVideoDecoder>(
      base::ThreadTaskRunnerHandle::Get(), gpu_factories_.get(), this,
      std::move(video_decoder_remote),
      media::VideoDecoderImplementation::kDefault,
      base::BindRepeating(&OnRequestOverlayInfo), gfx::ColorSpace());

  decoder_ = std::make_unique<media::VideoThumbnailDecoder>(
      std::move(mojo_decoder), config_,
      std::move(video_frame_data_->get_encoded_data()));

  decoder_->Start(base::BindOnce(&DownloadMediaParser::OnVideoFrameDecoded,
                                 weak_factory_.GetWeakPtr()));
  video_frame_data_.reset();
}

void DownloadMediaParser::OnVideoFrameDecoded(
    scoped_refptr<media::VideoFrame> frame) {
  if (!frame) {
    RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoDecodeFailed);
    OnError(MediaParserEvent::kVideoThumbnailFailed);
    return;
  }

  DCHECK(frame->HasTextures());
  decode_done_ = true;

  RenderVideoFrame(std::move(frame));
}

void DownloadMediaParser::RenderVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  auto context_provider =
      gpu_factories_ ? gpu_factories_->GetMediaContextProvider() : nullptr;

  media::PaintCanvasVideoRenderer renderer;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(video_frame->visible_rect().width(),
                        video_frame->visible_rect().height());

  // Draw the video frame to |bitmap|.
  cc::SkiaPaintCanvas canvas(bitmap);
  renderer.Copy(video_frame, &canvas, context_provider.get());

  RecordVideoThumbnailEvent(VideoThumbnailEvent::kVideoThumbnailComplete);
  NotifyComplete(std::move(bitmap));
}

media::mojom::InterfaceFactory*
DownloadMediaParser::GetMediaInterfaceFactory() {
  if (!media_interface_factory_) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> interfaces;
    media_interface_provider_ = std::make_unique<media::MediaInterfaceProvider>(
        interfaces.InitWithNewPipeAndPassReceiver());
    media::mojom::MediaServicePtr media_service;
    content::GetSystemConnector()->BindInterface(
        media::mojom::kMediaServiceName, &media_service);
    media_service->CreateInterfaceFactory(
        media_interface_factory_.BindNewPipeAndPassReceiver(),
        std::move(interfaces));
    media_interface_factory_.set_disconnect_handler(
        base::BindOnce(&DownloadMediaParser::OnDecoderConnectionError,
                       base::Unretained(this)));
  }

  return media_interface_factory_.get();
}

void DownloadMediaParser::OnDecoderConnectionError() {
  OnError(MediaParserEvent::kGpuConnectionError);
}

void DownloadMediaParser::OnMediaDataReady(
    chrome::mojom::MediaDataSource::ReadCallback callback,
    std::unique_ptr<std::string> data) {
  // TODO(xingliu): Change media_parser.mojom to move the data instead of copy.
  if (media_parser())
    std::move(callback).Run(std::vector<uint8_t>(data->begin(), data->end()));
}

void DownloadMediaParser::NotifyComplete(SkBitmap bitmap) {
  DCHECK(metadata_);
  DCHECK(parse_complete_cb_);
  RecordMediaParserEvent(MediaParserEvent::kSuccess);
  std::move(parse_complete_cb_)
      .Run(true, std::move(metadata_), std::move(bitmap));
}

void DownloadMediaParser::OnError(MediaParserEvent event) {
  DCHECK(parse_complete_cb_);
  RecordMediaParserEvent(MediaParserEvent::kFailure);
  RecordMediaParserEvent(event);
  std::move(parse_complete_cb_)
      .Run(false, chrome::mojom::MediaMetadata::New(), SkBitmap());
}
