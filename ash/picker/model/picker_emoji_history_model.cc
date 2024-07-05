// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_emoji_history_model.h"

#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace ash {
namespace {

constexpr int kMaxRecentEmoji = 20;

constexpr std::string_view kEmojiHistoryValueFieldName = "text";

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

PickerEmojiHistoryModel::PickerEmojiHistoryModel(PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {}

std::vector<std::string> PickerEmojiHistoryModel::GetRecentEmojis(
    ui::EmojiPickerCategory category) const {
  const base::Value::List* history =
      prefs_->GetDict(prefs::kEmojiPickerHistory)
          .FindList(ConvertEmojiCategoryToString(category));
  if (history == nullptr) {
    return {};
  }
  std::vector<std::string> results;
  for (const base::Value& it : *history) {
    const base::Value::Dict* value_dict = it.GetIfDict();
    if (value_dict == nullptr) {
      continue;
    }
    const std::string* text =
        value_dict->FindString(kEmojiHistoryValueFieldName);
    if (text != nullptr) {
      results.push_back(*text);
    }
  }
  return results;
}

void PickerEmojiHistoryModel::UpdateRecentEmoji(
    ui::EmojiPickerCategory category,
    std::string_view latest_emoji) {
  std::vector<std::string> history = GetRecentEmojis(category);
  base::Value::List history_value;
  history_value.Append(
      base::Value::Dict().Set(kEmojiHistoryValueFieldName, latest_emoji));
  for (const std::string& value : history) {
    if (value == latest_emoji) {
      continue;
    }
    history_value.Append(
        base::Value::Dict().Set(kEmojiHistoryValueFieldName, value));
    if (history_value.size() == kMaxRecentEmoji) {
      break;
    }
  }

  ScopedDictPrefUpdate update(&prefs_.get(), prefs::kEmojiPickerHistory);
  update->Set(ConvertEmojiCategoryToString(category), std::move(history_value));
}

}  // namespace ash
