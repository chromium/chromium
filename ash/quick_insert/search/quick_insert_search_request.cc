// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_search_request.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_client.h"
#include "ash/quick_insert/quick_insert_clipboard_history_provider.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/quick_insert_action_search.h"
#include "ash/quick_insert/search/quick_insert_date_search.h"
#include "ash/quick_insert/search/quick_insert_editor_search.h"
#include "ash/quick_insert/search/quick_insert_lobster_search.h"
#include "ash/quick_insert/search/quick_insert_math_search.h"
#include "ash/quick_insert/search/quick_insert_search_source.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/parameter_pack.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "url/gurl.h"

namespace ash {

namespace {

// TODO: b/330936766 - Prioritise "earlier" domains in this list.
constexpr auto kGoogleCorpGotoHosts = base::MakeFixedFlatSet<std::string_view>(
    {"goto2.corp.google.com", "goto.corp.google.com", "goto.google.com", "go"});

const char* SearchSourceToHistogram(QuickInsertSearchSource source) {
  switch (source) {
    case QuickInsertSearchSource::kOmnibox:
      return "Ash.Picker.Search.OmniboxProvider.QueryTime";
    case QuickInsertSearchSource::kDate:
      return "Ash.Picker.Search.DateProvider.QueryTime";
    case QuickInsertSearchSource::kAction:
      return "Ash.Picker.Search.CategoryProvider.QueryTime";
    case QuickInsertSearchSource::kLocalFile:
      return "Ash.Picker.Search.FileProvider.QueryTime";
    case QuickInsertSearchSource::kDrive:
      return "Ash.Picker.Search.DriveProvider.QueryTime";
    case QuickInsertSearchSource::kMath:
      return "Ash.Picker.Search.MathProvider.QueryTime";
    case QuickInsertSearchSource::kClipboard:
      return "Ash.Picker.Search.ClipboardProvider.QueryTime";
    case QuickInsertSearchSource::kEditorWrite:
    case QuickInsertSearchSource::kEditorRewrite:
      return "Ash.Picker.Search.EditorProvider.QueryTime";
    case QuickInsertSearchSource::kLobsterWithNoSelectedText:
    case QuickInsertSearchSource::kLobsterWithSelectedText:
      return "Ash.Picker.Search.LobsterProvider.QueryTime";
  }
  NOTREACHED() << "Unexpected search source " << base::to_underlying(source);
}

[[nodiscard]] std::vector<QuickInsertSearchResult>
DeduplicateGoogleCorpGotoDomains(
    std::vector<QuickInsertSearchResult> omnibox_results) {
  std::set<std::string, std::less<>> seen;
  std::vector<QuickInsertSearchResult> deduped_results;
  std::vector<QuickInsertSearchResult*> results_to_remove;

  for (QuickInsertSearchResult& link : omnibox_results) {
    auto* link_data = std::get_if<QuickInsertBrowsingHistoryResult>(&link);
    if (link_data == nullptr) {
      deduped_results.push_back(std::move(link));
      continue;
    }
    const GURL& url = link_data->url;
    if (!url.has_host() || !url.has_path() ||
        !kGoogleCorpGotoHosts.contains(url.host_piece())) {
      deduped_results.push_back(std::move(link));
      continue;
    }

    auto [it, inserted] = seen.emplace(url.path_piece());
    if (inserted) {
      deduped_results.push_back(std::move(link));
    }
  }

  return deduped_results;
}

}  // namespace

QuickInsertSearchRequest::QuickInsertSearchRequest(
    std::u16string_view query,
    std::optional<QuickInsertCategory> category,
    SearchResultsCallback callback,
    DoneCallback done_callback,
    QuickInsertClient* client,
    base::span<const QuickInsertCategory> available_categories,
    bool caps_lock_state_to_search,
    bool search_case_transforms)
    : is_category_specific_search_(category.has_value()),
      client_(CHECK_DEREF(client)),
      current_callback_(std::move(callback)),
      done_callback_(std::move(done_callback)) {
  CHECK(!current_callback_.is_null());
  CHECK(!done_callback_.is_null());
  std::string utf8_query = base::UTF16ToUTF8(query);

  std::vector<QuickInsertSearchSource> cros_search_sources;
  cros_search_sources.reserve(3);
  if ((!category.has_value() || category == QuickInsertCategory::kLinks) &&
      base::Contains(available_categories, QuickInsertCategory::kLinks)) {
    cros_search_sources.push_back(QuickInsertSearchSource::kOmnibox);
  }
  if ((!category.has_value() || category == QuickInsertCategory::kLocalFiles) &&
      base::Contains(available_categories, QuickInsertCategory::kLocalFiles)) {
    cros_search_sources.push_back(QuickInsertSearchSource::kLocalFile);
  }
  if ((!category.has_value() || category == QuickInsertCategory::kDriveFiles) &&
      base::Contains(available_categories, QuickInsertCategory::kDriveFiles)) {
    cros_search_sources.push_back(QuickInsertSearchSource::kDrive);
  }

  if (!cros_search_sources.empty()) {
    // TODO: b/326166751 - Use `available_categories_` to decide what searches
    // to do.
    for (QuickInsertSearchSource source : cros_search_sources) {
      MarkSearchStarted(source);
    }
    client_->StartCrosSearch(
        std::u16string(query), category,
        base::BindRepeating(&QuickInsertSearchRequest::HandleCrosSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if ((!category.has_value() || category == QuickInsertCategory::kClipboard) &&
      base::Contains(available_categories, QuickInsertCategory::kClipboard)) {
    clipboard_provider_ = std::make_unique<PickerClipboardHistoryProvider>();
    MarkSearchStarted(QuickInsertSearchSource::kClipboard);
    clipboard_provider_->FetchResults(
        base::BindOnce(&QuickInsertSearchRequest::HandleClipboardSearchResults,
                       weak_ptr_factory_.GetWeakPtr()),
        query);
  }

  if ((!category.has_value() || category == QuickInsertCategory::kDatesTimes) &&
      base::Contains(available_categories, QuickInsertCategory::kDatesTimes)) {
    MarkSearchStarted(QuickInsertSearchSource::kDate);
    // Date results is currently synchronous.
    HandleDateSearchResults(PickerDateSearch(base::Time::Now(), query));
  }

  if ((!category.has_value() || category == QuickInsertCategory::kUnitsMaths) &&
      base::Contains(available_categories, QuickInsertCategory::kUnitsMaths)) {
    MarkSearchStarted(QuickInsertSearchSource::kMath);
    // Math results is currently synchronous.
    HandleMathSearchResults(PickerMathSearch(query));
  }

  // These searches do not have category-specific search.
  if (!category.has_value()) {
    MarkSearchStarted(QuickInsertSearchSource::kAction);
    // Action results are currently synchronous.
    HandleActionSearchResults(
        PickerActionSearch(available_categories, caps_lock_state_to_search,
                           search_case_transforms, query));

    if (base::Contains(available_categories,
                       QuickInsertCategory::kEditorWrite)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(QuickInsertSearchSource::kEditorWrite);
      HandleEditorSearchResults(
          QuickInsertSearchSource::kEditorWrite,
          PickerEditorSearch(QuickInsertEditorResult::Mode::kWrite, query));
    }

    if (base::Contains(available_categories,
                       QuickInsertCategory::kEditorRewrite)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(QuickInsertSearchSource::kEditorRewrite);
      HandleEditorSearchResults(
          QuickInsertSearchSource::kEditorRewrite,
          PickerEditorSearch(QuickInsertEditorResult::Mode::kRewrite, query));
    }

    if (base::Contains(available_categories,
                       QuickInsertCategory::kLobsterWithNoSelectedText)) {
      // Lobster results are currently synchronous.
      MarkSearchStarted(QuickInsertSearchSource::kLobsterWithNoSelectedText);
      HandleLobsterSearchResults(
          QuickInsertSearchSource::kLobsterWithNoSelectedText,
          PickerLobsterSearch(QuickInsertLobsterResult::Mode::kNoSelection,
                              query));
    }

    if (base::Contains(available_categories,
                       QuickInsertCategory::kLobsterWithSelectedText)) {
      // Lobster results are currently synchronous.
      MarkSearchStarted(QuickInsertSearchSource::kLobsterWithSelectedText);
      HandleLobsterSearchResults(
          QuickInsertSearchSource::kLobsterWithSelectedText,
          PickerLobsterSearch(QuickInsertLobsterResult::Mode::kWithSelection,
                              query));
    }
  }

  can_call_done_closure_ = true;
  MaybeCallDoneClosure();
}

QuickInsertSearchRequest::~QuickInsertSearchRequest() {
  // Ensure that any bound callbacks to `Handle*SearchResults` - and therefore
  // `current_callback_` - will not get called by stopping searches.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!done_callback_.is_null()) {
    std::move(done_callback_).Run(/*interrupted=*/true);
    current_callback_.Reset();
  }
  client_->StopCrosQuery();
}

void QuickInsertSearchRequest::HandleSearchSourceResults(
    QuickInsertSearchSource source,
    std::vector<QuickInsertSearchResult> results,
    bool has_more_results) {
  MarkSearchEnded(source);
  // This method is only called from `Handle*SearchResults` methods (one for
  // each search source), and the only time `current_callback_` is null is when
  // this request is being destructed, or `done_closure_` was called.
  // The destructor invalidates any bound callbacks to `Handle*SearchResults`
  // before resetting the callback to null. If `done_closure_` was called, and
  // more calls would have occurred, this is a bug and we should noisly crash.
  CHECK(!current_callback_.is_null())
      << "Current callback is null in HandleSearchSourceResults";
  current_callback_.Run(source, std::move(results), has_more_results);
  MaybeCallDoneClosure();
}

void QuickInsertSearchRequest::HandleActionSearchResults(
    std::vector<QuickInsertSearchResult> results) {
  HandleSearchSourceResults(QuickInsertSearchSource::kAction,
                            std::move(results),
                            /*has_more_results*/ false);
}

void QuickInsertSearchRequest::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<QuickInsertSearchResult> results) {
  switch (type) {
    case AppListSearchResultType::kOmnibox: {
      results = DeduplicateGoogleCorpGotoDomains(std::move(results));
      size_t results_to_remove = is_category_specific_search_
                                     ? 0
                                     : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - results_to_remove, results.end());

      HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                std::move(results),
                                /*has_more_results=*/results_to_remove > 0);
      break;
    }
    case AppListSearchResultType::kDriveSearch: {
      size_t files_to_remove = is_category_specific_search_
                                   ? 0
                                   : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(QuickInsertSearchSource::kDrive,
                                std::move(results),
                                /*has_more_results=*/files_to_remove > 0);
      break;
    }
    case AppListSearchResultType::kFileSearch: {
      size_t files_to_remove = is_category_specific_search_
                                   ? 0
                                   : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(QuickInsertSearchSource::kLocalFile,
                                std::move(results),
                                /*has_more_results=*/files_to_remove > 0);
      break;
    }
    default:
      LOG(DFATAL) << "Got unexpected search result type "
                  << static_cast<int>(type);
      break;
  }
}

