// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_emoji_history_model.h"

#include "ash/constants/ash_pref_names.h"
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
  update->Set("emoji", base::Value::List()
                           .Append(base::Value::Dict().Set("text", "abc"))
                           .Append(base::Value::Dict().Set("text", "xyz")));
  PickerEmojiHistoryModel model(pref_service());

  EXPECT_THAT(model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
              ElementsAre("abc", "xyz"));
}

TEST_F(PickerEmojiHistoryModelTest, AddsNewRecentEmoji) {
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", base::Value::List()
                           .Append(base::Value::Dict().Set("text", "abc"))
                           .Append(base::Value::Dict().Set("text", "xyz")));
  PickerEmojiHistoryModel model(pref_service());

  model.UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, "def");

  EXPECT_THAT(model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
              ElementsAre("def", "abc", "xyz"));
}

TEST_F(PickerEmojiHistoryModelTest, AddsExistingRecentEmoji) {
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", base::Value::List()
                           .Append(base::Value::Dict().Set("text", "abc"))
                           .Append(base::Value::Dict().Set("text", "xyz")));
  PickerEmojiHistoryModel model(pref_service());

  model.UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, "xyz");

  EXPECT_THAT(model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
              ElementsAre("xyz", "abc"));
}

TEST_F(PickerEmojiHistoryModelTest, AddsRecentEmojiEmptyHistory) {
  PickerEmojiHistoryModel model(pref_service());

  model.UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, "abc");

  EXPECT_THAT(model.GetRecentEmojis(ui::EmojiPickerCategory::kEmojis),
              ElementsAre("abc"));
}

}  // namespace
}  // namespace ash
