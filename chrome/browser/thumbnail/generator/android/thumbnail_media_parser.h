// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_THUMBNAIL_MEDIA_PARSER_H_
#define CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_THUMBNAIL_MEDIA_PARSER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom-forward.h"

class SkBitmap;

// Parse local media files, including media metadata and thumbnails.
// Metadata is always parsed in utility process for both audio and video files.
class ThumbnailMediaParser {
 public:
  using ParseCompleteCB =
      base::OnceCallback<void(bool success,
                              chrome::mojom::MediaMetadataPtr media_metadata,
                              SkBitmap bitmap)>;

  // Creates the parser, may return an empty implementation when ffmpeg is
  // disabled.
  static std::unique_ptr<ThumbnailMediaParser> Create(
      const std::string& mime_type,
      const base::FilePath& file_path);

  ThumbnailMediaParser() = default;
  ThumbnailMediaParser(const ThumbnailMediaParser&) = delete;
  ThumbnailMediaParser& operator=(const ThumbnailMediaParser&) = delete;
  virtual ~ThumbnailMediaParser() = default;

  // Parse media metadata and thumbnail in a local file. All file IO will run on
  // |file_task_runner|. The metadata is parsed in an utility process safely.
  // The thumbnail is retrieved from GPU process or utility process based on
  // different codec.
  virtual void Start(ParseCompleteCB parse_complete_cb) = 0;
};

#endif  // CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_THUMBNAIL_MEDIA_PARSER_H_
