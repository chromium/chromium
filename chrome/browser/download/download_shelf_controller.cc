// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf_controller.h"

#include <utility>

#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"

using offline_items_collection::OfflineItemState;
using offline_items_collection::OfflineContentAggregator;

DownloadShelfController::DownloadShelfController(Profile* profile)
    : profile_(profile) {
  aggregator_ =
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
  observation_.Observe(aggregator_.get());
}

DownloadShelfController::~DownloadShelfController() = default;

void DownloadShelfController::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  for (const auto& item : items)
    OnItemUpdated(item, std::nullopt);
}

void DownloadShelfController::OnItemRemoved(const ContentId& id) {
  if (OfflineItemUtils::IsDownload(id))
    return;

  OfflineItemModelManagerFactory::GetForBrowserContext(profile_)
      ->RemoveOfflineItemModelData(id);
}

void DownloadShelfController::OnItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  if (profile_->IsOffTheRecord() != item.is_off_the_record)
    return;

  if (OfflineItemUtils::IsDownload(item.id))
    return;

  if (item.state == OfflineItemState::CANCELLED)
    return;

  if (item.id.name_space == ContentIndexProviderImpl::kProviderNamespace)
    return;

  OfflineItemModelManager* manager =
      OfflineItemModelManagerFactory::GetForBrowserContext(profile_);

  DownloadUIModel::DownloadUIModelPtr model =
      OfflineItemModel::Wrap(manager, item);

  if (!model->WasUINotified()) {
    model->SetWasUINotified(true);
    OnNewOfflineItemReady(std::move(model));
  }
}

void DownloadShelfController::OnContentProviderGoingDown() {
  observation_.Reset();
}

void DownloadShelfController::OnNewOfflineItemReady(
    DownloadUIModel::DownloadUIModelPtr model) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);

  if (browser && browser->window() && browser->window()->GetDownloadShelf()) {
    // Add the offline item to DownloadShelf in the browser window.
    browser->window()->GetDownloadShelf()->AddDownload(std::move(model));
  }
}
