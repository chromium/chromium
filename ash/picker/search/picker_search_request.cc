// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_request.h"

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

#include "ash/picker/picker_category.h"
#include "ash/picker/picker_client.h"
#include "ash/picker/picker_clipboard_history_provider.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/picker_action_search.h"
#include "ash/picker/search/picker_date_search.h"
#include "ash/picker/search/picker_editor_search.h"
#include "ash/picker/search/picker_lobster_search.h"
#include "ash/picker/search/picker_math_search.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/public/cpp/app_list/app_list_types.h"
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

const char* SearchSourceToHistogram(PickerSearchSource source) {
  switch (source) {
    case PickerSearchSource::kOmnibox:
      return "Ash.Picker.Search.OmniboxProvider.QueryTime";
    case PickerSearchSource::kDate:
      return "Ash.Picker.Search.DateProvider.QueryTime";
    case PickerSearchSource::kAction:
      return "Ash.Picker.Search.CategoryProvider.QueryTime";
    case PickerSearchSource::kLocalFile:
      return "Ash.Picker.Search.FileProvider.QueryTime";
    case PickerSearchSource::kDrive:
      return "Ash.Picker.Search.DriveProvider.QueryTime";
    case PickerSearchSource::kMath:
      return "Ash.Picker.Search.MathProvider.QueryTime";
    case PickerSearchSource::kClipboard:
      return "Ash.Picker.Search.ClipboardProvider.QueryTime";
    case PickerSearchSource::kEditorWrite:
    case PickerSearchSource::kEditorRewrite:
      return "Ash.Picker.Search.EditorProvider.QueryTime";
    case PickerSearchSource::kLobster:
      return "Ash.Picker.Search.LobsterProvider.QueryTime";
  }
  NOTREACHED() << "Unexpected search source " << base::to_underlying(source);
}

