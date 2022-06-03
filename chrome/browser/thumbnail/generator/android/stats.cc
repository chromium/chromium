// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/generator/android/stats.h"

#include "base/metrics/histogram_macros.h"

void RecordMediaParserEvent(MediaParserEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.MediaParser.Event", event,
                            MediaParserEvent::kCount);
}

void RecordMediaMetadataEvent(MediaMetadataEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.MediaMetadata.Event", event,
                            MediaMetadataEvent::kCount);
}

void RecordVideoThumbnailEvent(VideoThumbnailEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.VideoThumbnail.Event", event,
                            VideoThumbnailEvent::kCount);
}
