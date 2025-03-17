// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_emoji_suggester.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/quick_insert/model/quick_insert_emoji_history_model.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace ash {
namespace {

using HistoryItem = QuickInsertEmojiHistoryModel::EmojiHistoryItem;

constexpr std::string_view kDefaultSuggestedEmojis[] = {"🙂", "😂", "🤔",
                                                        "😢", "👏", "👍"};
constexpr size_t kSuggestedEmojisSize = 6u;

bool ContainsEmoji(const std::vector<HistoryItem>& vec,
                   std::string_view emoji) {
  return std::ranges::any_of(vec, [emoji](const HistoryItem& item) {
    return item.category == ui::EmojiPickerCategory::kEmojis &&
           item.text == emoji;
  });
}

}  // namespace

QuickInsertEmojiSuggester::QuickInsertEmojiSuggester(
    QuickInsertEmojiHistoryModel* history_model,
    GetNameCallback get_name)
    : get_name_(std::move(get_name)),
      history_model_(CHECK_DEREF(history_model)) {
  CHECK(!get_name_.is_null());
}

QuickInsertEmojiSuggester::~QuickInsertEmojiSuggester() = default;

std::vector<QuickInsertEmojiResult>
QuickInsertEmojiSuggester::GetSuggestedEmoji() const {
  std::vector<HistoryItem> recent_emojis =
      history_model_->GetRecentEmojis(ui::EmojiPickerCategory::kEmojis);
  std::vector<HistoryItem> recent_emoticons =
      history_model_->GetRecentEmojis(ui::EmojiPickerCategory::kEmoticons);
  std::vector<HistoryItem> recent_symbols =
      history_model_->GetRecentEmojis(ui::EmojiPickerCategory::kSymbols);

  recent_emojis.reserve(std::max(
      recent_emojis.size() + recent_emoticons.size() + recent_symbols.size(),
      kSuggestedEmojisSize));
  recent_emojis.insert(recent_emojis.end(), recent_emoticons.begin(),
                       recent_emoticons.end());
  recent_emojis.insert(recent_emojis.end(), recent_symbols.begin(),
                       recent_symbols.end());
  std::sort(recent_emojis.begin(), recent_emojis.end(),
            [](const HistoryItem& a, const HistoryItem& b) {
              return a.timestamp > b.timestamp;
            });

  // Fill with default emojis if history is not enough.
  for (std::string_view emoji : kDefaultSuggestedEmojis) {
    if (recent_emojis.size() >= kSuggestedEmojisSize) {
      break;
    }
    if (!ContainsEmoji(recent_emojis, emoji)) {
      recent_emojis.push_back({.text = std::string(emoji),
                               .category = ui::EmojiPickerCategory::kEmojis,
                               .timestamp = base::Time()});
    }
  }

  return base::ToVector(recent_emojis, [this](const HistoryItem& item) {
    switch (item.category) {
      case ui::EmojiPickerCategory::kEmojis:
        return QuickInsertEmojiResult::Emoji(
            base::UTF8ToUTF16(item.text),
            base::UTF8ToUTF16(get_name_.Run(item.text)));
      case ui::EmojiPickerCategory::kEmoticons:
        return QuickInsertEmojiResult::Emoticon(
            base::UTF8ToUTF16(item.text),
            base::UTF8ToUTF16(get_name_.Run(item.text)));
      case ui::EmojiPickerCategory::kSymbols:
        return QuickInsertEmojiResult::Symbol(
            base::UTF8ToUTF16(item.text),
            base::UTF8ToUTF16(get_name_.Run(item.text)));
      case ui::EmojiPickerCategory::kGifs:
        NOTREACHED();
    }
  });
}

}  // namespace ash
