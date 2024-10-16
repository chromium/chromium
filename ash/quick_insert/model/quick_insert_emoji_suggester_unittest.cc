// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_emoji_suggester.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_insert/model/quick_insert_emoji_history_model.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

class QuickInsertEmojiSuggesterTest : public testing::Test {
 public:
  QuickInsertEmojiSuggesterTest() {
    prefs_.registry()->RegisterDictionaryPref(prefs::kEmojiPickerHistory);
  }

  PrefService* pref_service() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

PickerEmojiSuggester::GetNameCallback GetName() {
  return base::BindRepeating([](std::string_view emoji) -> std::string {
    return base::StrCat({emoji, " name"});
  });
}

TEST_F(QuickInsertEmojiSuggesterTest, ReturnsDefaultEmojis) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model, GetName());

  EXPECT_THAT(suggester.GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"🙂", u"🙂 name"),
                          QuickInsertEmojiResult::Emoji(u"😂", u"😂 name"),
                          QuickInsertEmojiResult::Emoji(u"🤔", u"🤔 name"),
                          QuickInsertEmojiResult::Emoji(u"😢", u"😢 name"),
                          QuickInsertEmojiResult::Emoji(u"👏", u"👏 name"),
                          QuickInsertEmojiResult::Emoji(u"👍", u"👍 name")));
}

TEST_F(QuickInsertEmojiSuggesterTest,
       ReturnsRecentEmojiFollowedByDefaultEmojis) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model, GetName());
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  EXPECT_THAT(suggester.GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"abc", u"abc name"),
                          QuickInsertEmojiResult::Emoji(u"xyz", u"xyz name"),
                          QuickInsertEmojiResult::Emoji(u"🙂", u"🙂 name"),
                          QuickInsertEmojiResult::Emoji(u"😂", u"😂 name"),
                          QuickInsertEmojiResult::Emoji(u"🤔", u"🤔 name"),
                          QuickInsertEmojiResult::Emoji(u"😢", u"😢 name")));
}

TEST_F(QuickInsertEmojiSuggesterTest, SuggestedEmojiDoesNotContainDup) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model, GetName());
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "😂"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(pref_service(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  EXPECT_THAT(suggester.GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"😂", u"😂 name"),
                          QuickInsertEmojiResult::Emoji(u"xyz", u"xyz name"),
                          QuickInsertEmojiResult::Emoji(u"🙂", u"🙂 name"),
                          QuickInsertEmojiResult::Emoji(u"🤔", u"🤔 name"),
                          QuickInsertEmojiResult::Emoji(u"😢", u"😢 name"),
                          QuickInsertEmojiResult::Emoji(u"👏", u"👏 name")));
}

TEST_F(QuickInsertEmojiSuggesterTest, ReturnsRecentEmojiEmoticonAndSymbol) {
  PickerEmojiHistoryModel model(pref_service());
  PickerEmojiSuggester suggester(&model, GetName());
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

  EXPECT_THAT(
      suggester.GetSuggestedEmoji(),
      ElementsAre(
          QuickInsertEmojiResult::Symbol(u"symbol1", u"symbol1 name"),
          QuickInsertEmojiResult::Emoticon(u"emoticon1", u"emoticon1 name"),
          QuickInsertEmojiResult::Emoji(u"emoji1", u"emoji1 name"),
          QuickInsertEmojiResult::Symbol(u"symbol2", u"symbol2 name"),
          QuickInsertEmojiResult::Emoji(u"emoji2", u"emoji2 name"),
          QuickInsertEmojiResult::Emoticon(u"emoticon2", u"emoticon2 name")));
}

}  // namespace
}  // namespace ash
