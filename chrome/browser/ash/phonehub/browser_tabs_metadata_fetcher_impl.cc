// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/phonehub/browser_tabs_metadata_fetcher_impl.h"

#include "base/barrier_closure.h"
#include "base/time/time.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"
#include "components/sync_sessions/synced_session.h"
#include "components/ukm/scheme_constants.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {
namespace {

std::vector<BrowserTabsModel::BrowserTabMetadata>
GetSortedMetadataWithoutFavicons(const sync_sessions::SyncedSession* session) {
  std::vector<BrowserTabsModel::BrowserTabMetadata> browser_tab_metadata;

  using WindowPair =
      std::pair<const SessionID,
                std::unique_ptr<sync_sessions::SyncedSessionWindow>>;
  for (const WindowPair& window_pair : session->windows) {
    const sessions::SessionWindow& window = window_pair.second->wrapped_window;
    for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
      int selected_index = tab->normalized_navigation_index();

      if (selected_index + 1 > static_cast<int>(tab->navigations.size()) ||
          tab->navigations.empty()) {
        continue;
      }

      const sessions::SerializedNavigationEntry& current_navigation =
          tab->navigations.at(selected_index);

      GURL tab_url = current_navigation.virtual_url();

      // URLs whose schemes are not http:// or https:// should be ignored
      // because they may be platform specific (e.g., chrome:// URLs) or may
      // refer to local media on the phone (e.g., content:// URLs).
      if (!tab_url.SchemeIsHTTPOrHTTPS()) {
        continue;
      }

      // If the url is incorrectly formatted, is empty, or has a
      // scheme that should be omitted, do not proceed with storing its
      // metadata.
      if (!tab_url.is_valid()) {
        continue;
      }

      const std::u16string& title = current_navigation.title();
      const base::Time last_accessed_timestamp = tab->timestamp;
      browser_tab_metadata.emplace_back(tab_url, title, last_accessed_timestamp,
                                        gfx::Image());
    }
  }

  // Sorts the |browser_tab_metadata| from most recently visited to least
  // recently visited.
  std::sort(browser_tab_metadata.begin(), browser_tab_metadata.end());

  // At most |kMaxMostRecentTabs| tab metadata can be displayed.
  size_t num_tabs_to_display = std::min(browser_tab_metadata.size(),
                                        BrowserTabsModel::kMaxMostRecentTabs);
  return std::vector<BrowserTabsModel::BrowserTabMetadata>(
      browser_tab_metadata.begin(),
      browser_tab_metadata.begin() + num_tabs_to_display);
}

}  // namespace

BrowserTabsMetadataFetcherImpl::BrowserTabsMetadataFetcherImpl(
    favicon::HistoryUiFaviconRequestHandler* favicon_request_handler)
    : favicon_request_handler_(favicon_request_handler) {}

BrowserTabsMetadataFetcherImpl::~BrowserTabsMetadataFetcherImpl() = default;

void BrowserTabsMetadataFetcherImpl::Fetch(
    const sync_sessions::SyncedSession* session,
    base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) {
  // A new fetch was made, return a std::nullopt to the previous |callback_|.
  if (!callback_.is_null()) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(std::nullopt);
  }

  results_ = GetSortedMetadataWithoutFavicons(session);
  callback_ = std::move(callback);

  // When |barrier| is run |num_tabs_to_display| times, it will run
  // |OnAllFaviconsFetched|.
  base::RepeatingClosure barrier = base::BarrierClosure(
      results_.size(),
      base::BindOnce(&BrowserTabsMetadataFetcherImpl::OnAllFaviconsFetched,
                     weak_ptr_factory_.GetWeakPtr()));

  for (size_t i = 0; i < results_.size(); ++i) {
    favicon_request_handler_->GetFaviconImageForPageURL(
        results_[i].url,
        base::BindOnce(&BrowserTabsMetadataFetcherImpl::OnFaviconReady,
                       weak_ptr_factory_.GetWeakPtr(), i, barrier),
        favicon::HistoryUiFaviconRequestOrigin::kRecentTabs);
  }
}

void BrowserTabsMetadataFetcherImpl::OnAllFaviconsFetched() {
  std::move(callback_).Run(std::move(results_));
}

void BrowserTabsMetadataFetcherImpl::OnFaviconReady(
    size_t index_in_results,
    base::OnceClosure done_closure,
    const favicon_base::FaviconImageResult& favicon_image_result) {
  DCHECK(index_in_results < results_.size());

  results_[index_in_results].favicon = std::move(favicon_image_result.image);
  std::move(done_closure).Run();
}

}  // namespace phonehub
}  // namespace ash
