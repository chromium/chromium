// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"

#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

const int kContextId = 24601;

TEST(LongpressDiacriticsSuggesterTest, SuggestsOnTrySuggest) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress('a');

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"à;á;â;ã;ã;ä;å;ā");
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
}

TEST(LongpressDiacriticsSuggesterTest, DoesNotSuggestForInvalidKeyChar) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress('~');  // Char doesn't have diacritics.

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST(LongpressDiacriticsSuggesterTest, DoesNotSuggestAfterBlur) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.OnBlur();
  suggester.TrySuggestOnLongpress('a');

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

}  // namespace
}  // namespace input_method
}  // namespace ash
