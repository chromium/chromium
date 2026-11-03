// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_

namespace offline_pages {
namespace android {

/**
 * Bridge between C++ and Java to handle user initiated download of an offline
 * page. Other user interactions related to offline page are handled by the
 * DownloadUIAdapter.
 */
class OfflinePageDownloadBridge {
 public:
  OfflinePageDownloadBridge() = delete;
  OfflinePageDownloadBridge(const OfflinePageDownloadBridge&) = delete;
  OfflinePageDownloadBridge& operator=(const OfflinePageDownloadBridge&) =
      delete;

  static void ShowDownloadingToast();
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_
