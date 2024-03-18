// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_controller.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/picker_category_search.h"
#include "ash/picker/search/picker_date_search.h"
#include "ash/picker/search/picker_math_search.h"
#include "ash/picker/search/picker_search_debouncer.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/emoji/emoji_search.h"

namespace ash {

enum class AppListSearchResultType;

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

PickerSearchController::PickerSearchController(
    PickerClient* client,
    base::span<const PickerCategory> available_categories,
    base::TimeDelta burn_in_period)
    : client_(CHECK_DEREF(client)),
      available_categories_(available_categories.begin(),
                            available_categories.end()),
      burn_in_period_(burn_in_period),
      gif_search_debouncer_(kGifDebouncingDelay) {}

PickerSearchController::~PickerSearchController() = default;

void PickerSearchController::StartSearch(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    PickerViewDelegate::SearchResultsCallback callback) {
  StopSearch();
  current_callback_ = std::move(callback);
  std::string utf8_query = base::UTF16ToUTF8(query);
  current_query_ = utf8_query;

  // TODO: b/324154537 - Show a loading animation while waiting for results.
  burn_in_timer_.Start(FROM_HERE, burn_in_period_, this,
                       &PickerSearchController::PublishBurnInResults);

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
        base::BindRepeating(&PickerSearchController::HandleCrosSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // These searches do not have category-specific search.
  if (!category.has_value()) {
    gif_search_debouncer_.RequestSearch(
        base::BindOnce(&PickerSearchController::StartGifSearch,
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

void PickerSearchController::StopSearch() {
  current_callback_.Reset();
  date_search_start_.reset();
  cros_search_start_.reset();
  gif_search_start_.reset();
  emoji_search_start_.reset();
  category_search_start_.reset();
  client_->StopCrosQuery();
  client_->StopGifSearch();
  ResetResults();
}

bool PickerSearchController::IsSearchStopped() const {
  return current_callback_.is_null();
}

bool PickerSearchController::IsPostBurnIn() const {
  return !burn_in_timer_.IsRunning();
}

void PickerSearchController::StartGifSearch(const std::string& query) {
  if (current_query_ != query) {
    LOG(DFATAL) << "Current query " << current_query_
                << " does not match debounced query " << query;
    return;
  }

  gif_search_start_ = base::TimeTicks::Now();
  client_->FetchGifSearch(
      query, base::BindOnce(&PickerSearchController::HandleGifSearchResults,
                            weak_ptr_factory_.GetWeakPtr(), query));
}

void PickerSearchController::ResetResults() {
  omnibox_results_.clear();
  gif_results_.clear();
  emoji_results_.clear();
  suggested_results_.clear();
}

void PickerSearchController::PublishBurnInResults() {
  if (IsSearchStopped()) {
    return;
  }

  std::vector<PickerSearchResultsSection> sections;
  if (!suggested_results_.empty()) {
    sections.push_back(PickerSearchResultsSection(
        PickerSectionType::kSuggestions, std::move(suggested_results_)));
  }
  if (!category_results_.empty()) {
    sections.push_back(PickerSearchResultsSection(
        PickerSectionType::kCategories, std::move(category_results_)));
  }
  if (!emoji_results_.empty()) {
    sections.push_back(PickerSearchResultsSection(
        PickerSectionType::kExpressions, std::move(emoji_results_)));
  }
  if (!omnibox_results_.empty()) {
    sections.push_back(PickerSearchResultsSection(PickerSectionType::kLinks,
                                                  std::move(omnibox_results_)));
  }
  if (!local_file_results_.empty()) {
    sections.emplace_back(PickerSectionType::kFiles,
                          std::move(local_file_results_));
  }
  if (!drive_file_results_.empty()) {
    sections.emplace_back(PickerSectionType::kDriveFiles,
                          std::move(drive_file_results_));
  }
  if (!gif_results_.empty()) {
    sections.push_back(PickerSearchResultsSection(PickerSectionType::kGifs,
                                                  std::move(gif_results_)));
  }
  current_callback_.Run(std::move(sections));
}

void PickerSearchController::AppendPostBurnInResults(
    PickerSearchResultsSection section) {
  if (IsSearchStopped()) {
    return;
  }

  CHECK(IsPostBurnIn());
  if (!section.results().empty()) {
    current_callback_.Run({{std::move(section)}});
  }
}

void PickerSearchController::HandleCategorySearchResults(
    std::vector<PickerSearchResult> results) {
  if (category_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *category_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.CategoryProvider.QueryTime",
                            elapsed);
  }

  category_results_ = std::move(results);
}

void PickerSearchController::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  if (IsSearchStopped()) {
    return;
  }
  switch (type) {
    case AppListSearchResultType::kOmnibox:
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.OmniboxProvider.QueryTime",
                                elapsed);
      }

      omnibox_results_ = std::move(results);

      if (IsPostBurnIn()) {
        AppendPostBurnInResults(PickerSearchResultsSection(
            PickerSectionType::kLinks, std::move(omnibox_results_)));
      }
      break;
    case AppListSearchResultType::kDriveSearch: {
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.DriveProvider.QueryTime",
                                elapsed);
      }
      drive_file_results_ = std::move(results);
      size_t files_to_remove =
          std::max<size_t>(drive_file_results_.size(), 3) - 3;
      drive_file_results_.erase(drive_file_results_.end() - files_to_remove,
                                drive_file_results_.end());

      if (IsPostBurnIn()) {
        AppendPostBurnInResults(PickerSearchResultsSection(
            PickerSectionType::kDriveFiles, std::move(drive_file_results_)));
      }
      break;
    }
    case AppListSearchResultType::kFileSearch: {
      if (cros_search_start_.has_value()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - *cros_search_start_;
        base::UmaHistogramTimes("Ash.Picker.Search.FileProvider.QueryTime",
                                elapsed);
      }
      local_file_results_ = std::move(results);
      size_t files_to_remove =
          std::max<size_t>(local_file_results_.size(), 3) - 3;
      local_file_results_.erase(local_file_results_.end() - files_to_remove,
                                local_file_results_.end());

      if (IsPostBurnIn()) {
        AppendPostBurnInResults(PickerSearchResultsSection(
            PickerSectionType::kFiles, std::move(local_file_results_)));
      }
      break;
    }
    default:
      LOG(DFATAL) << "Got unexpected search result type "
                  << static_cast<int>(type);
      break;
  }
}

