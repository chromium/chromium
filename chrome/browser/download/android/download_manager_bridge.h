// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_BRIDGE_H_

#include "base/callback.h"
#include "components/download/public/common/download_item.h"

using DownloadItem = download::DownloadItem;
using AddCompletedDownloadCallback = base::OnceCallback<void(int64_t)>;

// This class pairs with DownloadManagerBridge on Java side, that handles all
// the android DownloadManager related functionalities. Both classes have only
// static functions.
class DownloadManagerBridge {
 public:
  static void AddCompletedDownload(DownloadItem* download,
                                   AddCompletedDownloadCallback callback);
  static void RemoveCompletedDownload(DownloadItem* download);

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerBridge);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_BRIDGE_H_
