// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_manager.h"

#include "chrome/browser/download/download_item_model.h"

DownloadNotificationManager::DownloadNotificationManager(Profile* profile)
    : profile_(profile) {}

DownloadNotificationManager::~DownloadNotificationManager() {
  for (auto& item : items_) {
    DownloadItemNotification* download_notification = item.second.get();
    DownloadUIModel* model = download_notification->GetDownload();
    if (model->GetState() == download::DownloadItem::IN_PROGRESS)
      download_notification->DisablePopup();
    download_notification->ShutDown();
  }
}

void DownloadNotificationManager::OnNewDownloadReady(
    download::DownloadItem* item) {
  // Lower the priority of all existing in-progress download notifications.
  for (auto& item : items_) {
    DownloadUIModel* model = item.second->GetDownload();
    DownloadItemNotification* download_notification = item.second.get();
    if (model->GetState() == download::DownloadItem::IN_PROGRESS)
      download_notification->DisablePopup();
  }

  DownloadUIModel::DownloadUIModelPtr model(
      new DownloadItemModel(item),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
  ContentId contentId = model->GetContentId();
  DownloadItemNotification::DownloadItemNotificationPtr notification(
      new DownloadItemNotification(profile_, std::move(model)),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
  notification->SetObserver(this);
  items_.emplace(contentId, std::move(notification));
}

void DownloadNotificationManager::OnDownloadDestroyed(
    const ContentId& contentId) {
  items_.erase(contentId);
}
