// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf_controller.h"

#include <utility>

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
  aggregator_->AddObserver(this);
}

DownloadShelfController::~DownloadShelfController() {
  aggregator_->RemoveObserver(this);
}

void DownloadShelfController::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  for (const auto& item : items)
    OnItemUpdated(item, base::nullopt);
}

void DownloadShelfController::OnItemRemoved(const ContentId& id) {
  if (OfflineItemUtils::IsDownload(id))
    return;

  OfflineItemModelManagerFactory::GetForBrowserContext(profile_)
      ->RemoveOfflineItemModelData(id);
}

void DownloadShelfController::OnItemUpdated(
    const OfflineItem& item,
    const base::Optional<UpdateDelta>& update_delta) {
  if (profile_->IsOffTheRecord() != item.is_off_the_record)
    return;

  if (OfflineItemUtils::IsDownload(item.id))
    return;

  if (item.state == OfflineItemState::CANCELLED)
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

void DownloadShelfController::OnNewOfflineItemReady(
    DownloadUIModel::DownloadUIModelPtr model) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);

  if (browser && browser->window()) {
    // Add the offline item to DownloadShelf in the browser window.
    browser->window()->GetDownloadShelf()->AddDownload(std::move(model));
  }
}
