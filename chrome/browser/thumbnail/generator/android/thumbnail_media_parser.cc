// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser.h"

#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_FFMPEG)
#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser_impl.h"
#else
#include "chrome/browser/thumbnail/generator/android/noop_thumbnail_media_parser.h"
#endif

std::unique_ptr<ThumbnailMediaParser> ThumbnailMediaParser::Create(
    const std::string& mime_type,
    const base::FilePath& file_path) {
#if BUILDFLAG(ENABLE_FFMPEG)
  return std::make_unique<ThumbnailMediaParserImpl>(mime_type, file_path);
#else
  return std::make_unique<NoopThumbnailMediaParser>();
#endif
}
