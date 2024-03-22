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
    PickerClient* client,
    base::span<const PickerCategory> available_categories)
    : client_(CHECK_DEREF(client)),
      available_categories_(available_categories.begin(),
                            available_categories.end()),
      gif_search_debouncer_(kGifDebouncingDelay) {}

PickerSearchRequest::~PickerSearchRequest() = default;

void PickerSearchRequest::StartSearch(const std::u16string& query,
                                      std::optional<PickerCategory> category,
                                      SearchResultsCallback callback) {
  StopSearch();
  current_callback_ = std::move(callback);
  std::string utf8_query = base::UTF16ToUTF8(query);
  current_query_ = utf8_query;

  // TODO: b/326166751 - Use `available_categories_` to decide what searches to
  // do.
  if (!category.has_value() || (category == PickerCategory::kBrowsingHistory ||
                                category == PickerCategory::kBookmarks ||
                                category == PickerCategory::kOpenTabs ||
                                category == PickerCategory::kLocalFiles ||
                                category == PickerCategory::kDriveFiles)) {
    cros_search_start_ = base::TimeTicks::Now();
    client_->StartCrosSearch(
        query, category,
        base::BindRepeating(&PickerSearchRequest::HandleCrosSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // These searches do not have category-specific search.
  if (!category.has_value()) {
    gif_search_debouncer_.RequestSearch(
        base::BindOnce(&PickerSearchRequest::StartGifSearch,
                       weak_ptr_factory_.GetWeakPtr(), utf8_query));

    emoji_search_start_ = base::TimeTicks::Now();
    // Emoji search is currently synchronous.
    HandleEmojiSearchResults(emoji_search_.SearchEmoji(utf8_query));

    date_search_start_ = base::TimeTicks::Now();
    // Date results is currently synchronous.
    HandleDateSearchResults(PickerDateSearch(base::Time::Now(), query));

    // Math results is currently synchronous.
    HandleMathSearchResults(PickerMathSearch(query));

    category_search_start_ = base::TimeTicks::Now();
    // Category results are currently synchronous.
    HandleCategorySearchResults(
        PickerCategorySearch(available_categories_, query));
  }
}

void PickerSearchRequest::StopSearch() {
  // Ensure that any bound callbacks to `Handle*SearchResults` will not get
  // called after the current callback is reset.
  weak_ptr_factory_.InvalidateWeakPtrs();
  current_callback_.Reset();
  date_search_start_.reset();
  cros_search_start_.reset();
  gif_search_start_.reset();
  emoji_search_start_.reset();
  category_search_start_.reset();
  client_->StopCrosQuery();
  client_->StopGifSearch();
}

void PickerSearchRequest::StartGifSearch(const std::string& query) {
  if (current_query_ != query) {
    LOG(DFATAL) << "Current query " << current_query_
                << " does not match debounced query " << query;
    return;
  }

  gif_search_start_ = base::TimeTicks::Now();
  client_->FetchGifSearch(
      query, base::BindOnce(&PickerSearchRequest::HandleGifSearchResults,
                            weak_ptr_factory_.GetWeakPtr(), query));
}

void PickerSearchRequest::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results) {
  // This method is only called from `Handle*SearchResults` methods (one for
  // each search source), and the only time `current_callback_` is null is when
  // the search is stopped.
  // As our `WeakPtrFactory` should have invalidated any bound callbacks to
  // `Handle*SearchResults` before resetting the callback to null, this method
  // should - in theory - never be called after `current_callback_` is reset.
  CHECK(!current_callback_.is_null())
      << "Current callback is null in HandleSearchSourceResults";
  current_callback_.Run(source, std::move(results));
}

void PickerSearchRequest::HandleCategorySearchResults(
    std::vector<PickerSearchResult> results) {
  if (category_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *category_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.CategoryProvider.QueryTime",
                            elapsed);
  }

  HandleSearchSourceResults(PickerSearchSource::kCategory, std::move(results));
}

void PickerSearchRequest::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  switch (type) {
    case AppListSearchResultType::kOmnibox:
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.OmniboxProvider.QueryTime",
                                elapsed);
      }

      HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                std::move(results));
      break;
    case AppListSearchResultType::kDriveSearch: {
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.DriveProvider.QueryTime",
                                elapsed);
      }
      size_t files_to_remove = std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(PickerSearchSource::kDrive, std::move(results));
      break;
    }
    case AppListSearchResultType::kFileSearch: {
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.FileProvider.QueryTime",
                                elapsed);
      }
      size_t files_to_remove = std::max<size_t>(results.size(), 3) - 3;
      results.erase(results.end() - files_to_remove, results.end());

      HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                std::move(results));
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
  if (current_query_ != query) {
    LOG(DFATAL) << "Current query " << current_query_
                << " does not match query of returned responses " << query;
    return;
  }

  if (gif_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *gif_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.GifProvider.QueryTime", elapsed);
  }

  HandleSearchSourceResults(PickerSearchSource::kTenor, std::move(results));
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

  HandleSearchSourceResults(PickerSearchSource::kEmoji,
                            std::move(emoji_results));
}

void PickerSearchRequest::HandleDateSearchResults(
    std::optional<PickerSearchResult> result) {
  if (date_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *date_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.DateProvider.QueryTime",
                            elapsed);
  }

  std::vector<PickerSearchResult> results;
  if (result.has_value()) {
    results.push_back(*std::move(result));
  }
  HandleSearchSourceResults(PickerSearchSource::kDate, std::move(results));
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
  HandleSearchSourceResults(PickerSearchSource::kMath, std::move(results));
}

}  // namespace ash