void QuickInsertSearchRequest::HandleDateSearchResults(
    std::vector<QuickInsertSearchResult> results) {
  // Date results are never truncated.
  HandleSearchSourceResults(QuickInsertSearchSource::kDate, std::move(results),
                            /*has_more_results=*/false);
}

void QuickInsertSearchRequest::HandleMathSearchResults(
    std::optional<QuickInsertSearchResult> result) {
  std::vector<QuickInsertSearchResult> results;
  if (result.has_value()) {
    results.push_back(*std::move(result));
  }

  // Math results are never truncated.
  HandleSearchSourceResults(QuickInsertSearchSource::kMath, std::move(results),
                            /*has_more_results=*/false);
}

void QuickInsertSearchRequest::HandleClipboardSearchResults(
    std::vector<QuickInsertSearchResult> results) {
  // Clipboard results are never truncated.
  HandleSearchSourceResults(QuickInsertSearchSource::kClipboard,
                            std::move(results),
                            /*has_more_results=*/false);
}

void QuickInsertSearchRequest::HandleEditorSearchResults(
    QuickInsertSearchSource source,
    std::optional<QuickInsertSearchResult> result) {
  std::vector<QuickInsertSearchResult> results;
  if (result.has_value()) {
    results.push_back(std::move(*result));
  }

  // Editor results are never truncated.
  HandleSearchSourceResults(source, std::move(results),
                            /*has_more_results=*/false);
}

