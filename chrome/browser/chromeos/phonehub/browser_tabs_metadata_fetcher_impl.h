// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chromeos/components/phonehub/browser_tabs_metadata_fetcher.h"

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace chromeos {
namespace phonehub {

// BrowserTabsMetadataFetcher implementation. First, a vector containing
// metadata of the most recently visited tab to least recently visited is
// created. The metadata is stored from data provided by a SyncedSession. After
// the ordered vector is created, the FaviconService is used to asynchronously
// fetch favicon images for the most recently visited tabs. Once all the
// favicons for the most recently visited tabs (up to
// BrowserTabsModel::kMaxMostRecentTabs) have been fetched, |results_| is
// invoked with the callback passed to the class.
class BrowserTabsMetadataFetcherImpl : public BrowserTabsMetadataFetcher {
 public:
  explicit BrowserTabsMetadataFetcherImpl(
      favicon::FaviconService* favicon_service);
  ~BrowserTabsMetadataFetcherImpl() override;

  // BrowserTabsMetadataFetcher:
  void Fetch(
      const sync_sessions::SyncedSession* session,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) override;

 private:
  void OnAllFaviconsFetched();
  void OnFaviconReady(
      size_t index_in_results,
      base::OnceClosure done_closure,
      const favicon_base::FaviconImageResult& favicon_image_result);

  favicon::FaviconService* const favicon_service_;
  std::vector<BrowserTabsModel::BrowserTabMetadata> results_;
  base::OnceCallback<void(BrowserTabsMetadataResponse)> callback_;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  base::WeakPtrFactory<BrowserTabsMetadataFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_IMPL_H_
