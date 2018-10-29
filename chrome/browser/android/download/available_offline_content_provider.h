// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/available_offline_content.mojom.h"

namespace content {
class BrowserContext;
}

namespace offline_items_collection {
class OfflineContentAggregator;
struct OfflineItem;
}  // namespace offline_items_collection

namespace android {

// Provides access to items available while offline.
class AvailableOfflineContentProvider
    : public chrome::mojom::AvailableOfflineContentProvider {
 public:
  // Public for testing.
  explicit AvailableOfflineContentProvider(
      content::BrowserContext* browser_context);
  ~AvailableOfflineContentProvider() override;

  // chrome::mojom::AvailableOfflineContentProvider methods.
  void List(ListCallback callback) override;
  void Summarize(SummarizeCallback) override;
  void LaunchItem(const std::string& item_id,
                  const std::string& name_space) override;
  void LaunchDownloadsPage(bool open_prefetched_articles_tab) override;

  static void Create(
      content::BrowserContext* browser_context,
      chrome::mojom::AvailableOfflineContentProviderRequest request);

 private:
  void SummarizeFinalize(
      AvailableOfflineContentProvider::SummarizeCallback callback,
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  void ListFinalize(
      AvailableOfflineContentProvider::ListCallback callback,
      offline_items_collection::OfflineContentAggregator* aggregator,
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  content::BrowserContext* browser_context_;

  base::WeakPtrFactory<AvailableOfflineContentProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AvailableOfflineContentProvider);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_
