// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_STATS_H_
#define CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_STATS_H_

#include "build/build_config.h"

// Tracks media parser events. Each media parser hubs IPC channels for local
// media analysis tasks. Used in UMA, do not remove, change or reuse existing
// entries.
enum class MediaParserEvent {
  // Started to initialize the media parser.
  kInitialize = 0,
  // The mime type is not supported by the media parser.
  kUnsupportedMimeType = 1,
  // Failed to read the local media file.
  kReadFileError = 2,
  // Utility process connection error.
  kUtilityConnectionError = 3,
  // GPU process connection error.
  kGpuConnectionError = 4,
  // Failed to parse metadata.
  kMetadataFailed = 5,
  // Failed to retrieve video thumbnail.
  kVideoThumbnailFailed = 6,
  // Failed to parse media file, aggregation of all failure reasons.
  kFailure = 7,
  // Media file successfully parsed.
  kSuccess = 8,
  // Time out and failed.
  kTimeout = 9,
  kCount
};

// Tracks local media metadata requests. Used in UMA, do not remove, change or
// reuse existing entries.
enum class MediaMetadataEvent {
  // Started to retrieve metadata.
  kMetadataStart = 0,
  // Failed to retrieve metadata.
  kMetadataFailed = 1,
  // Completed to retrieve metadata.
  kMetadataComplete = 2,
  kCount
};

// Tracks video thumbnail requests. Used in UMA, do not remove, change or
// reuse existing entries.
enum class VideoThumbnailEvent {
  kVideoThumbnailStart = 0,
  // Failed to extract video frame.
  kVideoFrameExtractionFailed = 1,
  // Failed to decode video frame.
  kVideoDecodeFailed = 2,
  // Completed to retrieve video thumbnail.
  kVideoThumbnailComplete = 3,
  kCount
};

// Records download media parser event.
void RecordMediaParserEvent(MediaParserEvent event);

// Records media metadata parsing events.
void RecordMediaMetadataEvent(MediaMetadataEvent event);

// Records video thumbnail retrieval events.
void RecordVideoThumbnailEvent(VideoThumbnailEvent event);

#endif  // CHROME_BROWSER_THUMBNAIL_GENERATOR_ANDROID_STATS_H_
