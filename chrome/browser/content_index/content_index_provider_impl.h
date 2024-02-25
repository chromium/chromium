// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

namespace site_engagement {
class SiteEngagementService;
}

class Profile;

class ContentIndexProviderImpl
    : public KeyedService,
      public offline_items_collection::OfflineContentProvider,
      public content::ContentIndexProvider {
 public:
  static const char kProviderNamespace[];

  explicit ContentIndexProviderImpl(Profile* profile);

  ContentIndexProviderImpl(const ContentIndexProviderImpl&) = delete;
  ContentIndexProviderImpl& operator=(const ContentIndexProviderImpl&) = delete;

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
  void OpenItem(const offline_items_collection::OpenParams& open_params,
                const offline_items_collection::ContentId& id) override;
  void RemoveItem(const offline_items_collection::ContentId& id) override;
  void CancelDownload(const offline_items_collection::ContentId& id) override;
  void PauseDownload(const offline_items_collection::ContentId& id) override;
  void ResumeDownload(const offline_items_collection::ContentId& id) override;
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

  void SetIconSizesForTesting(std::vector<gfx::Size> icon_sizes) {
    icon_sizes_for_testing_ = std::move(icon_sizes);
  }

 private:
  void DidGetItem(SingleItemCallback callback,
                  std::optional<content::ContentIndexEntry> entry);
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
  void DidGetEntryToOpen(std::optional<content::ContentIndexEntry> entry);
  void DidOpenTab(content::ContentIndexEntry entry,
                  content::WebContents* web_contents);
  offline_items_collection::OfflineItem EntryToOfflineItem(
      const content::ContentIndexEntry& entry);

  raw_ptr<Profile> profile_;
  ContentIndexMetrics metrics_;
  raw_ptr<offline_items_collection::OfflineContentAggregator> aggregator_;
  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;
  std::optional<std::vector<gfx::Size>> icon_sizes_for_testing_;
  base::WeakPtrFactory<ContentIndexProviderImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_
