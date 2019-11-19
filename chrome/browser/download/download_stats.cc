// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/metrics/histogram_macros.h"

void RecordDownloadCount(ChromeDownloadCountTypes type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.CountsChrome", type, CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadSource(ChromeDownloadSource source) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.SourcesChrome", source, CHROME_DOWNLOAD_SOURCE_LAST_ENTRY);
}

void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("Download.DownloadWarningShown", danger_type,
                            download::DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordOpenedDangerousConfirmDialog(
    download::DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("Download.ShowDangerousDownloadConfirmationPrompt",
                            danger_type, download::DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordDownloadOpenMethod(ChromeDownloadOpenMethod open_method) {
  UMA_HISTOGRAM_ENUMERATION("Download.OpenMethod",
                            open_method,
                            DOWNLOAD_OPEN_METHOD_LAST_ENTRY);
}

void RecordDatabaseAvailability(bool is_available) {
  UMA_HISTOGRAM_BOOLEAN("Download.Database.IsAvailable", is_available);
}

void RecordDownloadPathGeneration(DownloadPathGenerationEvent event,
                                  bool is_transient) {
  if (is_transient) {
    UMA_HISTOGRAM_ENUMERATION("Download.PathGenerationEvent.Transient", event,
                              DownloadPathGenerationEvent::COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Download.PathGenerationEvent.UserDownload",
                              event, DownloadPathGenerationEvent::COUNT);
  }
}

void RecordDownloadPathValidation(download::PathValidationResult result,
                                  bool is_transient) {
  if (is_transient) {
    UMA_HISTOGRAM_ENUMERATION("Download.PathValidationResult.Transient", result,
                              download::PathValidationResult::COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Download.PathValidationResult.UserDownload",
                              result, download::PathValidationResult::COUNT);
  }
}

void RecordDownloadShelfDragEvent(DownloadShelfDragEvent drag_event) {
  UMA_HISTOGRAM_ENUMERATION("Download.Shelf.DragEvent", drag_event,
                            DownloadShelfDragEvent::COUNT);
}

#if defined(OS_ANDROID)
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
#endif
