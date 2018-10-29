// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>

#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/notification/download_item_notification.h"
#include "components/offline_items_collection/core/offline_item.h"

class Profile;

using offline_items_collection::ContentId;

class DownloadNotificationManager : public DownloadUIController::Delegate,
                                    public DownloadItemNotification::Observer {
 public:
  explicit DownloadNotificationManager(Profile* profile);
  ~DownloadNotificationManager() override;

  // DownloadUIController::Delegate overrides.
  void OnNewDownloadReady(download::DownloadItem* item) override;

  // DownloadItemNotification::Observer overrides.
  void OnDownloadDestroyed(const ContentId& contentId) override;

 private:
  friend class test::DownloadItemNotificationTest;

  Profile* profile_;
  std::map<ContentId, DownloadItemNotification::DownloadItemNotificationPtr>
      items_;

  DISALLOW_COPY_AND_ASSIGN(DownloadNotificationManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
