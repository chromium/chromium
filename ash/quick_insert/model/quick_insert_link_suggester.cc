// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_link_suggester.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/barrier_callback.h"
#include "base/strings/string_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace {

constexpr int kRecentDayRange = 7;

// Returns true if the given link is likely to be personalized to the user,
// which makes it unlikely that the URL works as intended when shared.
bool IsLinkLikelyPersonalized(const GURL& url) {
  // TODO: b/366237507 - Add more domains.
  static constexpr std::pair<std::string_view, std::string_view> kBlocklist[] =
      {
          {"mail.google.com", "/chat/"},
          {"mail.google.com", "/mail/"},
      };

  for (const auto& [domain, path_prefix] : kBlocklist) {
    if (url.DomainIs(domain) && base::StartsWith(url.GetPath(), path_prefix)) {
      return true;
    }
  }
  return false;
}

}  // namespace

QuickInsertLinkSuggester::QuickInsertLinkSuggester() = default;

QuickInsertLinkSuggester::~QuickInsertLinkSuggester() = default;

void QuickInsertLinkSuggester::GetSuggestedLinks(
    history::HistoryService* history_service,
    favicon::FaviconService* favicon_service,
    size_t max_links,
    SuggestedLinksCallback callback) {
  CHECK(history_service);
  history::QueryOptions options;
  options.max_count = max_links;
  options.policy_for_404_visits = history::VisitQuery404sPolicy::kExclude404s;
  options.SetRecentDayRange(kRecentDayRange);
  history_service->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&QuickInsertLinkSuggester::OnGetBrowsingHistory,
                     weak_factory_.GetWeakPtr(), favicon_service,
                     std::move(callback)),
      &history_query_tracker_);
}

void QuickInsertLinkSuggester::OnGetBrowsingHistory(
    favicon::FaviconService* favicon_service,
    SuggestedLinksCallback callback,
    history::QueryResults results) {
  std::vector<history::URLResult> filtered_results;
  std::ranges::copy_if(results, std::back_inserter(filtered_results),
                       [](const history::URLResult result) {
                         if (IsLinkLikelyPersonalized(result.url())) {
                           return false;
                         }
                         return result.url().SchemeIsHTTPOrHTTPS();
                       });

  if (favicon_service) {
    favicon_query_trackers_ =
        std::vector<base::CancelableTaskTracker>(filtered_results.size());
    auto barrier_callback = base::BarrierCallback<ash::QuickInsertSearchResult>(
        /*num_callbacks=*/filtered_results.size(),
        /*done_callback=*/std::move(callback));

    for (size_t i = 0; i < filtered_results.size(); ++i) {
      favicon_service->GetFaviconImageForPageURL(
          filtered_results[i].url(),
          base::BindOnce(&QuickInsertLinkSuggester::OnGetFaviconImage,
                         weak_factory_.GetWeakPtr(), filtered_results[i],
                         barrier_callback),
          &favicon_query_trackers_[i]);
    }
  } else {
    // Fallback to placeholder icon if favicon service is not available.
    std::vector<ash::QuickInsertSearchResult> quick_insert_search_results;
    for (const auto& result : filtered_results) {
      quick_insert_search_results.push_back(
          ash::QuickInsertBrowsingHistoryResult(
              result.url(), result.title(),
              ui::ImageModel::FromVectorIcon(ash::kOmniboxGenericIcon,
                                             cros_tokens::kCrosSysOnSurface),
              false));
    }
    std::move(callback).Run(quick_insert_search_results);
  }
}

void QuickInsertLinkSuggester::OnGetFaviconImage(
    history::URLResult result,
    base::OnceCallback<void(ash::QuickInsertSearchResult)> callback,
    const favicon_base::FaviconImageResult& favicon_image_result) {
  std::move(callback).Run(ash::QuickInsertBrowsingHistoryResult(
      result.url(), result.title(),
      favicon_image_result.image.IsEmpty()
          ? ui::ImageModel::FromVectorIcon(ash::kOmniboxGenericIcon,
                                           cros_tokens::kCrosSysOnSurface)
          : ui::ImageModel::FromImage(favicon_image_result.image),
      false));
}
