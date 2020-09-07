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
    kCancelled = 0,  // User cancelled sharesheet.
    kArc,            // Opened an ARC app.
    kMaxValue = kArc,
  };

  SharesheetMetrics();

  static void RecordSharesheetActionMetrics(UserAction action);

  static void RecordSharesheetAppCount(int app_count);
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_METRICS_H_
