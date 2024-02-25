// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chromeos/components/sharesheet/constants.h"
#include "components/services/app_service/public/cpp/intent.h"

namespace sharesheet {

extern const char kSharesheetUserActionResultHistogram[];
extern const char kSharesheetAppCountAllResultHistogram[];
extern const char kSharesheetAppCountArcResultHistogram[];
extern const char kSharesheetAppCountWebResultHistogram[];
extern const char kSharesheetLaunchSourceResultHistogram[];
extern const char kSharesheetFileCountResultHistogram[];
extern const char kSharesheetMimeTypeResultHistogram[];
extern const char kSharesheetCopyToClipboardMimeTypeResultHistogram[];

class SharesheetMetrics {
 public:
  // The action taken by a user after the sharesheet is invoked.
  // This enum is for recording histograms and must be treated as append-only.
  enum class UserAction {
    kCancelledThroughClickingOut =
        0,          // User cancelled sharesheet by clicking outside the bubble.
    kArc,           // Opened an ARC app.
    kNearbyAction,  // User selected the nearby share action.
    kCancelledThroughEscPress,  // User cancelled sharesheet by pressing esc on
                                // keyboard.
    kWeb,                       // Opened a web app.
    kDriveAction,               // User selected the drive share action.
    kCopyAction,                // User selected the copy share action.
    kMaxValue = kCopyAction,
  };

  // The mime type that is being shared.
  // This enum is for recording histograms and must be treated as append-only.
  enum class MimeType {
    kUnknown = 0,
    kText = 1,
    kUrl = 2,
    kTextFile = 3,
    kImageFile = 4,
    kVideoFile = 5,
    kAudioFile = 6,
    kPdfFile = 7,
    kMaxValue = kPdfFile
  };

  SharesheetMetrics();

  static void RecordSharesheetActionMetrics(const UserAction action);

  // Records number of each target type that appear in the Sharesheet
  // when it is invoked.
  static void RecordSharesheetAppCount(const int app_count);
  static void RecordSharesheetArcAppCount(const int app_count);
  static void RecordSharesheetWebAppCount(const int app_count);

  static void RecordSharesheetLaunchSource(const LaunchSource source);

  static void RecordSharesheetFilesSharedCount(const int file_count);
  static void RecordSharesheetMimeType(const MimeType mime_type);
  static void RecordCopyToClipboardShareActionMimeType(
      const MimeType mime_type);

  // Utility Functions
  static MimeType ConvertMimeTypeForMetrics(std::string mime_type);
  static base::flat_set<MimeType> GetMimeTypesFromIntentForMetrics(
      const apps::IntentPtr& intent);
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_
