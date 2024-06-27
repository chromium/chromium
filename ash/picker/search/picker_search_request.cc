// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_request.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/picker_clipboard_provider.h"
#include "ash/picker/search/picker_category_search.h"
#include "ash/picker/search/picker_date_search.h"
#include "ash/picker/search/picker_editor_search.h"
#include "ash/picker/search/picker_math_search.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"

namespace ash {

namespace {

const char* SearchSourceToHistogram(PickerSearchSource source) {
  switch (source) {
    case PickerSearchSource::kOmnibox:
      return "Ash.Picker.Search.OmniboxProvider.QueryTime";
    case PickerSearchSource::kTenor:
      return "Ash.Picker.Search.GifProvider.QueryTime";
    case PickerSearchSource::kEmoji:
      // Unused.
      return "Ash.Picker.Search.EmojiProvider.QueryTime";
    case PickerSearchSource::kDate:
      return "Ash.Picker.Search.DateProvider.QueryTime";
    case PickerSearchSource::kCategory:
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
  }
  NOTREACHED() << "Unexpected search source " << base::to_underlying(source);
}

}  // namespace

PickerSearchRequest::PickerSearchRequest(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    SearchResultsCallback callback,
    PickerClient* client,
    base::span<const PickerCategory> available_categories)
    : is_category_specific_search_(category.has_value()),
      client_(CHECK_DEREF(client)),
      current_callback_(std::move(callback)),
      gif_search_debouncer_(kGifDebouncingDelay) {
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
        query, category,
        base::BindRepeating(&PickerSearchRequest::HandleCrosSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if ((!category.has_value() || category == PickerCategory::kClipboard) &&
      base::Contains(available_categories, PickerCategory::kClipboard)) {
    clipboard_provider_ = std::make_unique<PickerClipboardProvider>();
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
    if (base::Contains(available_categories, PickerCategory::kExpressions)) {
      // TODO: b/348067874 - Add "pending start" to `search_starts_` state.
      gif_search_debouncer_.RequestSearch(
          base::BindOnce(&PickerSearchRequest::StartGifSearch,
                         weak_ptr_factory_.GetWeakPtr(), utf8_query));
    }

    MarkSearchStarted(PickerSearchSource::kCategory);
    // Category results are currently synchronous.
    HandleCategorySearchResults(
        PickerCategorySearch(available_categories, query));

    if (base::Contains(available_categories, PickerCategory::kEditorWrite)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(PickerSearchSource::kEditorWrite);
      HandleEditorSearchResults(
          PickerSearchSource::kEditorWrite,
          PickerEditorSearch(PickerSearchResult::EditorData::Mode::kWrite,
                             query));
    }

    if (base::Contains(available_categories, PickerCategory::kEditorRewrite)) {
      // Editor results are currently synchronous.
      MarkSearchStarted(PickerSearchSource::kEditorRewrite);
      HandleEditorSearchResults(
          PickerSearchSource::kEditorRewrite,
          PickerEditorSearch(PickerSearchResult::EditorData::Mode::kRewrite,
                             query));
    }
  }
}

PickerSearchRequest::~PickerSearchRequest() {
  // Ensure that any bound callbacks to `Handle*SearchResults` will not get
  // called by stopping searches.
  weak_ptr_factory_.InvalidateWeakPtrs();
  client_->StopCrosQuery();
  client_->StopGifSearch();
}

void PickerSearchRequest::StartGifSearch(const std::string& query) {
  MarkSearchStarted(PickerSearchSource::kTenor);
  client_->FetchGifSearch(
      query, base::BindOnce(&PickerSearchRequest::HandleGifSearchResults,
                            weak_ptr_factory_.GetWeakPtr(), query));
}

void PickerSearchRequest::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results,
    bool has_more_results) {
  MarkSearchEnded(source);
  // This method is only called from `Handle*SearchResults` methods (one for
  // each search source), and the only time `current_callback_` is null is when
  // the search is stopped.
  // As our `WeakPtrFactory` should have invalidated any bound callbacks to
  // `Handle*SearchResults` before resetting the callback to null, this method
  // should - in theory - never be called after `current_callback_` is reset.
  CHECK(!current_callback_.is_null())
      << "Current callback is null in HandleSearchSourceResults";
  current_callback_.Run(source, std::move(results), has_more_results);
}

void PickerSearchRequest::HandleCategorySearchResults(
    std::vector<PickerSearchResult> results) {
  HandleSearchSourceResults(PickerSearchSource::kCategory, std::move(results),
                            /*has_more_results*/ false);
}

void PickerSearchRequest::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  switch (type) {
    case AppListSearchResultType::kOmnibox: {
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

void PickerSearchRequest::HandleGifSearchResults(
    std::string query,
    std::vector<PickerSearchResult> results) {
  // There are always more GIF results.
  HandleSearchSourceResults(PickerSearchSource::kTenor, std::move(results),
                            /*has_more_results=*/true);
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

}  // namespace ash
