// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_EMOJI_HISTORY_MODEL_H_
#define ASH_PICKER_MODEL_PICKER_EMOJI_HISTORY_MODEL_H_

#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ref.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "ui/base/emoji/emoji_panel_helper.h"

class PrefService;

namespace ash {

class ASH_EXPORT PickerEmojiHistoryModel {
 public:
  struct EmojiHistoryItem {
    std::string text;
    ui::EmojiPickerCategory category;
    base::Time timestamp;

    bool operator==(const EmojiHistoryItem&) const;
  };

  explicit PickerEmojiHistoryModel(
      PrefService* prefs,
      base::Clock* clock = base::DefaultClock::GetInstance());

  // Returns the list of recent emojis for `category`.
  std::vector<EmojiHistoryItem> GetRecentEmojis(
      ui::EmojiPickerCategory category) const;

  // Updates the recent emojis for `category` with `latest_emoji`.
  void UpdateRecentEmoji(ui::EmojiPickerCategory category,
                         std::string_view latest_emoji);

 private:
  raw_ref<PrefService> prefs_;
  raw_ptr<base::Clock> clock_;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_EMOJI_HISTORY_MODEL_H_
