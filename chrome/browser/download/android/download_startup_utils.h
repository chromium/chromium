// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_

// Native side of DownloadStartupUtils.java.
class DownloadStartupUtils {
 public:
  // Ensures that the download system is initialized for the targeted profile.
  // If the corresponding profile is not created, this method will do nothing.
  static void EnsureDownloadSystemInitialized(bool is_full_browser_started,
                                              bool is_incognito);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_
