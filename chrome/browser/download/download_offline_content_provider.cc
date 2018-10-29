// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider.h"

#include <utility>

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/download/image_thumbnail_request.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using OfflineItemVisuals = offline_items_collection::OfflineItemVisuals;

namespace {

// Thumbnail size used for generating thumbnails for image files.
const int kThumbnailSizeInDP = 64;

bool ShouldShowDownloadItem(const download::DownloadItem* item) {
  return !item->IsTemporary() && !item->IsTransient() && !item->IsDangerous();
}

}  // namespace

DownloadOfflineContentProvider::DownloadOfflineContentProvider(
    DownloadManager* manager)
    : manager_(manager),
      download_notifier_(manager, this),
      weak_ptr_factory_(this) {
  Profile* profile = Profile::FromBrowserContext(manager_->GetBrowserContext());
  profile = profile->GetOriginalProfile();
  aggregator_ = OfflineContentAggregatorFactory::GetForBrowserContext(profile);
  bool incognito = manager_->GetBrowserContext()->IsOffTheRecord();
  name_space_ = OfflineContentAggregator::CreateUniqueNameSpace(
      OfflineItemUtils::GetDownloadNamespacePrefix(incognito), incognito);
  aggregator_->RegisterProvider(name_space_, this);
}

DownloadOfflineContentProvider::~DownloadOfflineContentProvider() {
  aggregator_->UnregisterProvider(name_space_);
}

// TODO(shaktisahu) : Pass DownloadOpenSource.
void DownloadOfflineContentProvider::OpenItem(LaunchLocation location,
                                              const ContentId& id) {
  download::DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  if (item)
    item->OpenDownload();
}

void DownloadOfflineContentProvider::RemoveItem(const ContentId& id) {
  download::DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  if (item)
    item->Remove();
}

void DownloadOfflineContentProvider::CancelDownload(const ContentId& id) {
  download::DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  if (item)
    item->Cancel(true);
}

void DownloadOfflineContentProvider::PauseDownload(const ContentId& id) {
  download::DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  if (item)
    item->Pause();
}

void DownloadOfflineContentProvider::ResumeDownload(const ContentId& id,
                                                    bool has_user_gesture) {
  download::DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  if (item)
    item->Resume();
}

void DownloadOfflineContentProvider::GetItemById(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback) {
  DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  auto offline_item =
      item && ShouldShowDownloadItem(item)
          ? base::make_optional(
                OfflineItemUtils::CreateOfflineItem(name_space_, item))
          : base::nullopt;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), offline_item));
}

void DownloadOfflineContentProvider::GetAllItems(
    OfflineContentProvider::MultipleItemCallback callback) {
  DownloadManager::DownloadVector all_items;
  manager_->GetAllDownloads(&all_items);

  std::vector<OfflineItem> items;
  for (auto* item : all_items) {
    if (!ShouldShowDownloadItem(item))
      continue;

    items.push_back(OfflineItemUtils::CreateOfflineItem(name_space_, item));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), items));
}

void DownloadOfflineContentProvider::GetVisualsForItem(
    const ContentId& id,
    VisualsCallback callback) {
  // TODO(crbug.com/855330) Supply thumbnail if item is visible.
  DownloadItem* item = manager_->GetDownloadByGuid(id.id);
  if (!item)
    return;

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  int icon_size = kThumbnailSizeInDP * display.device_scale_factor();

  auto request = std::make_unique<ImageThumbnailRequest>(
      icon_size,
      base::BindOnce(&DownloadOfflineContentProvider::OnThumbnailRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
  request->Start(item->GetTargetFilePath());

  // Dropping ownership of |request| here because it will clean itself up once
  // the started request finishes.
  request.release();
}

void DownloadOfflineContentProvider::GetShareInfoForItem(
    const ContentId& id,
    ShareCallback callback) {}

void DownloadOfflineContentProvider::OnThumbnailRetrieved(
    const ContentId& id,
    VisualsCallback callback,
    const SkBitmap& bitmap) {
  auto visuals = std::make_unique<OfflineItemVisuals>();
  visuals->icon = gfx::Image::CreateFrom1xBitmap(bitmap);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
}

void DownloadOfflineContentProvider::AddObserver(
    OfflineContentProvider::Observer* observer) {
  if (observers_.HasObserver(observer))
    return;
  observers_.AddObserver(observer);
}

void DownloadOfflineContentProvider::RemoveObserver(
    OfflineContentProvider::Observer* observer) {
  if (!observers_.HasObserver(observer))
    return;

  observers_.RemoveObserver(observer);
}

void DownloadOfflineContentProvider::OnDownloadUpdated(DownloadManager* manager,
                                                       DownloadItem* item) {
  // Wait until the target path is determined or the download is canceled.
  if (item->GetTargetFilePath().empty() &&
      item->GetState() != DownloadItem::CANCELLED)
    return;

  if (!ShouldShowDownloadItem(item))
    return;

  for (auto& observer : observers_) {
    observer.OnItemUpdated(
        OfflineItemUtils::CreateOfflineItem(name_space_, item));
  }
}

void DownloadOfflineContentProvider::OnDownloadRemoved(DownloadManager* manager,
                                                       DownloadItem* item) {
  if (!ShouldShowDownloadItem(item))
    return;

  ContentId contentId(name_space_, item->GetGuid());
  for (auto& observer : observers_)
    observer.OnItemRemoved(contentId);
}
