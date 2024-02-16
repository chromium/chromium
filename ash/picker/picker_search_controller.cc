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

base::span<const std::string> FirstNOrLessElements(
    base::span<const std::string> container,
    size_t n) {
  return container.subspan(0, std::min(container.size(), n));
}

}  // namespace

PickerSearchController::PickerSearchController(PickerClient* client,
                                               base::TimeDelta burn_in_period)
    : client_(CHECK_DEREF(client)), burn_in_period_(burn_in_period) {}

PickerSearchController::~PickerSearchController() = default;

void PickerSearchController::StartSearch(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    PickerViewDelegate::SearchResultsCallback callback) {
  current_callback_.Reset();
  client_->StopCrosQuery();
  ResetResults();
  current_callback_ = std::move(callback);
  current_query_ = query;

  client_->StartCrosSearch(
      query, base::BindRepeating(&PickerSearchController::HandleSearchResults,
                                 weak_ptr_factory_.GetWeakPtr()));
  std::string utf8_query = base::UTF16ToUTF8(query);
  client_->FetchGifSearch(
      utf8_query,
      base::BindOnce(&PickerSearchController::HandleGifSearchResults,
                     weak_ptr_factory_.GetWeakPtr(), query));

  // Emoji search is currently synchronous.
  HandleEmojiSearchResults(emoji_search_.SearchEmoji(utf8_query));

  // TODO: b/324154537 - Show a loading animation while waiting for results.
  burn_in_timer_.Start(FROM_HERE, burn_in_period_, this,
                       &PickerSearchController::PublishResults);
}

void PickerSearchController::ResetResults() {
  omnibox_results_.clear();
  gif_results_.clear();
  emoji_search_results_.clear();
}

void PickerSearchController::PublishResults() {
  if (!current_callback_) {
    return;
  }

  std::vector<PickerSearchResult> expression_results;
  expression_results.reserve(emoji_search_results_.size() +
                             gif_results_.size());
  expression_results.insert(expression_results.end(),
                            emoji_search_results_.begin(),
                            emoji_search_results_.end());
  expression_results.insert(expression_results.end(), gif_results_.begin(),
                            gif_results_.end());

  std::vector<PickerSearchResults::Section> sections;
  if (!expression_results.empty()) {
    sections.push_back(PickerSearchResults::Section(u"Matching expressions",
                                                    expression_results));
  }
  if (!omnibox_results_.empty()) {
    sections.push_back(
        PickerSearchResults::Section(u"Matching links", omnibox_results_));
  }
  current_callback_.Run(PickerSearchResults(std::move(sections)));
}

void PickerSearchController::HandleSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  omnibox_results_ = std::move(results);
}

void PickerSearchController::HandleGifSearchResults(
    std::u16string query,
    std::vector<PickerSearchResult> results) {
  // As we cannot stop GIF search result callbacks, check whether the query for
  // this request is the current query to prevent showing results from an
  // outdated query.
  // TODO: b/324992789 - Allow stopping GIF search results.
  if (query == current_query_) {
    gif_results_ = std::move(results);
  }
}

void PickerSearchController::HandleEmojiSearchResults(
    emoji::EmojiSearchResult results) {
  std::vector<PickerSearchResult> picker_emoji_search_results;
  for (const std::string& result : FirstNOrLessElements(results.emojis, 3)) {
    picker_emoji_search_results.push_back(
        PickerSearchResult::Emoji(base::UTF8ToUTF16(result)));
  }
  for (const std::string& result : FirstNOrLessElements(results.symbols, 2)) {
    picker_emoji_search_results.push_back(
        PickerSearchResult::Symbol(base::UTF8ToUTF16(result)));
  }
  for (const std::string& result : FirstNOrLessElements(results.emoticons, 2)) {
    picker_emoji_search_results.push_back(
        PickerSearchResult::Emoticon(base::UTF8ToUTF16(result)));
  }
  emoji_search_results_ = std::move(picker_emoji_search_results);
}

}  // namespace ash
