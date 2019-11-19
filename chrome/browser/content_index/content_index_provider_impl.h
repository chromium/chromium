// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/content_index/content_index_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/content_index_provider.h"

namespace content {
class WebContents;
}  // namespace content

namespace offline_items_collection {
class OfflineContentAggregator;
}  // namespace offline_items_collection

class Profile;
class SiteEngagementService;

class ContentIndexProviderImpl
    : public KeyedService,
      public offline_items_collection::OfflineContentProvider,
      public content::ContentIndexProvider {
 public:
  explicit ContentIndexProviderImpl(Profile* profile);
  ~ContentIndexProviderImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // ContentIndexProvider implementation.
  std::vector<gfx::Size> GetIconSizes(
      blink::mojom::ContentCategory category) override;
  void OnContentAdded(content::ContentIndexEntry entry) override;
  void OnContentDeleted(int64_t service_worker_registration_id,
                        const url::Origin& origin,
                        const std::string& description_id) override;

  // OfflineContentProvider implementation.
  void OpenItem(offline_items_collection::LaunchLocation location,
                const offline_items_collection::ContentId& id) override;
  void RemoveItem(const offline_items_collection::ContentId& id) override;
  void CancelDownload(const offline_items_collection::ContentId& id) override;
  void PauseDownload(const offline_items_collection::ContentId& id) override;
  void ResumeDownload(const offline_items_collection::ContentId& id,
                      bool has_user_gesture) override;
  void GetItemById(const offline_items_collection::ContentId& id,
                   SingleItemCallback callback) override;
  void GetAllItems(MultipleItemCallback callback) override;
  void GetVisualsForItem(const offline_items_collection::ContentId& id,
                         GetVisualsOptions options,
                         VisualsCallback callback) override;
  void GetShareInfoForItem(const offline_items_collection::ContentId& id,
                           ShareCallback callback) override;
  void RenameItem(const offline_items_collection::ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void SetIconSizesForTesting(std::vector<gfx::Size> icon_sizes) {
    icon_sizes_for_testing_ = std::move(icon_sizes);
  }

 private:
  void DidGetItem(SingleItemCallback callback,
                  base::Optional<content::ContentIndexEntry> entry);
  void DidGetAllEntriesAcrossStorageParitions(
      std::unique_ptr<OfflineItemList> item_list,
      MultipleItemCallback callback);
  void DidGetAllEntries(base::OnceClosure done_closure,
                        OfflineItemList* item_list,
                        blink::mojom::ContentIndexError error,
                        std::vector<content::ContentIndexEntry> entries);
  void DidGetIcons(const offline_items_collection::ContentId& id,
                   VisualsCallback callback,
                   std::vector<SkBitmap> icons);
  void DidGetEntryToOpen(base::Optional<content::ContentIndexEntry> entry);
  void DidOpenTab(content::ContentIndexEntry entry,
                  content::WebContents* web_contents);
  offline_items_collection::OfflineItem EntryToOfflineItem(
      const content::ContentIndexEntry& entry);

  Profile* profile_;
  ContentIndexMetrics metrics_;
  offline_items_collection::OfflineContentAggregator* aggregator_;
  SiteEngagementService* site_engagement_service_;
  base::ObserverList<Observer>::Unchecked observers_;
  base::Optional<std::vector<gfx::Size>> icon_sizes_for_testing_;
  base::WeakPtrFactory<ContentIndexProviderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentIndexProviderImpl);
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_
