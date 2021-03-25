// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_

namespace sharesheet {

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
    kMaxValue = kDriveAction,
  };

  // Device form factor when sharesheet is invoked.
  // This enum is for recording histograms and must be treated as append-only.
  enum class FormFactor {
    kTablet = 0,
    kClamshell,
    kMaxValue = kClamshell,
  };

  // The source from which the sharesheet was launched from.
  // This enum is for recording histograms and must be treated as append-only.
  enum class LaunchSource {
    kUnknown = 0,
    kFilesAppShareButton = 1,
    kFilesAppContextMenu = 2,
    kWebShare = 3,
    kArcNearbyShare = 4,
    kMaxValue = kArcNearbyShare,
  };

  SharesheetMetrics();

  static void RecordSharesheetActionMetrics(const UserAction action);

  // Records number of each target type that appear in the Sharesheet
  // when it is invoked.
  static void RecordSharesheetAppCount(const int app_count);
  static void RecordSharesheetArcAppCount(const int app_count);
  static void RecordSharesheetWebAppCount(const int app_count);
  static void RecordSharesheetShareAction(const UserAction action);

  static void RecordSharesheetFormFactor(const FormFactor form_factor);

  static void RecordSharesheetLaunchSource(const LaunchSource source);
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_
