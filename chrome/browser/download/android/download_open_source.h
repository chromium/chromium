// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_OPEN_SOURCE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_OPEN_SOURCE_H_

// Tracks where the users interact with download files on Android. Used in
// histogram. See AndroidDownloadOpenSource in enums.xml. The values used by
// this enum will be persisted to server logs and should not be deleted, changed
// or reused.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.download)
enum class DownloadOpenSource {
  // The action source is unknown.
  kUnknown = 0,
  // Android DownloadManager.
  kAndroidDownloadManager = 1,
  // Download home page.
  kDownloadHome = 2,
  // Android download notification.
  kNotification = 3,
  // New tab page.
  kNewTabPage = 4,
  // Download info bar.
  kInfoBar = 5,
  // Download snack bar.
  kSnackBar = 6,
  // Download is auto opened after completion.
  kAutoOpen = 7,
  // Download progress info bar.
  kDownloadProgressInfoBar = 8,
  // Main menu.
  kMenu = 9,
  // Offline content on Dino page
  kDinoPageOfflineContent = 10,
  // Offline indicator.
  kOfflineIndicator = 11,
  // Android notification for offline content.
  kOfflineContentNotification = 12,
  // Download progress message.
  kDownloadProgressMessage = 13,
  // Duplicate download dialog.
  kDuplicateDownloadDialog = 14,
  // Download triggered by external app.
  kExternalApp = 15,
  // New download tab open button.
  kNewDownloadTabOpenButton = 16,
  kMaxValue = kNewDownloadTabOpenButton
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_OPEN_SOURCE_H_
