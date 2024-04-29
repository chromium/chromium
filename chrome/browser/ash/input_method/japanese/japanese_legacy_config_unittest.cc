// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/japanese/japanese_legacy_config.h"

#include "chromeos/ash/services/ime/public/mojom/user_data_japanese_legacy_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

using ::ash::ime::mojom::JapaneseLegacyConfig;

TEST(JapaneseLegacyConfigTest, TestMojomToPref) {
  base::Value::Dict response =
      CreatePrefsDictFromJapaneseLegacyConfig(JapaneseLegacyConfig::New(
          /*preedit_method=*/JapaneseLegacyConfig::PreeditMethod::kKana,
          /*punctuation_method=*/
          JapaneseLegacyConfig::PunctuationMethod::kCommaPeriod,
          /*symbol_method=*/
          JapaneseLegacyConfig::SymbolMethod::kCornerBracketMiddleDot,
          /*space_character_form=*/
          JapaneseLegacyConfig::FundamentalCharacterForm::kFullWidth,
          /*selection_shortcut=*/
          JapaneseLegacyConfig::SelectionShortcut::kAsdfghjkl,
          /*session_keymap=*/JapaneseLegacyConfig::SessionKeymap::kAtok,
          /*use_auto_conversion=*/true,
          /*shift_key_mode_switch=*/
          JapaneseLegacyConfig::ShiftKeyModeSwitch::kAsciiInputMode,
          /*use_history_suggest=*/true,
          /*use_dictionary_suggest=*/true,
          /*suggestion_size=*/8,
          /*incognito_mode=*/true,
          /*upload_usage_stats=*/false));

  base::Value::Dict expected;
  expected.Set("JapaneseInputMode", "Kana");
  expected.Set("JapanesePunctuationStyle", "CommaPeriod");
  expected.Set("JapaneseSymbolStyle", "CornerBracketMiddleDot");
  expected.Set("JapaneseSpaceInputStyle", "Fullwidth");
  expected.Set("JapaneseSectionShortcut", "ASDFGHJKL");
  expected.Set("JapaneseKeymapStyle", "Atok");
  expected.Set("AutomaticallySwitchToHalfwidth", true);
  expected.Set("ShiftKeyModeStyle", "Alphanumeric");
  expected.Set("JapaneseDisableSuggestions", true);
  expected.Set("UseSystemDictionary", true);
  expected.Set("numberOfSuggestions", 8);
  expected.Set("UseInputHistory", true);
  expected.Set("AutomaticallySendStatisticsToGoogle", false);

  EXPECT_EQ(response, expected);
}

}  // namespace
}  // namespace ash::input_method
