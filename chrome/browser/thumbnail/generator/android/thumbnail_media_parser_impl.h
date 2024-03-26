// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_THUMBNAIL_MEDIA_PARSER_IMPL_H_
#define CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_THUMBNAIL_MEDIA_PARSER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/thumbnail/generator/android/stats.h"
#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "media/base/media_log.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class GpuVideoAcceleratorFactories;
class MojoVideoDecoder;
class VideoDecoderConfig;
class VideoThumbnailDecoder;
}  // namespace media

class SkBitmap;

// Parse media file in remote process using ffmpeg to extract video frame.
// The decoding may happen in utility or GPU process based on video codec.
// For video file, the thumbnail will be the a video key frame.
class ThumbnailMediaParserImpl : public ThumbnailMediaParser,
                                 public MediaParserProvider,
                                 public media::MediaLog {
 public:
  ThumbnailMediaParserImpl(const std::string& mime_type,
                           const base::FilePath& file_path);

  ThumbnailMediaParserImpl(const ThumbnailMediaParserImpl&) = delete;
  ThumbnailMediaParserImpl& operator=(const ThumbnailMediaParserImpl&) = delete;

  ~ThumbnailMediaParserImpl() override;

  // ThumbnailMediaParser implementation.
  void Start(ParseCompleteCB parse_complete_cb) override;

 private:
  void OnReadFileSize(int64_t file_size);

  // MediaParserProvider implementation:
  void OnMediaParserCreated() override;
  void OnConnectionError() override;

  // Called after media metadata are parsed.
  void OnMediaMetadataParsed(
      bool parse_success,
      chrome::mojom::MediaMetadataPtr metadata,
      const std::vector<metadata::AttachedImage>& attached_images);

  // Retrieves an encoded video frame.
  void RetrieveEncodedVideoFrame();
  void OnVideoFrameRetrieved(chrome::mojom::ExtractVideoFrameResultPtr result);

  // Decodes the video frame.
  void OnGpuVideoAcceleratorFactoriesReady(
      std::unique_ptr<media::GpuVideoAcceleratorFactories>);
  void DecodeVideoFrame();
  void OnVideoFrameDecoded(scoped_refptr<media::VideoFrame> decoded_frame);

  // Renders the video frame to bitmap.
  void RenderVideoFrame(scoped_refptr<media::VideoFrame> video_frame);

  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void OnDecoderConnectionError();

  // Overlays media data source read operation. Gradually read data from media
  // file.
  void OnMediaDataReady(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::string data);

  void NotifyComplete(SkBitmap bitmap);
  void OnError(MediaParserEvent event);

  int64_t size_;
  std::string mime_type_;
  base::FilePath file_path_;

  ParseCompleteCB parse_complete_cb_;
  chrome::mojom::MediaMetadataPtr metadata_;

  // Used to read media files chunks to feed to IPC channel.
  std::unique_ptr<chrome::mojom::MediaDataSource> media_data_source_;

  // The task runner to do blocking disk IO.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // A timer to prevent unresponsive Android decoder during video file parsing,
  // and the parser will fail gracefully after timeout.
  base::OneShotTimer timer_;

  // Cached video frame data, which contains either encoded frame or decoded
  // video frame. Encoded frame is extracted with ffmpeg, the data can be large
  // for high resolution video.
  chrome::mojom::VideoFrameDataPtr video_frame_data_;

  // Objects used to decode the video into media::VideoFrame with
  // MojoVideoDecoder.
  media::VideoDecoderConfig config_;
  // `gpu_factories_` must outlive `decoder_`.
  std::unique_ptr<media::GpuVideoAcceleratorFactories> gpu_factories_;
  std::unique_ptr<media::VideoThumbnailDecoder> decoder_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;
  bool decode_done_;

  base::WeakPtrFactory<ThumbnailMediaParserImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_THUMBNAIL_MEDIA_PARSER_IMPL_H_