[[nodiscard]] std::vector<PickerSearchResult> DeduplicateGoogleCorpGotoDomains(
    std::vector<PickerSearchResult> omnibox_results) {
  std::set<std::string, std::less<>> seen;
  std::vector<PickerSearchResult> deduped_results;
  std::vector<PickerSearchResult*> results_to_remove;

  for (PickerSearchResult& link : omnibox_results) {
    auto* link_data = std::get_if<PickerBrowsingHistoryResult>(&link);
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

PickerSearchRequest::PickerSearchRequest(std::u16string_view query,
                                         std::optional<PickerCategory> category,
                                         SearchResultsCallback callback,
                                         DoneCallback done_callback,
                                         PickerClient* client,
                                         const Options& options)
    : is_category_specific_search_(category.has_value()),
      client_(CHECK_DEREF(client)),
      current_callback_(std::move(callback)),
      done_callback_(std::move(done_callback)) {
  CHECK(!current_callback_.is_null());
  CHECK(!done_callback_.is_null());
  base::span<const PickerCategory> available_categories =
      options.available_categories;
  std::string utf8_query = base::UTF16ToUTF8(query);

  std::vector<PickerSearchSource> cros_search_sources;
  cros_search_sources.reserve(3);
  if ((!category.has_value() || category == PickerCategory::kLinks) &&
      base::Contains(available_categories, PickerCategory::kLinks)) {
    cros_search_sources.push_back(PickerSearchSource::kOmnibox);
  }
  if ((!category.has_value() || category == PickerCategory::kLocalFiles) &&
      base::Contains(available_categories, PickerCategory::kLocalFiles)) {
    cros_search_sources.push_back(PickerSearchSource::kLocalFile);
  }
  if ((!category.has_value() || category == PickerCategory::kDriveFiles) &&
      base::Contains(available_categories, PickerCategory::kDriveFiles)) {
    cros_search_sources.push_back(PickerSearchSource::kDrive);
  }

  if (!cros_search_sources.empty()) {
    // TODO: b/326166751 - Use `available_categories_` to decide what searches
    // to do.
    for (PickerSearchSource source : cros_search_sources) {
      MarkSearchStarted(source);
    }
    client_->StartCrosSearch(
        std::u16string(query), category,
        base::BindRepeating(&PickerSearchRequest::HandleCrosSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if ((!category.has_value() || category == PickerCategory::kClipboard) &&
      base::Contains(available_categories, PickerCategory::kClipboard)) {
    clipboard_provider_ = std::make_unique<PickerClipboardHistoryProvider>();
    MarkSearchStarted(PickerSearchSource::kClipboard);
    clipboard_provider_->FetchResults(
        base::BindOnce(&PickerSearchRequest::HandleClipboardSearchResults,
                       weak_ptr_factory_.GetWeakPtr()),
        query);
  }

  if ((!category.has_value() || category == PickerCategory::kDatesTimes) &&
      base::Contains(available_categories, PickerCategory::kDatesTimes)) {
    MarkSearchStarted(PickerSearchSource::kDate);
    // Date results is currently synchronous.
    HandleDateSearchResults(PickerDateSearch(base::Time::Now(), query));
  }

  if ((!category.has_value() || category == PickerCategory::kUnitsMaths) &&
      base::Contains(available_categories, PickerCategory::kUnitsMaths)) {
    MarkSearchStarted(PickerSearchSource::kMath);
    // Math results is currently synchronous.
    HandleMathSearchResults(PickerMathSearch(query));
  }

  // These searches do not have category-specific search.
  if (!category.has_value()) {
    MarkSearchStarted(PickerSearchSource::kAction);
    // Action results are currently synchronous.
    HandleActionSearchResults(PickerActionSearch(
        {.available_categories = available_categories,
         .caps_lock_state_to_search = options.caps_lock_state_to_search,
         .search_case_transforms = options.search_case_transforms},
        query));

    if (base::Contains(available_categories, PickerCategory::kEditorWrite)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(PickerSearchSource::kEditorWrite);
      HandleEditorSearchResults(
          PickerSearchSource::kEditorWrite,
          PickerEditorSearch(PickerEditorResult::Mode::kWrite, query));
    }

    if (base::Contains(available_categories, PickerCategory::kEditorRewrite)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(PickerSearchSource::kEditorRewrite);
      HandleEditorSearchResults(
          PickerSearchSource::kEditorRewrite,
          PickerEditorSearch(PickerEditorResult::Mode::kRewrite, query));
    }

    if (base::Contains(available_categories, PickerCategory::kLobster)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(PickerSearchSource::kLobster);
      HandleLobsterSearchResults(PickerSearchSource::kLobster,
                                 PickerLobsterSearch(query));
    }
  }

  can_call_done_closure_ = true;
  MaybeCallDoneClosure();
}

PickerSearchRequest::~PickerSearchRequest() {
  // Ensure that any bound callbacks to `Handle*SearchResults` - and therefore
  // `current_callback_` - will not get called by stopping searches.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!done_callback_.is_null()) {
    std::move(done_callback_).Run(/*interrupted=*/true);
    current_callback_.Reset();
  }
  client_->StopCrosQuery();
}

void PickerSearchRequest::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results,
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

void PickerSearchRequest::HandleActionSearchResults(
    std::vector<PickerSearchResult> results) {
  HandleSearchSourceResults(PickerSearchSource::kAction, std::move(results),
                            /*has_more_results*/ false);
}

void PickerSearchRequest::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  switch (type) {
    case AppListSearchResultType::kOmnibox: {
      results = DeduplicateGoogleCorpGotoDomains(std::move(results));
      size_t results_to_remove = is_category_specific_search_
                                     ? 0
                                     : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - results_to_remove, results.end());

      HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                std::move(results),
                                /*has_more_results=*/results_to_remove > 0);
      break;
    }
    case AppListSearchResultType::kDriveSearch: {
      size_t files_to_remove = is_category_specific_search_
                                   ? 0
                                   : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(PickerSearchSource::kDrive, std::move(results),
                                /*has_more_results=*/files_to_remove > 0);
      break;
    }
    case AppListSearchResultType::kFileSearch: {
      size_t files_to_remove = is_category_specific_search_
                                   ? 0
                                   : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(PickerSearchSource::kLocalFile,
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

void PickerSearchRequest::HandleDateSearchResults(
    std::vector<PickerSearchResult> results) {
  // Date results are never truncated.
  HandleSearchSourceResults(PickerSearchSource::kDate, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::HandleMathSearchResults(
    std::optional<PickerSearchResult> result) {
  std::vector<PickerSearchResult> results;
  if (result.has_value()) {
    results.push_back(*std::move(result));
  }

  // Math results are never truncated.
  HandleSearchSourceResults(PickerSearchSource::kMath, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::HandleClipboardSearchResults(
    std::vector<PickerSearchResult> results) {
  // Clipboard results are never truncated.
  HandleSearchSourceResults(PickerSearchSource::kClipboard, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::HandleEditorSearchResults(
    PickerSearchSource source,
    std::optional<PickerSearchResult> result) {
  std::vector<PickerSearchResult> results;
  if (result.has_value()) {
    results.push_back(std::move(*result));
  }

  // Editor results are never truncated.
  HandleSearchSourceResults(source, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::HandleLobsterSearchResults(
    PickerSearchSource source,
    std::optional<PickerSearchResult> result) {
  std::vector<PickerSearchResult> results;
  if (result.has_value()) {
    results.push_back(std::move(*result));
  }

  // Lobster results are never truncated.
  HandleSearchSourceResults(source, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::MarkSearchStarted(PickerSearchSource source) {
  CHECK(!SwapSearchStart(source, base::TimeTicks::Now()).has_value())
      << "search_starts_ enum " << base::to_underlying(source)
      << " was already set";
}

void PickerSearchRequest::MarkSearchEnded(PickerSearchSource source) {
  std::optional<base::TimeTicks> start = SwapSearchStart(source, std::nullopt);
  CHECK(start.has_value()) << "search_starts_ enum "
                           << base::to_underlying(source) << " was not set";

  base::TimeDelta elapsed = base::TimeTicks::Now() - *start;
  base::UmaHistogramTimes(SearchSourceToHistogram(source), elapsed);
}

std::optional<base::TimeTicks> PickerSearchRequest::SwapSearchStart(
    PickerSearchSource source,
    std::optional<base::TimeTicks> new_value) {
  return std::exchange(search_starts_[base::to_underlying(source)],
                       std::move(new_value));
}

void PickerSearchRequest::MaybeCallDoneClosure() {
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
