// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/picker_search_aggregator.h"
#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

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

const base::Value::Dict* LoadEmojiVariantsFromPrefs(PrefService* prefs) {
  if (prefs == nullptr) {
    return nullptr;
  }
  return prefs->GetDict(prefs::kEmojiPickerPreferences)
      .FindDict("preferred_variants");
}

}  // namespace

PickerSearchController::PickerSearchController(PickerClient* client,
                                               base::TimeDelta burn_in_period)
    : client_(CHECK_DEREF(client)), burn_in_period_(burn_in_period) {}

PickerSearchController::~PickerSearchController() = default;

void PickerSearchController::StartSearch(
    std::u16string_view query,
    std::optional<PickerCategory> category,
    PickerSearchRequest::Options search_options,
    PickerViewDelegate::SearchResultsCallback callback) {
  StopSearch();
  aggregator_ = std::make_unique<PickerSearchAggregator>(burn_in_period_,
                                                         std::move(callback));

  // TODO: b/348067874 - Hook `done_closure` up to `aggregator_`.
  search_request_ = std::make_unique<PickerSearchRequest>(
      query, std::move(category),
      base::BindRepeating(&PickerSearchAggregator::HandleSearchSourceResults,
                          aggregator_->GetWeakPtr()),
      base::BindOnce(&PickerSearchAggregator::HandleNoMoreResults,
                     aggregator_->GetWeakPtr()),
      &client_.get(), std::move(search_options));
}

void PickerSearchController::StopSearch() {
  // The search request must be reset first so it can let the aggregator know
  // that it has been interrupted.
  search_request_.reset();
  aggregator_.reset();
}

void PickerSearchController::StartEmojiSearch(
    std::u16string_view query,
    PickerViewDelegate::EmojiSearchResultsCallback callback) {
  const base::TimeTicks search_start = base::TimeTicks::Now();

  emoji::EmojiSearchResult results =
      emoji_search_.SearchEmoji(base::UTF16ToUTF8(query));

  base::TimeDelta elapsed = base::TimeTicks::Now() - search_start;
  base::UmaHistogramTimes("Ash.Picker.Search.EmojiProvider.QueryTime", elapsed);

  std::vector<PickerSearchResult> emoji_results;
  emoji_results.reserve(kMaxEmojiResults + kMaxSymbolResults +
                        kMaxEmoticonResults);

  const base::Value::Dict* emoji_variants =
      LoadEmojiVariantsFromPrefs(client_->GetPrefs());

  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.emojis, kMaxEmojiResults)) {
    std::string emoji_string = result.emoji_string;
    if (emoji_variants != nullptr) {
      const std::string* variant_string =
          emoji_variants->FindString(emoji_string);
      if (variant_string != nullptr) {
        emoji_string = *variant_string;
      }
    }
    emoji_results.push_back(PickerSearchResult::Emoji(
        base::UTF8ToUTF16(emoji_string),
        base::UTF8ToUTF16(emoji_search_.GetEmojiName(emoji_string))));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.symbols, kMaxSymbolResults)) {
    emoji_results.push_back(PickerSearchResult::Symbol(
        base::UTF8ToUTF16(result.emoji_string),
        base::UTF8ToUTF16(emoji_search_.GetEmojiName(result.emoji_string))));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrLessElements(results.emoticons, kMaxEmoticonResults)) {
    emoji_results.push_back(PickerSearchResult::Emoticon(
        base::UTF8ToUTF16(result.emoji_string),
        base::UTF8ToUTF16(emoji_search_.GetEmojiName(result.emoji_string))));
  }

  std::move(callback).Run(std::move(emoji_results));
}

}  // namespace ash
