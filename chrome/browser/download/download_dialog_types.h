// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIALOG_TYPES_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIALOG_TYPES_H_

// The type of download location dialog that should by shown by Android.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.download
// Recorded in histogram, so do not delete or reuse entries. The values must
// match DownloadLocationDialogType in enums.xml.
enum class DownloadLocationDialogType {
  NO_DIALOG = 0,           // No dialog.
  DEFAULT = 1,             // Dialog without any error states.
  LOCATION_FULL = 2,       // Error dialog, default location is full.
  LOCATION_NOT_FOUND = 3,  // Error dialog, default location is not found.
  NAME_CONFLICT = 4,  // Error dialog, there is already a file with that name.
  NAME_TOO_LONG = 5,  // Error dialog, the file name is too long.
  LOCATION_SUGGESTION = 6,  // Dialog showing alternative location suggestion.
  kMaxValue = LOCATION_SUGGESTION
};

// Result of download location dialog.
// Recorded in histogram, so do not delete or reuse entries. The values must
// match DownloadLocationDialogResult in enums.xml.
enum class DownloadLocationDialogResult {
  USER_CONFIRMED = 0,    // User confirmed a file path.
  USER_CANCELED = 1,     // User canceled file path selection.
  DUPLICATE_DIALOG = 2,  // Dialog is already showing.
  kMaxValue = DUPLICATE_DIALOG
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIALOG_TYPES_H_
