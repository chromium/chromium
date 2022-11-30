// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_INTERCEPT_OMA_DOWNLOAD_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_INTERCEPT_OMA_DOWNLOAD_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

// Used to intercept the OMA DRM download from navigation and pass it to Android
// DownloadManager.
class InterceptOMADownloadNavigationThrottle
    : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> Create(
      content::NavigationHandle* handle);

  InterceptOMADownloadNavigationThrottle(
      const InterceptOMADownloadNavigationThrottle&) = delete;
  InterceptOMADownloadNavigationThrottle& operator=(
      const InterceptOMADownloadNavigationThrottle&) = delete;

  ~InterceptOMADownloadNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  explicit InterceptOMADownloadNavigationThrottle(
      content::NavigationHandle* handle);

  // Helper method to intercept the download.
  void InterceptDownload();
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_INTERCEPT_OMA_DOWNLOAD_NAVIGATION_THROTTLE_H_
