// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"
#include "chrome/browser/android/explore_sites/image_helper.h"
#include "components/offline_pages/task/task_queue.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using offline_pages::TaskQueue;

namespace explore_sites {

class ExploreSitesServiceImpl : public ExploreSitesService,
                                public TaskQueue::Delegate {
 public:
  // Helper class used to inject a dependency that can later vend a
  // URLLoaderFactory. URLLoaderFactory fetching requires fully initialized
  // Profile which is generally available later than service creation time.
  class URLLoaderFactoryGetter {
   public:
    virtual ~URLLoaderFactoryGetter() {}
    virtual scoped_refptr<network::SharedURLLoaderFactory> GetFactory() = 0;
  };

  ExploreSitesServiceImpl(
      std::unique_ptr<ExploreSitesStore> store,
      std::unique_ptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      std::unique_ptr<HistoryStatisticsReporter> history_statistics_reporter);
  ~ExploreSitesServiceImpl() override;

  static bool IsExploreSitesEnabled();

  // ExploreSitesService implementation.
  void GetCatalog(CatalogCallback callback) override;
  void GetCategoryImage(int category_id,
                        int pixel_size,
                        BitmapCallback callback) override;
  void GetSummaryImage(int pixel_size, BitmapCallback callback) override;
  void GetSiteImage(int site_id, BitmapCallback callback) override;
  void UpdateCatalogFromNetwork(bool is_immediate_fetch,
                                const std::string& accept_languages,
                                BooleanCallback callback) override;
  void RecordClick(const std::string& url, int category_type) override;
  void BlacklistSite(const std::string& url) override;
  void ClearActivities(base::Time begin,
                       base::Time end,
                       base::OnceClosure callback) override;
  void IncrementNtpShownCount(int category_id) override;
  void ClearCachedCatalogsForDebugging() override;
  void OverrideCountryCodeForDebugging(
      const std::string& country_code) override;
  std::string GetCountryCode() override;

  // Test hook to call the OnCatalogFetched method since we can't pass nullptr
  // through the test fetcher code.
  void OnCatalogFetchedForTest(
      ExploreSitesRequestStatus status,
      std::unique_ptr<std::string> serialized_protobuf);

 private:
  // KeyedService implementation:
  void Shutdown() override;

  // TaskQueue::Delegate implementation:
  void OnTaskQueueIsIdle() override;

  // Callback that is run when we have the catalog version.
  void GotVersionToStartFetch(bool is_immediate_fetch,
                              const std::string& accept_languages,
                              std::string catalog_version);

  // Callback returning from the UpdateCatalogFromNetwork operation.  It
  // passes along the call back to the bridge and eventually back to Java land.
  void OnCatalogFetched(ExploreSitesRequestStatus status,
                        std::unique_ptr<std::string> serialized_protobuf);

  void NotifyCatalogUpdated(std::vector<BooleanCallback> callbacks,
                            bool success);

  // Wrappers to call ImageHelper::Compose[Site|Category]Image.
  void ComposeSiteImage(BitmapCallback callback, EncodedImageList images);
  void ComposeCategoryImage(BitmapCallback callback,
                            int pixel_size,
                            EncodedImageList images);

  std::unique_ptr<std::string> country_override_;
  ImageHelper image_helper_;

  // Used to control access to the ExploreSitesStore.
  TaskQueue task_queue_;
  std::unique_ptr<ExploreSitesStore> explore_sites_store_;
  std::unique_ptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  std::unique_ptr<ExploreSitesFetcher> explore_sites_fetcher_;
  std::unique_ptr<HistoryStatisticsReporter> history_statistics_reporter_;
  std::vector<BooleanCallback> update_catalog_callbacks_;
  base::WeakPtrFactory<ExploreSitesServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceImpl);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
