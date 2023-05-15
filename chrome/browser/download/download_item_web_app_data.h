// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WEB_APP_DATA_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WEB_APP_DATA_H_

#include "base/supports_user_data.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace download {
class DownloadItem;
}

// Per DownloadItem data for storing web app data on downloads. This data is
// only set if a download was initiated by a web app.
class DownloadItemWebAppData : public base::SupportsUserData::Data {
 public:
  // Returns nullptr if no DownloadItemWebAppData is present, which will be the
  // case for most downloads (i.e. those not initiated by web apps).
  static DownloadItemWebAppData* Get(download::DownloadItem* item);

  // Attaches itself to the |item|.
  DownloadItemWebAppData(download::DownloadItem* item,
                         const web_app::AppId& web_app_id);

  DownloadItemWebAppData(const DownloadItemWebAppData&) = delete;
  DownloadItemWebAppData& operator=(const DownloadItemWebAppData&) = delete;

  const web_app::AppId& id() const { return web_app_id_; }

 private:
  static const char kKey[];

  web_app::AppId web_app_id_;
};

#endif  // __CHROMIUM_SRC_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WEB_APP_DATA_H_
