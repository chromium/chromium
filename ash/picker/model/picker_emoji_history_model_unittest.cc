// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_emoji_history_model.h"

#include "ash/constants/ash_pref_names.h"
#include "base/json/values_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using HistoryItem = PickerEmojiHistoryModel::EmojiHistoryItem;

base::Time TimeFromMicroSeconds(int64_t microseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(microseconds));
}

class PickerEmojiHistoryModelTest : public testing::Test {
 public:
  PickerEmojiHistoryModelTest() {
    prefs_.registry()->RegisterDictionaryPref(prefs::kEmojiPickerHistory);
  }

  PrefService* pref_service() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(PickerEmojiHistoryModelTest, ReturnsRecentEmojisFromPrefs) {
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set(
      "emoji",
      base::Value::List()
          .Append(base::Value::Dict().Set("text", "abc").Set("timestamp", "10"))
          .Append(
              base::Value::Dict().Set("text", "xyz").Set("timestamp", "5")));
  PickerEmojiHistoryModel model(pref_service());

  EXPECT_THAT(
      model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
      ElementsAre(HistoryItem{.text = "abc",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(10)},
                  HistoryItem{.text = "xyz",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(5)}));
}

TEST_F(PickerEmojiHistoryModelTest, AddsNewRecentEmoji) {
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set(
      "emoji",
      base::Value::List()
          .Append(base::Value::Dict().Set("text", "abc").Set("timestamp", "10"))
          .Append(
              base::Value::Dict().Set("text", "xyz").Set("timestamp", "5")));
  base::SimpleTestClock clock;
  PickerEmojiHistoryModel model(pref_service(), &clock);
  clock.SetNow(TimeFromMicroSeconds(20));

  model.UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, "def");

  EXPECT_THAT(
      model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
      ElementsAre(HistoryItem{.text = "def",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(20)},
                  HistoryItem{.text = "abc",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(10)},
                  HistoryItem{.text = "xyz",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(5)}));
}

TEST_F(PickerEmojiHistoryModelTest, AddsExistingRecentEmoji) {
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set(
      "emoji",
      base::Value::List()
          .Append(base::Value::Dict().Set("text", "abc").Set("timestamp", "10"))
          .Append(
              base::Value::Dict().Set("text", "xyz").Set("timestamp", "5")));
  base::SimpleTestClock clock;
  PickerEmojiHistoryModel model(pref_service(), &clock);
  clock.SetNow(TimeFromMicroSeconds(20));

  model.UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, "xyz");

  EXPECT_THAT(
      model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
      ElementsAre(HistoryItem{.text = "xyz",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(20)},
                  HistoryItem{.text = "abc",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(10)}));
}

TEST_F(PickerEmojiHistoryModelTest, AddsRecentEmojiEmptyHistory) {
  base::SimpleTestClock clock;
  PickerEmojiHistoryModel model(pref_service(), &clock);
  clock.SetNow(TimeFromMicroSeconds(5));

  model.UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, "abc");

  EXPECT_THAT(
      model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
      ElementsAre(HistoryItem{.text = "abc",
                              .category = ui::EmojiPickerCategory::kEmojis,
                              .timestamp = TimeFromMicroSeconds(5)}));
}

}  // namespace
}  // namespace ash