void PickerSearchController::HandleGifSearchResults(
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

  gif_results_ = std::move(results);

  if (IsPostBurnIn()) {
    AppendPostBurnInResults(PickerSearchResultsSection(
        PickerSectionType::kGifs, std::move(gif_results_)));
  }
}

void PickerSearchController::HandleEmojiSearchResults(
    emoji::EmojiSearchResult results) {
  if (emoji_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *emoji_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.EmojiProvider.QueryTime",
                            elapsed);
  }

  emoji_results_.clear();
  emoji_results_.reserve(kMaxEmojiResults + kMaxSymbolResults +
                         kMaxEmoticonResults);

  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.emojis, kMaxEmojiResults)) {
    emoji_results_.push_back(
        PickerSearchResult::Emoji(base::UTF8ToUTF16(result.emoji_string)));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.symbols, kMaxSymbolResults)) {
    emoji_results_.push_back(
        PickerSearchResult::Symbol(base::UTF8ToUTF16(result.emoji_string)));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.emoticons, kMaxEmoticonResults)) {
    emoji_results_.push_back(
        PickerSearchResult::Emoticon(base::UTF8ToUTF16(result.emoji_string)));
  }
}

void PickerSearchController::HandleDateSearchResults(
    std::optional<PickerSearchResult> result) {
  if (date_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *date_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.DateProvider.QueryTime",
                            elapsed);
  }

  if (result.has_value()) {
    suggested_results_.push_back(*std::move(result));
  }
}

void PickerSearchController::HandleMathSearchResults(
    std::optional<PickerSearchResult> result) {
  if (math_search_start_.has_value()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - *math_search_start_;
    base::UmaHistogramTimes("Ash.Picker.Search.MathProvider.QueryTime",
                            elapsed);
  }

  if (result.has_value()) {
    suggested_results_.push_back(*std::move(result));
  }
}

}  // namespace ash
