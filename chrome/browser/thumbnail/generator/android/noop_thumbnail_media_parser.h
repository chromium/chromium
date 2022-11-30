// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_NOOP_THUMBNAIL_MEDIA_PARSER_H_
#define CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_NOOP_THUMBNAIL_MEDIA_PARSER_H_

#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser.h"

// Empty implementation used when ENABLE_FFMPEG build flag is false.
class NoopThumbnailMediaParser : public ThumbnailMediaParser {
 public:
  NoopThumbnailMediaParser() = default;
  NoopThumbnailMediaParser(const NoopThumbnailMediaParser&) = delete;
  NoopThumbnailMediaParser& operator=(const NoopThumbnailMediaParser&) = delete;
  ~NoopThumbnailMediaParser() override = default;

 private:
  // ThumbnailMediaParser implementation.
  void Start(ParseCompleteCB parse_complete_cb) override;
};

#endif  // CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_NOOP_THUMBNAIL_MEDIA_PARSER_H_
