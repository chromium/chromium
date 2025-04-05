// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_manager.h"

#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/offline_item_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

using offline_items_collection::OfflineContentAggregator;
using offline_items_collection::OfflineItemState;

DownloadNotificationManager::DownloadNotificationManager(Profile* profile)
    : profile_(profile) {
#if BUILDFLAG(IS_CHROMEOS)
  if (ash::features::IsOfflineItemsInNotificationsEnabled()) {
    aggregator_ =
        OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
    observation_.Observe(aggregator_.get());
  }
#endif
}

DownloadNotificationManager::~DownloadNotificationManager() {
  for (auto& item : items_) {
    DownloadItemNotification* download_notification = item.second.get();
    DownloadUIModel* model = download_notification->GetDownload();
    if (model->GetState() == download::DownloadItem::IN_PROGRESS)
      download_notification->DisablePopup();
  }
}

void DownloadNotificationManager::OnNewDownloadReady(
    download::DownloadItem* item) {
  PrioritizeDownloadItemNotification(std::make_unique<DownloadItemModel>(item));
}

void DownloadNotificationManager::OnDownloadDestroyed(
    const ContentId& contentId) {
  items_.erase(contentId);
}

void DownloadNotificationManager::OnContentProviderGoingDown() {
  observation_.Reset();
}

void DownloadNotificationManager::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  for (const auto& item : items) {
    OnItemUpdated(item, std::nullopt);
  }
}

void DownloadNotificationManager::OnItemRemoved(const ContentId& id) {
  if (OfflineItemUtils::IsDownload(id)) {
    return;
  }
  OfflineItemModelManagerFactory::GetForBrowserContext(profile_)
      ->RemoveOfflineItemModelData(id);
}

void DownloadNotificationManager::OnItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  if (profile_->IsOffTheRecord() != item.is_off_the_record) {
    return;
  }

  if (OfflineItemUtils::IsDownload(item.id)) {
    return;
  }

  if (item.state == OfflineItemState::CANCELLED) {
    return;
  }

  // We don't show a UI when an item is saved to the ContentIndex.
  if (item.id.name_space == ContentIndexProviderImpl::kProviderNamespace) {
    return;
  }

  OfflineItemModelManager* manager =
      OfflineItemModelManagerFactory::GetForBrowserContext(profile_);

  DownloadUIModel::DownloadUIModelPtr model =
      OfflineItemModel::Wrap(manager, item);

  if (!model->WasUINotified()) {
    model->SetWasUINotified(true);
    PrioritizeDownloadItemNotification(std::move(model));
  }
}

void DownloadNotificationManager::PrioritizeDownloadItemNotification(
    DownloadUIModel::DownloadUIModelPtr ui_model) {
  // Lower the priority of all existing in-progress download notifications.
  for (auto& it : items_) {
    DownloadItemNotification* notification = it.second.get();
    DownloadUIModel* model = notification->GetDownload();
    if (model->GetState() == download::DownloadItem::IN_PROGRESS) {
      notification->DisablePopup();
    }
  }
  ContentId contentId = ui_model->GetContentId();
  auto entry = items_.emplace(
      contentId, new DownloadItemNotification(profile_, std::move(ui_model)));
  entry.first->second->SetObserver(this);
}
