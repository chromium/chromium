// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_search_controller.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/emoji/emoji_search.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace ash {

enum class AppListSearchResultType;

namespace {

constexpr int kMaxEmojiResults = 3;
constexpr int kMaxSymbolResults = 2;
constexpr int kMaxEmoticonResults = 2;

base::span<const std::string> FirstNOrLessElements(
    base::span<const std::string> container,
    size_t n) {
  return container.subspan(0, std::min(container.size(), n));
}

}  // namespace

PickerSearchController::PickerSearchController(PickerClient* client,
                                               base::TimeDelta burn_in_period)
    : client_(CHECK_DEREF(client)),
      burn_in_period_(burn_in_period),
      gif_search_debouncer_(kGifDebouncingDelay) {}

PickerSearchController::~PickerSearchController() = default;

void PickerSearchController::StartSearch(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    PickerViewDelegate::SearchResultsCallback callback) {
  current_callback_.Reset();
  client_->StopCrosQuery();
  client_->StopGifSearch();
  ResetResults();
  current_callback_ = std::move(callback);
  std::string utf8_query = base::UTF16ToUTF8(query);
  current_query_ = utf8_query;

  // TODO: b/324154537 - Show a loading animation while waiting for results.
  burn_in_timer_.Start(FROM_HERE, burn_in_period_, this,
                       &PickerSearchController::PublishBurnInResults);

  client_->StartCrosSearch(
      query,
      base::BindRepeating(&PickerSearchController::HandleCrosSearchResults,
                          weak_ptr_factory_.GetWeakPtr()));
  gif_search_debouncer_.RequestSearch(
      base::BindOnce(&PickerSearchController::StartGifSearch,
                     weak_ptr_factory_.GetWeakPtr(), utf8_query));

  // Emoji search is currently synchronous.
  HandleEmojiSearchResults(emoji_search_.SearchEmoji(utf8_query));
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
  client_->FetchGifSearch(
      query, base::BindOnce(&PickerSearchController::HandleGifSearchResults,
                            weak_ptr_factory_.GetWeakPtr(), query));
}

void PickerSearchController::ResetResults() {
  omnibox_results_.clear();
  gif_results_.clear();
  emoji_results_.clear();
}

void PickerSearchController::PublishBurnInResults() {
  if (!current_callback_) {
    return;
  }

  std::vector<PickerSearchResults::Section> sections;
  if (!emoji_results_.empty()) {
    sections.push_back(PickerSearchResults::Section(u"Matching expressions",
                                                    std::move(emoji_results_)));
  }
  if (!omnibox_results_.empty()) {
    sections.push_back(PickerSearchResults::Section(
        u"Matching links", std::move(omnibox_results_)));
  }
  if (!gif_results_.empty()) {
    sections.push_back(PickerSearchResults::Section(u"Other expressions",
                                                    std::move(gif_results_)));
  }
  current_callback_.Run(PickerSearchResults(std::move(sections)));
}

void PickerSearchController::AppendPostBurnInResults(
    PickerSearchResults::Section section) {
  if (!current_callback_) {
    return;
  }

  CHECK(IsPostBurnIn());
  current_callback_.Run(PickerSearchResults({{std::move(section)}}));
}

void PickerSearchController::HandleCrosSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  omnibox_results_ = std::move(results);

  if (IsPostBurnIn()) {
    AppendPostBurnInResults(PickerSearchResults::Section(
        u"Matching links", std::move(omnibox_results_)));
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

  gif_results_ = std::move(results);

  if (IsPostBurnIn()) {
    AppendPostBurnInResults(PickerSearchResults::Section(
        u"Other expressions", std::move(gif_results_)));
  }
}

void PickerSearchController::HandleEmojiSearchResults(
    emoji::EmojiSearchResult results) {
  emoji_results_.clear();
  emoji_results_.reserve(kMaxEmojiResults + kMaxSymbolResults +
                         kMaxEmoticonResults);

  for (const std::string& result :
       FirstNOrLessElements(results.emojis, kMaxEmojiResults)) {
    emoji_results_.push_back(
        PickerSearchResult::Emoji(base::UTF8ToUTF16(result)));
  }
  for (const std::string& result :
       FirstNOrLessElements(results.symbols, kMaxSymbolResults)) {
    emoji_results_.push_back(
        PickerSearchResult::Symbol(base::UTF8ToUTF16(result)));
  }
  for (const std::string& result :
       FirstNOrLessElements(results.emoticons, kMaxEmoticonResults)) {
    emoji_results_.push_back(
        PickerSearchResult::Emoticon(base::UTF8ToUTF16(result)));
  }
}

}  // namespace ash
