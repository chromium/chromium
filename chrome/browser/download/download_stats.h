// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_

#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_path_reservation_tracker.h"

// Used for counting UMA stats. Similar to content's
// download_stats::DownloadCountTypes but from the chrome layer.
enum ChromeDownloadCountTypes {
  // Stale enum values left around os that values passed to UMA don't
  // change.
  CHROME_DOWNLOAD_COUNT_UNUSED_0 = 0,
  CHROME_DOWNLOAD_COUNT_UNUSED_1,
  CHROME_DOWNLOAD_COUNT_UNUSED_2,
  CHROME_DOWNLOAD_COUNT_UNUSED_3,

  // A download *would* have been initiated, but it was blocked
  // by the DownloadThrottlingResourceHandler.
  CHROME_DOWNLOAD_COUNT_BLOCKED_BY_THROTTLING,

  CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

// Used for counting UMA stats. Similar to content's
// download_stats::DownloadInitiattionSources but from the chrome layer.
enum ChromeDownloadSource {
  // The download was initiated by navigating to a URL (e.g. by user click).
  DOWNLOAD_INITIATED_BY_NAVIGATION = 0,

  // The download was initiated by invoking a context menu within a page.
  DOWNLOAD_INITIATED_BY_CONTEXT_MENU,

  // Formerly DOWNLOAD_INITIATED_BY_WEBSTORE_INSTALLER.
  CHROME_DOWNLOAD_SOURCE_UNUSED_2,

  // Formerly DOWNLOAD_INITIATED_BY_IMAGE_BURNER.
  CHROME_DOWNLOAD_SOURCE_UNUSED_3,

  // Formerly DOWNLOAD_INITIATED_BY_PLUGIN_INSTALLER.
  CHROME_DOWNLOAD_SOURCE_UNUSED_4,

  // The download was initiated by the PDF plugin.
  DOWNLOAD_INITIATED_BY_PDF_SAVE,

  // Formerly DOWNLOAD_INITIATED_BY_EXTENSION.
  CHROME_DOWNLOAD_SOURCE_UNUSED_6,

  CHROME_DOWNLOAD_SOURCE_LAST_ENTRY
};

// How a download was opened. Note that a download could be opened multiple
// times.
enum ChromeDownloadOpenMethod {
  // The download was opened using the platform handler. There was no special
  // handling for this download.
  DOWNLOAD_OPEN_METHOD_DEFAULT_PLATFORM = 0,

  // The download was opened using the browser bypassing the system handler.
  DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER,

  // The user chose to open the download using the system handler even though
  // the preferred method was to open the download using the browser.
  DOWNLOAD_OPEN_METHOD_USER_PLATFORM,

  DOWNLOAD_OPEN_METHOD_LAST_ENTRY
};

// Records path generation behavior in download target determination process.
// Used in UMA, do not remove, change or reuse existing entries.
// Update histograms.xml and enums.xml when adding entries.
enum class DownloadPathGenerationEvent {
  // Use existing virtual path provided to download target determiner.
  USE_EXISTING_VIRTUAL_PATH = 0,
  // Use the force path provided to download target determiner.
  USE_FORCE_PATH,
  // Use last prompt directory.
  USE_LAST_PROMPT_DIRECTORY,
  // Use the default download directory.
  USE_DEFAULTL_DOWNLOAD_DIRECTORY,
  // No valid target file path is provided, the download will fail soon.
  NO_VALID_PATH,

  COUNT
};

// Increment one of the above counts.
void RecordDownloadCount(ChromeDownloadCountTypes type);

// Record initiation of a download from a specific source.
void RecordDownloadSource(ChromeDownloadSource source);

// Record that a download warning was shown.
void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type);

// Record that the user opened the confirmation dialog for a dangerous download.
void RecordOpenedDangerousConfirmDialog(
    download::DownloadDangerType danger_type);

// Record how a download was opened.
void RecordDownloadOpenMethod(ChromeDownloadOpenMethod open_method);

// Record if the database is available to provide the next download id before
// starting all downloads.
void RecordDatabaseAvailability(bool is_available);

// Record download path generation event in target determination process.
void RecordDownloadPathGeneration(DownloadPathGenerationEvent event,
                                  bool is_transient);

// Record path validation result.
void RecordDownloadPathValidation(download::PathValidationResult result,
                                  bool is_transient);

// Records drags of completed downloads from the shelf. Used in UMA, do not
// remove, change or reuse existing entries. Update histograms.xml and
// enums.xml when adding entries.
enum class DownloadShelfDragEvent {
  // A download was dragged. All platforms.
  STARTED,
  // The download was dropped somewhere that isn't a drag target. Currently
  // only recorded on Mac.
  CANCELED,
  // The download was dropped somewhere useful (a folder, an application,
  // etc.). Currently only recorded on Mac.
  DROPPED,

  COUNT
};

void RecordDownloadShelfDragEvent(DownloadShelfDragEvent drag_event);

#if defined(OS_ANDROID)
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

#endif

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
