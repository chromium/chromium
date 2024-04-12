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
#include "ash/picker/search/picker_math_search.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/emoji/emoji_search.h"

namespace ash {

namespace {

constexpr int kMaxEmojiResults = 3;
constexpr int kMaxSymbolResults = 2;
constexpr int kMaxEmoticonResults = 2;

base::span<const emoji::EmojiSearchEntry> FirstNOrLessElements(
    base::span<const emoji::EmojiSearchEntry> container,
    size_t n) {
  return container.subspan(0, std::min(container.size(), n));
}

}  // namespace

PickerSearchRequest::PickerSearchRequest(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    SearchResultsCallback callback,
    PickerClient* client,
    emoji::EmojiSearch* emoji_search,
    base::span<const PickerCategory> available_categories)
    : is_category_specific_search_(category.has_value()),
      client_(CHECK_DEREF(client)),
      emoji_search_(CHECK_DEREF(emoji_search)),
      current_callback_(std::move(callback)),
      gif_search_debouncer_(kGifDebouncingDelay) {
  std::string utf8_query = base::UTF16ToUTF8(query);

  // TODO: b/326166751 - Use `available_categories_` to decide what searches to
  // do.
  if (!category.has_value() || (category == PickerCategory::kLinks ||
                                category == PickerCategory::kLocalFiles ||
                                category == PickerCategory::kDriveFiles)) {
    cros_search_start_ = base::TimeTicks::Now();
    client_->StartCrosSearch(
        query, category,
        base::BindRepeating(&PickerSearchRequest::HandleCrosSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));

    if (!category.has_value() || category == PickerCategory::kDriveFiles) {
      drive_search_timeout_timer_.Start(
          FROM_HERE, kDriveSearchTimeout, this,
          &PickerSearchRequest::OnDriveSearchTimeout);
    }
  }

  if (!category.has_value() || category == PickerCategory::kClipboard) {
    clipboard_provider_ = std::make_unique<PickerClipboardProvider>();
    clipboard_search_start_ = base::TimeTicks::Now();
    clipboard_provider_->FetchResults(
        base::BindOnce(&PickerSearchRequest::HandleClipboardSearchResults,
                       weak_ptr_factory_.GetWeakPtr()),
        query);
  }

  if (!category.has_value() || category == PickerCategory::kDatesTimes) {
    date_search_start_ = base::TimeTicks::Now();
    // Date results is currently synchronous.
    HandleDateSearchResults(PickerDateSearch(base::Time::Now(), query));
  }

  if (!category.has_value() || category == PickerCategory::kUnitsMaths) {
    math_search_start_ = base::TimeTicks::Now();
    // Math results is currently synchronous.
    HandleMathSearchResults(PickerMathSearch(query));
  }

  // These searches do not have category-specific search.
  if (!category.has_value()) {
    gif_search_debouncer_.RequestSearch(
        base::BindOnce(&PickerSearchRequest::StartGifSearch,
                       weak_ptr_factory_.GetWeakPtr(), utf8_query));

    emoji_search_start_ = base::TimeTicks::Now();
    // Emoji search is currently synchronous.
    HandleEmojiSearchResults(emoji_search_->SearchEmoji(utf8_query));

    category_search_start_ = base::TimeTicks::Now();
    // Category results are currently synchronous.
    HandleCategorySearchResults(
        PickerCategorySearch(available_categories, query));
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
  gif_search_start_ = base::TimeTicks::Now();
  client_->FetchGifSearch(
      query, base::BindOnce(&PickerSearchRequest::HandleGifSearchResults,
                            weak_ptr_factory_.GetWeakPtr(), query));
}

void PickerSearchRequest::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results,
    bool has_more_results) {
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
  if (category_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *category_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.CategoryProvider.QueryTime",
                            elapsed);
  }

  HandleSearchSourceResults(PickerSearchSource::kCategory, std::move(results),
                            /*has_more_results*/ false);
}

void PickerSearchRequest::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  switch (type) {
    case AppListSearchResultType::kOmnibox: {
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.OmniboxProvider.QueryTime",
                                elapsed);
      }

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
      if (!drive_search_timeout_timer_.IsRunning()) {
        return;
      }

      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.DriveProvider.QueryTime",
                                elapsed);
      }
      size_t files_to_remove = is_category_specific_search_
                                   ? 0
                                   : std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(PickerSearchSource::kDrive, std::move(results),
                                /*has_more_results=*/files_to_remove > 0);
      drive_search_timeout_timer_.Stop();
      break;
    }
    case AppListSearchResultType::kFileSearch: {
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.FileProvider.QueryTime",
                                elapsed);
      }
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
  if (gif_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *gif_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.GifProvider.QueryTime", elapsed);
  }

  // There are always more GIF results.
  HandleSearchSourceResults(PickerSearchSource::kTenor, std::move(results),
                            /*has_more_results=*/true);
}

void PickerSearchRequest::HandleEmojiSearchResults(
    emoji::EmojiSearchResult results) {
  if (emoji_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *emoji_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.EmojiProvider.QueryTime",
                            elapsed);
  }

  std::vector<PickerSearchResult> emoji_results;
  emoji_results.reserve(kMaxEmojiResults + kMaxSymbolResults +
                        kMaxEmoticonResults);

  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.emojis, kMaxEmojiResults)) {
    emoji_results.push_back(
        PickerSearchResult::Emoji(base::UTF8ToUTF16(result.emoji_string)));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.symbols, kMaxSymbolResults)) {
    emoji_results.push_back(
        PickerSearchResult::Symbol(base::UTF8ToUTF16(result.emoji_string)));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.emoticons, kMaxEmoticonResults)) {
    emoji_results.push_back(
        PickerSearchResult::Emoticon(base::UTF8ToUTF16(result.emoji_string)));
  }

  const size_t full_results_count =
      results.emojis.size() + results.symbols.size() + results.emoticons.size();
  HandleSearchSourceResults(
      PickerSearchSource::kEmoji, std::move(emoji_results),
      /*has_more_results=*/emoji_results.size() < full_results_count);
}

void PickerSearchRequest::HandleDateSearchResults(
    std::vector<PickerSearchResult> results) {
  if (date_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *date_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.DateProvider.QueryTime",
                            elapsed);
  }

  // Date results are never truncated.
  HandleSearchSourceResults(PickerSearchSource::kDate, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::HandleMathSearchResults(
    std::optional<PickerSearchResult> result) {
  if (math_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *math_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.MathProvider.QueryTime",
                            elapsed);
  }

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
  if (clipboard_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *clipboard_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.ClipboardProvider.QueryTime",
                            elapsed);
  }

  // Clipboard results are never truncated.
  HandleSearchSourceResults(PickerSearchSource::kClipboard, std::move(results),
                            /*has_more_results=*/false);
}

void PickerSearchRequest::OnDriveSearchTimeout() {
  HandleSearchSourceResults(PickerSearchSource::kDrive, {},
                            /*has_more_results=*/false);
}

}  // namespace ash
