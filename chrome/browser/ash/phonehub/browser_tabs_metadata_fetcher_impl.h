// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_IMPL_H_
#define CHROME_BROWSER_ASH_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_IMPL_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace favicon {
class HistoryUiFaviconRequestHandler;
}  // namespace favicon

namespace gfx {
class ImageSkia;
}  // namespace gfx
namespace ash {

struct ForeignSyncedSessionAsh;
class SyncedSessionClientAsh;

namespace phonehub {

// BrowserTabsMetadataFetcher implementation. First, a vector containing
// metadata of the most recently visited tab to least recently visited is
// created. The metadata is stored from data provided by a SyncedSession. After
// the ordered vector is created, the HistoryUiFaviconRequestHandler is used to
// asynchronously fetch favicon images for the most recently visited tabs. Once
// all the favicons for the most recently visited tabs (up to
// BrowserTabsModel::kMaxMostRecentTabs) have been fetched, |results_| is
// invoked with the callback passed to the class.
class BrowserTabsMetadataFetcherImpl : public BrowserTabsMetadataFetcher {
 public:
  explicit BrowserTabsMetadataFetcherImpl(
      favicon::HistoryUiFaviconRequestHandler* favicon_request_handler);
  ~BrowserTabsMetadataFetcherImpl() override;

  // BrowserTabsMetadataFetcher:
  void Fetch(
      const sync_sessions::SyncedSession* session,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) override;
  void FetchForeignSyncedPhoneSessionMetadata(
      const ForeignSyncedSessionAsh& session,
      SyncedSessionClientAsh* synced_session_client_ash,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) override;

 private:
  void OnAllFaviconsFetched();
  void OnAllForeignSyncedSessionFaviconsFetched(
      std::vector<BrowserTabsModel::BrowserTabMetadata> results,
      base::OnceCallback<void(BrowserTabsMetadataResponse)> callback);
  void OnFaviconReady(
      size_t index_in_results,
      base::OnceClosure done_closure,
      const favicon_base::FaviconImageResult& favicon_image_result);
  void OnForeignSyncedSessionFaviconReady(
      BrowserTabsModel::BrowserTabMetadata* tab,
      base::OnceClosure done_closure,
      const gfx::ImageSkia& favicon);

  const raw_ptr<favicon::HistoryUiFaviconRequestHandler, ExperimentalAsh>
      favicon_request_handler_;
  std::vector<BrowserTabsModel::BrowserTabMetadata> results_;
  base::OnceCallback<void(BrowserTabsMetadataResponse)> callback_;

  base::WeakPtrFactory<BrowserTabsMetadataFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PHONEHUB_BROWSER_TABS_METADATA_FETCHER_IMPL_H_
