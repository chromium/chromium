// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_emoji_suggester.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/picker/model/picker_emoji_history_model.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

class PickerEmojiSuggesterTest : public testing::Test {
 public:
  PickerEmojiSuggesterTest() {
    prefs_.registry()->RegisterDictionaryPref(prefs::kEmojiPickerHistory);
  }

  PrefService* pref_service() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(PickerEmojiSuggesterTest, ReturnsDefaultEmojis) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model);

  EXPECT_THAT(
      suggester.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"🙂"), PickerSearchResult::Emoji(u"😂"),
          PickerSearchResult::Emoji(u"🤔"), PickerSearchResult::Emoji(u"😢"),
          PickerSearchResult::Emoji(u"👏"), PickerSearchResult::Emoji(u"👍")));
}

TEST_F(PickerEmojiSuggesterTest, ReturnsRecentEmojiFollowedByDefaultEmojis) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model);
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  EXPECT_THAT(
      suggester.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"abc"), PickerSearchResult::Emoji(u"xyz"),
          PickerSearchResult::Emoji(u"🙂"), PickerSearchResult::Emoji(u"😂"),
          PickerSearchResult::Emoji(u"🤔"), PickerSearchResult::Emoji(u"😢")));
}

TEST_F(PickerEmojiSuggesterTest, SuggestedEmojiDoesNotContainDup) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model);
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "😂"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  EXPECT_THAT(
      suggester.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"😂"), PickerSearchResult::Emoji(u"xyz"),
          PickerSearchResult::Emoji(u"🙂"), PickerSearchResult::Emoji(u"🤔"),
          PickerSearchResult::Emoji(u"😢"), PickerSearchResult::Emoji(u"👏")));
}

TEST_F(PickerEmojiSuggesterTest, ReturnsRecentEmojiEmoticonAndSymbol) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model);
  base::Value::List emoji_history_value;
  emoji_history_value.Append(
      base::Value::Dict().Set("text", "emoji1").Set("timestamp", "10"));
  emoji_history_value.Append(
      base::Value::Dict().Set("text", "emoji2").Set("timestamp", "5"));
  base::Value::List emoticon_history_value;
  emoticon_history_value.Append(
      base::Value::Dict().Set("text", "emoticon1").Set("timestamp", "12"));
  emoticon_history_value.Append(
      base::Value::Dict().Set("text", "emoticon2").Set("timestamp", "2"));
  base::Value::List symbol_history_value;
  symbol_history_value.Append(
      base::Value::Dict().Set("text", "symbol1").Set("timestamp", "15"));
  symbol_history_value.Append(
      base::Value::Dict().Set("text", "symbol2").Set("timestamp", "8"));
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(emoji_history_value));
  update->Set("emoticon", std::move(emoticon_history_value));
  update->Set("symbol", std::move(symbol_history_value));

  EXPECT_THAT(suggester.GetSuggestedEmoji(),
              ElementsAre(PickerSearchResult::Symbol(u"symbol1"),
                          PickerSearchResult::Emoticon(u"emoticon1"),
                          PickerSearchResult::Emoji(u"emoji1"),
                          PickerSearchResult::Symbol(u"symbol2"),
                          PickerSearchResult::Emoji(u"emoji2"),
                          PickerSearchResult::Emoticon(u"emoticon2")));
}

}  // namespace
}  // namespace ash