void QuickInsertSearchRequest::HandleLobsterSearchResults(
    QuickInsertSearchSource source,
    std::optional<QuickInsertSearchResult> result) {
  std::vector<QuickInsertSearchResult> results;
  if (result.has_value()) {
    results.push_back(std::move(*result));
  }

  // Lobster results are never truncated.
  HandleSearchSourceResults(source, std::move(results),
                            /*has_more_results=*/false);
}

void QuickInsertSearchRequest::MarkSearchStarted(
    QuickInsertSearchSource source) {
  CHECK(!SwapSearchStart(source, base::TimeTicks::Now()).has_value())
      << "search_starts_ enum " << base::to_underlying(source)
      << " was already set";
}

void QuickInsertSearchRequest::MarkSearchEnded(QuickInsertSearchSource source) {
  std::optional<base::TimeTicks> start = SwapSearchStart(source, std::nullopt);
  CHECK(start.has_value()) << "search_starts_ enum "
                           << base::to_underlying(source) << " was not set";

  base::TimeDelta elapsed = base::TimeTicks::Now() - *start;
  base::UmaHistogramTimes(SearchSourceToHistogram(source), elapsed);
}

std::optional<base::TimeTicks> QuickInsertSearchRequest::SwapSearchStart(
    QuickInsertSearchSource source,
    std::optional<base::TimeTicks> new_value) {
  return std::exchange(search_starts_[base::to_underlying(source)],
                       std::move(new_value));
}

void QuickInsertSearchRequest::MaybeCallDoneClosure() {
  if (!can_call_done_closure_) {
    return;
  }
  if (base::ranges::any_of(search_starts_,
                           [](std::optional<base::TimeTicks>& start) {
                             return start.has_value();
                           })) {
    return;
  }

  std::move(done_callback_).Run(/*interrupted=*/false);
  current_callback_.Reset();
}

}  // namespace ash
