// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MEDIA_PARSER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MEDIA_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "media/base/media_log.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class GpuVideoAcceleratorFactories;
class MediaInterfaceProvider;
class MojoVideoDecoder;
class VideoDecoderConfig;
class VideoThumbnailDecoder;
}  // namespace media

class SkBitmap;

// Parse local media files, including media metadata and thumbnails.
// Metadata is always parsed in utility process for both audio and video files.
//
// For video file, the thumbnail will be the a video key frame. The frame
// extraction always happens in utility process. The decoding may happen in
// utility or GPU process based on video codec.
class DownloadMediaParser : public MediaParserProvider, public media::MediaLog {
 public:
  using ParseCompleteCB =
      base::OnceCallback<void(bool success,
                              chrome::mojom::MediaMetadataPtr media_metadata,
                              SkBitmap bitmap)>;

  DownloadMediaParser(const std::string& mime_type,
                      const base::FilePath& file_path);
  ~DownloadMediaParser() override;

  // Parse media metadata and thumbnail in a local file. All file IO will run on
  // |file_task_runner|. The metadata is parsed in an utility process safely.
  // The thumbnail is retrieved from GPU process or utility process based on
  // different codec.
  void Start(ParseCompleteCB parse_complete_cb);

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
  void OnVideoFrameRetrieved(
      bool success,
      chrome::mojom::VideoFrameDataPtr video_frame_data,
      const base::Optional<media::VideoDecoderConfig>& config);

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
                        std::unique_ptr<std::string> data);

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
  std::unique_ptr<media::VideoThumbnailDecoder> decoder_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;
  std::unique_ptr<media::MediaInterfaceProvider> media_interface_provider_;
  std::unique_ptr<media::GpuVideoAcceleratorFactories> gpu_factories_;
  bool decode_done_;

  base::WeakPtrFactory<DownloadMediaParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadMediaParser);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MEDIA_PARSER_H_
