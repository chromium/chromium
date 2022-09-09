// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_

class ProfileKey;

// Native side of DownloadStartupUtils.java.
class DownloadStartupUtils {
 public:
  DownloadStartupUtils() = delete;
  DownloadStartupUtils(const DownloadStartupUtils&) = delete;
  DownloadStartupUtils& operator=(const DownloadStartupUtils&) = delete;

  // Ensures that the download system is initialized for the targeted profile.
  // If |profile_key| is null, reduced mode will be assumed. The returned value
  // is the ProfileKey that was used.
  static ProfileKey* EnsureDownloadSystemInitialized(ProfileKey* profile_key);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_
