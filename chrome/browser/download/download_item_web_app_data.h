// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WEB_APP_DATA_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WEB_APP_DATA_H_

#include "base/supports_user_data.h"
#include "components/webapps/common/web_app_id.h"

namespace download {
class DownloadItem;
}

// Per DownloadItem data for storing web app data on downloads. This data is
// only set if a download was initiated by a web app.
class DownloadItemWebAppData : public base::SupportsUserData::Data {
 public:
  // Creates an instance with the given `web_app_id` and attaches it to the
  // item. Overwrites any existing DownloadItemWebAppData on the item.
  static void CreateAndAttachToItem(download::DownloadItem* item,
                                    const webapps::AppId& web_app_id);

  // Returns nullptr if no DownloadItemWebAppData is present, which will be the
  // case for most downloads (i.e. those not initiated by web apps).
  static DownloadItemWebAppData* Get(download::DownloadItem* item);

  DownloadItemWebAppData(const DownloadItemWebAppData&) = delete;
  DownloadItemWebAppData& operator=(const DownloadItemWebAppData&) = delete;

  const webapps::AppId& id() const { return web_app_id_; }

 private:
  static const char kKey[];

  explicit DownloadItemWebAppData(const webapps::AppId& web_app_id);

  webapps::AppId web_app_id_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WEB_APP_DATA_H_
