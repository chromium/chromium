// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
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

  DownloadNotificationManager(const DownloadNotificationManager&) = delete;
  DownloadNotificationManager& operator=(const DownloadNotificationManager&) =
      delete;

  ~DownloadNotificationManager() override;

  // DownloadUIController::Delegate overrides.
  void OnNewDownloadReady(download::DownloadItem* item) override;

  // DownloadItemNotification::Observer overrides.
  void OnDownloadDestroyed(const ContentId& contentId) override;

 private:
  friend class test::DownloadItemNotificationTest;

  raw_ptr<Profile> profile_;
  std::map<ContentId, std::unique_ptr<DownloadItemNotification>> items_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
