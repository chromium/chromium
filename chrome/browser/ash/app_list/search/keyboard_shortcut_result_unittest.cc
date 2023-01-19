// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace app_list::test {

using ::ash::string_matching::TokenizedString;
using ::ui::KeyboardCode;
using TextVector = ChromeSearchResult::TextVector;
using TextType = ::ash::SearchResultTextItemType;

class KeyboardShortcutResultTest : public ChromeAshTestBase {
 public:
  TextVector GetTextVectorFromTemplate(
      const std::u16string& template_string,
      const std::vector<std::u16string>& replacement_strings,
      const std::vector<ui::KeyboardCode>& shortcut_key_codes) {
    return KeyboardShortcutResult::CreateTextVectorFromTemplateString(
        template_string, replacement_strings, shortcut_key_codes);
  }
};

TEST_F(KeyboardShortcutResultTest, CalculateRelevance) {
  const std::u16string query(u"minimize");
  const std::u16string target(u"Minimize window");

  const TokenizedString query_tokenized(query, TokenizedString::Mode::kWords);
  double relevance =
      KeyboardShortcutResult::CalculateRelevance(query_tokenized, target);

  EXPECT_GT(relevance, 0.5);
}

// Smoke test to ensure that our assumptions about the format of each keyboard
// shortcut result hold true.
TEST_F(KeyboardShortcutResultTest, MakeEveryResult) {
  // A DCHECK inside a KSV metadata utility function relies on device lists
  // being complete.
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  for (const auto& item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    KeyboardShortcutResult result(
        /*profile=*/nullptr, KeyboardShortcutData(item), /*relevance=*/0.1);
  }
  SUCCEED();
}

TEST_F(KeyboardShortcutResultTest,
       CreateTextVector_TemplateStartsWithPlaceholder) {
  // String representation of intended shortcut instructions:
  //   "Ctrl + l or Alt + d"
  const std::u16string template_string = u"$1$2$3 or $4$5$6";
  const std::vector<std::u16string>& replacement_strings = {
      u"Ctrl", u"+ ", u"l", u"Alt", u"+ ", u"d"};

  // N.B. VKEY_UNKNOWN is used to represent the "+" delimiter. See:
  // ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.cc for reasoning.
  const std::vector<KeyboardCode>& shortcut_key_codes = {
      KeyboardCode::VKEY_CONTROL, KeyboardCode::VKEY_UNKNOWN,
      KeyboardCode::VKEY_L,       KeyboardCode::VKEY_LMENU,
      KeyboardCode::VKEY_UNKNOWN, KeyboardCode::VKEY_D};

  const TextVector text_vector = GetTextVectorFromTemplate(
      template_string, replacement_strings, shortcut_key_codes);
  ASSERT_EQ(text_vector.size(), 7u);

  EXPECT_EQ(text_vector[0].GetType(), TextType::kIconifiedText);
  EXPECT_EQ(text_vector[0].GetText(), u"Ctrl");

  EXPECT_EQ(text_vector[1].GetType(), TextType::kString);
  EXPECT_EQ(text_vector[1].GetText(), u" + ");

  EXPECT_EQ(text_vector[2].GetType(), TextType::kIconifiedText);
  EXPECT_EQ(text_vector[2].GetText(), u"l");

  EXPECT_EQ(text_vector[3].GetType(), TextType::kString);
  EXPECT_EQ(text_vector[3].GetText(), u" or ");

  EXPECT_EQ(text_vector[4].GetType(), TextType::kIconifiedText);
  EXPECT_EQ(text_vector[4].GetText(), u"Alt");

  EXPECT_EQ(text_vector[5].GetType(), TextType::kString);
  EXPECT_EQ(text_vector[5].GetText(), u" + ");

  EXPECT_EQ(text_vector[6].GetType(), TextType::kIconifiedText);
  EXPECT_EQ(text_vector[6].GetText(), u"d");
}

TEST_F(KeyboardShortcutResultTest,
       CreateTextVector_TemplateStartsWithPlainText) {
  // String representation of intended shortcut instructions:
  //   "Press <capture mode key>, tap shift, and release."
  const std::u16string template_string = u"Press $1, tap $2, and release";
  const std::vector<std::u16string>& replacement_strings = {u"Capture mode key",
                                                            u"shift"};

  // N.B. VKEY_UNKNOWN is used to represent the "+" delimiter, which has a
  // string representation of "+ ". See
  // ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.cc for reasoning.
  const std::vector<KeyboardCode>& shortcut_key_codes = {
      KeyboardCode::VKEY_SNAPSHOT, KeyboardCode::VKEY_SHIFT};

  const TextVector text_vector = GetTextVectorFromTemplate(
      template_string, replacement_strings, shortcut_key_codes);
  ASSERT_EQ(text_vector.size(), 5u);

  EXPECT_EQ(text_vector[0].GetType(), TextType::kString);
  EXPECT_EQ(text_vector[0].GetText(), u"Press ");

  EXPECT_EQ(text_vector[1].GetType(), TextType::kIconCode);

  EXPECT_EQ(text_vector[2].GetType(), TextType::kString);
  EXPECT_EQ(text_vector[2].GetText(), u", tap ");

  EXPECT_EQ(text_vector[3].GetType(), TextType::kIconifiedText);
  EXPECT_EQ(text_vector[3].GetText(), u"shift");

  EXPECT_EQ(text_vector[4].GetType(), TextType::kString);
  EXPECT_EQ(text_vector[4].GetText(), u", and release");
}

}  // namespace app_list::test
