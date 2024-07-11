// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_emoji_history_model.h"

#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace ash {
namespace {

constexpr int kMaxRecentEmoji = 20;

constexpr std::string_view kEmojiHistoryValueFieldName = "text";
constexpr std::string_view kEmojiHistoryTimestampFieldName = "timestamp";

std::string ConvertEmojiCategoryToString(ui::EmojiPickerCategory category) {
  switch (category) {
    case ui::EmojiPickerCategory::kEmojis:
      return "emoji";
    case ui::EmojiPickerCategory::kSymbols:
      return "symbol";
    case ui::EmojiPickerCategory::kEmoticons:
      return "emoticon";
    case ui::EmojiPickerCategory::kGifs:
      return "gif";
  }
}

}  // namespace

bool PickerEmojiHistoryModel::EmojiHistoryItem::operator==(
    const PickerEmojiHistoryModel::EmojiHistoryItem&) const = default;

PickerEmojiHistoryModel::PickerEmojiHistoryModel(PrefService* prefs,
                                                 base::Clock* clock)
    : prefs_(CHECK_DEREF(prefs)), clock_(clock) {}

std::vector<PickerEmojiHistoryModel::EmojiHistoryItem>
PickerEmojiHistoryModel::GetRecentEmojis(
    ui::EmojiPickerCategory category) const {
  const base::Value::List* history =
      prefs_->GetDict(prefs::kEmojiPickerHistory)
          .FindList(ConvertEmojiCategoryToString(category));
  if (history == nullptr) {
    return {};
  }
  std::vector<EmojiHistoryItem> results;
  for (const base::Value& it : *history) {
    const base::Value::Dict* value_dict = it.GetIfDict();
    if (value_dict == nullptr) {
      continue;
    }
    const std::string* text =
        value_dict->FindString(kEmojiHistoryValueFieldName);
    std::optional<base::Time> timestamp =
        base::ValueToTime(value_dict->Find(kEmojiHistoryTimestampFieldName));
    if (text != nullptr) {
      results.push_back({.text = *text,
                         .category = category,
                         .timestamp = timestamp == std::nullopt
                                          ? base::Time::UnixEpoch()
                                          : *timestamp});
    }
  }
  return results;
}

void PickerEmojiHistoryModel::UpdateRecentEmoji(
    ui::EmojiPickerCategory category,
    std::string_view latest_emoji) {
  std::vector<EmojiHistoryItem> history = GetRecentEmojis(category);
  base::Value::List history_value;
  history_value.Append(base::Value::Dict()
                           .Set(kEmojiHistoryValueFieldName, latest_emoji)
                           .Set(kEmojiHistoryTimestampFieldName,
                                base::TimeToValue(clock_->Now())));
  for (const EmojiHistoryItem& item : history) {
    if (item.text == latest_emoji) {
      continue;
    }
    history_value.Append(base::Value::Dict()
                             .Set(kEmojiHistoryValueFieldName, item.text)
                             .Set(kEmojiHistoryTimestampFieldName,
                                  base::TimeToValue(item.timestamp)));
    if (history_value.size() == kMaxRecentEmoji) {
      break;
    }
  }

  ScopedDictPrefUpdate update(&prefs_.get(), prefs::kEmojiPickerHistory);
  update->Set(ConvertEmojiCategoryToString(category), std::move(history_value));
}

}  // namespace ash
