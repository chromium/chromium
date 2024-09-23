// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_impl.h"

#include <memory>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/update_delta.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/service_tab_launcher.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#else
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#endif

using offline_items_collection::ContentId;
using offline_items_collection::LaunchLocation;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemFilter;

namespace {

constexpr char kEntryKeySeparator[] = "#";

struct EntryKeyComponents {
  int64_t service_worker_registration_id;
  url::Origin origin;
  std::string description_id;
};

std::string EntryKey(int64_t service_worker_registration_id,
                     const url::Origin& origin,
                     const std::string& description_id) {
  return base::NumberToString(service_worker_registration_id) +
         kEntryKeySeparator + origin.GetURL().spec() + kEntryKeySeparator +
         description_id;
}

std::string EntryKey(const content::ContentIndexEntry& entry) {
  return EntryKey(
      entry.service_worker_registration_id,
      url::Origin::Create(entry.launch_url.DeprecatedGetOriginAsURL()),
      entry.description->id);
}

EntryKeyComponents GetEntryKeyComponents(const std::string& key) {
  size_t pos1 = key.find_first_of(kEntryKeySeparator);
  DCHECK_NE(pos1, std::string::npos);
  size_t pos2 = key.find_first_of(kEntryKeySeparator, pos1 + 1);
  DCHECK_NE(pos2, std::string::npos);

  int64_t service_worker_registration_id = -1;
  base::StringToInt64(std::string_view(key.data(), pos1),
                      &service_worker_registration_id);

  GURL origin(key.substr(pos1 + 1, pos2 - pos1 - 1));
  DCHECK(origin.is_valid());

  return {service_worker_registration_id, url::Origin::Create(origin),
          key.substr(pos2 + 1)};
}

OfflineItemFilter CategoryToFilter(blink::mojom::ContentCategory category) {
  switch (category) {
    case blink::mojom::ContentCategory::NONE:
    case blink::mojom::ContentCategory::HOME_PAGE:
    case blink::mojom::ContentCategory::ARTICLE:
      return OfflineItemFilter::FILTER_PAGE;
    case blink::mojom::ContentCategory::VIDEO:
      return OfflineItemFilter::FILTER_VIDEO;
    case blink::mojom::ContentCategory::AUDIO:
      return OfflineItemFilter::FILTER_AUDIO;
  }
}

}  // namespace

const char ContentIndexProviderImpl::kProviderNamespace[] = "content_index";

ContentIndexProviderImpl::ContentIndexProviderImpl(Profile* profile)
    : profile_(profile),
      metrics_(ukm::UkmBackgroundRecorderFactory::GetForProfile(profile)),
      aggregator_(
          OfflineContentAggregatorFactory::GetForKey(profile->GetProfileKey())),
      site_engagement_service_(
          site_engagement::SiteEngagementServiceFactory::GetForProfile(
              profile)) {
  aggregator_->RegisterProvider(kProviderNamespace, this);
}

ContentIndexProviderImpl::~ContentIndexProviderImpl() {
  if (aggregator_)
    aggregator_->UnregisterProvider(kProviderNamespace);
}

void ContentIndexProviderImpl::Shutdown() {
  aggregator_->UnregisterProvider(kProviderNamespace);
  aggregator_ = nullptr;
  site_engagement_service_ = nullptr;
}

std::vector<gfx::Size> ContentIndexProviderImpl::GetIconSizes(
    blink::mojom::ContentCategory category) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (icon_sizes_for_testing_)
    return *icon_sizes_for_testing_;

#if BUILDFLAG(IS_ANDROID)
  // Recommended notification icon size for Android.
  return {{192, 192}};
#else
  return {};
#endif
}

void ContentIndexProviderImpl::OnContentAdded(
    content::ContentIndexEntry entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!entry.is_top_level_context)
    return;

  OfflineItemList items(1, EntryToOfflineItem(entry));

  // Delete the entry before adding it just in case the ID was overwritten.
  NotifyItemRemoved(items[0].id);

  NotifyItemsAdded(items);

  metrics_.RecordContentAdded(
      url::Origin::Create(entry.launch_url.DeprecatedGetOriginAsURL()),
      entry.description->category);
}

void ContentIndexProviderImpl::OnContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string entry_key =
      EntryKey(service_worker_registration_id, origin, description_id);
  ContentId id(kProviderNamespace, entry_key);
  NotifyItemRemoved(id);
}

void ContentIndexProviderImpl::OpenItem(
    const offline_items_collection::OpenParams& open_params,
    const ContentId& id) {
  auto components = GetEntryKeyComponents(id.id);

  auto* storage_partition = profile_->GetStoragePartitionForUrl(
      components.origin.GetURL(), /* can_create= */ false);

  if (!storage_partition || !storage_partition->GetContentIndexContext())
    return;

  storage_partition->GetContentIndexContext()->GetEntry(
      components.service_worker_registration_id, components.description_id,
      base::BindOnce(&ContentIndexProviderImpl::DidGetEntryToOpen,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContentIndexProviderImpl::DidGetEntryToOpen(
    std::optional<content::ContentIndexEntry> entry) {
  if (!entry)
    return;

#if BUILDFLAG(IS_ANDROID)
  content::OpenURLParams params(entry->launch_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /* is_renderer_initiated= */ false);
  ServiceTabLauncher::GetInstance()->LaunchTab(
      profile_, params,
      base::BindOnce(&ContentIndexProviderImpl::DidOpenTab,
                     weak_ptr_factory_.GetWeakPtr(), std::move(*entry)));
#else
  NavigateParams nav_params(profile_, entry->launch_url,
                            ui::PAGE_TRANSITION_LINK);
  Navigate(&nav_params);
  DidOpenTab(std::move(*entry), nav_params.navigated_or_inserted_contents);
#endif
}

void ContentIndexProviderImpl::DidOpenTab(content::ContentIndexEntry entry,
                                          content::WebContents* web_contents) {
  metrics_.RecordContentOpened(web_contents, entry.description->category);
}

void ContentIndexProviderImpl::RemoveItem(const ContentId& id) {
  auto components = GetEntryKeyComponents(id.id);

  auto* storage_partition = profile_->GetStoragePartitionForUrl(
      components.origin.GetURL(), /* can_create= */ false);

  if (!storage_partition || !storage_partition->GetContentIndexContext())
    return;

  metrics_.RecordContentDeletedByUser(components.origin);

  storage_partition->GetContentIndexContext()->OnUserDeletedItem(
      components.service_worker_registration_id, components.origin,
      components.description_id);
}

void ContentIndexProviderImpl::CancelDownload(const ContentId& id) {
  NOTREACHED_IN_MIGRATION();
}

void ContentIndexProviderImpl::PauseDownload(const ContentId& id) {
  NOTREACHED_IN_MIGRATION();
}

void ContentIndexProviderImpl::ResumeDownload(const ContentId& id) {
  NOTREACHED_IN_MIGRATION();
}

void ContentIndexProviderImpl::GetItemById(const ContentId& id,
                                           SingleItemCallback callback) {
  auto components = GetEntryKeyComponents(id.id);

  auto* storage_partition = profile_->GetStoragePartitionForUrl(
      components.origin.GetURL(), /* can_create= */ false);

  if (!storage_partition || !storage_partition->GetContentIndexContext()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  storage_partition->GetContentIndexContext()->GetEntry(
      components.service_worker_registration_id, components.description_id,
      base::BindOnce(&ContentIndexProviderImpl::DidGetItem,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentIndexProviderImpl::DidGetItem(
    SingleItemCallback callback,
    std::optional<content::ContentIndexEntry> entry) {
  if (!entry)
    std::move(callback).Run(std::nullopt);
  else
    std::move(callback).Run(EntryToOfflineItem(*entry));
}

void ContentIndexProviderImpl::GetAllItems(MultipleItemCallback callback) {
  // Get the number of Storage Paritions.
  std::vector<content::StoragePartition*> storage_partitions;
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* partition) {
        storage_partitions.push_back(partition);
      });
  DCHECK(!storage_partitions.empty());

  auto item_list = std::make_unique<OfflineItemList>();
  OfflineItemList* item_list_ptr = item_list.get();

  // Get the all entries from every partition.
  auto barrier_closure = base::BarrierClosure(
      storage_partitions.size(),
      base::BindOnce(
          &ContentIndexProviderImpl::DidGetAllEntriesAcrossStorageParitions,
          weak_ptr_factory_.GetWeakPtr(), std::move(item_list),
          std::move(callback)));

  for (auto* storage_partition : storage_partitions) {
    if (!storage_partition || !storage_partition->GetContentIndexContext()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, barrier_closure);
      continue;
    }

    // |item_list_ptr| is safe to use since it is owned by the barrier
    // closure.
    storage_partition->GetContentIndexContext()->GetAllEntries(base::BindOnce(
        &ContentIndexProviderImpl::DidGetAllEntries,
        weak_ptr_factory_.GetWeakPtr(), barrier_closure, item_list_ptr));
  }
}

void ContentIndexProviderImpl::DidGetAllEntriesAcrossStorageParitions(
    std::unique_ptr<OfflineItemList> item_list,
    MultipleItemCallback callback) {
  std::move(callback).Run(*item_list);
}

void ContentIndexProviderImpl::DidGetAllEntries(
    base::OnceClosure done_closure,
    OfflineItemList* item_list,
    blink::mojom::ContentIndexError error,
    std::vector<content::ContentIndexEntry> entries) {
  if (error != blink::mojom::ContentIndexError::NONE) {
    std::move(done_closure).Run();
    return;
  }

  for (const auto& entry : entries) {
    if (!entry.is_top_level_context)
      continue;
    item_list->push_back(EntryToOfflineItem(entry));
  }

  std::move(done_closure).Run();
}

void ContentIndexProviderImpl::GetVisualsForItem(const ContentId& id,
                                                 GetVisualsOptions options,
                                                 VisualsCallback callback) {
  auto components = GetEntryKeyComponents(id.id);

  auto* storage_partition = profile_->GetStoragePartitionForUrl(
      components.origin.GetURL(), /* can_create= */ false);

  if (!storage_partition || !storage_partition->GetContentIndexContext()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

  storage_partition->GetContentIndexContext()->GetIcons(
      components.service_worker_registration_id, components.description_id,
      base::BindOnce(&ContentIndexProviderImpl::DidGetIcons,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

OfflineItem ContentIndexProviderImpl::EntryToOfflineItem(
    const content::ContentIndexEntry& entry) {
  OfflineItem item;
  item.id = ContentId(kProviderNamespace, EntryKey(entry));
  item.title = entry.description->title;
  item.description = entry.description->description;
  item.filter = CategoryToFilter(entry.description->category);
  item.is_transient = false;
  item.is_suggested = true;
  item.creation_time = entry.registration_time;
  item.is_openable = true;
  item.state = offline_items_collection::OfflineItemState::COMPLETE;
  item.is_resumable = false;
  item.can_rename = false;
  item.url = entry.launch_url;

  if (site_engagement_service_) {
    item.content_quality_score =
        site_engagement_service_->GetScore(
            entry.launch_url.DeprecatedGetOriginAsURL()) /
        site_engagement::SiteEngagementScore::kMaxPoints;
  }

  return item;
}

void ContentIndexProviderImpl::DidGetIcons(const ContentId& id,
                                           VisualsCallback callback,
                                           std::vector<SkBitmap> icons) {
  auto visuals =
      std::make_unique<offline_items_collection::OfflineItemVisuals>();
  if (!icons.empty()) {
    DCHECK_EQ(icons.size(), 1u);
    visuals->icon = gfx::Image::CreateFrom1xBitmap(std::move(icons.front()));
  }
  std::move(callback).Run(id, std::move(visuals));
}

void ContentIndexProviderImpl::GetShareInfoForItem(const ContentId& id,
                                                   ShareCallback callback) {
  NOTIMPLEMENTED();
}

void ContentIndexProviderImpl::RenameItem(const ContentId& id,
                                          const std::string& name,
                                          RenameCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
