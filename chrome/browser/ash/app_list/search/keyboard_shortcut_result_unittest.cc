// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace app_list::test {

using ::ash::string_matching::TokenizedString;
using ::ui::KeyboardCode;
using TextVector = ChromeSearchResult::TextVector;
using TextType = ::ash::SearchResultTextItemType;
using ash::shortcut_customization::mojom::SearchResult;
using ash::shortcut_customization::mojom::SearchResultPtr;

class KeyboardShortcutResultTest : public ChromeAshTestBase {
 public:
  TextVector GetTextVectorFromTemplate(
      const std::u16string& template_string,
      const std::vector<std::u16string>& replacement_strings,
      const std::vector<ui::KeyboardCode>& shortcut_key_codes) {
    return KeyboardShortcutResult::CreateTextVectorFromTemplateString(
        template_string, replacement_strings, shortcut_key_codes);
  }

  void PopulateTextVector(TextVector* text_vector,
                          std::vector<std::u16string>& accessible_name,
                          const ui::Accelerator& accelerator) {
    auto shortcut_result = CreateKeyboardShortcutResult();
    shortcut_result->PopulateTextVector(text_vector, accessible_name,
                                        accelerator);
  }

  void PopulateTextVectorWithText(
      TextVector* text_vector,
      std::vector<std::u16string>& accessible_name,
      const std::vector<ash::mojom::TextAcceleratorPartPtr>& text_parts) {
    auto shortcut_result = CreateKeyboardShortcutResult();
    shortcut_result->PopulateTextVectorWithTextParts(
        text_vector, accessible_name, text_parts);
  }

  void PopulateTextVectorForNoShortcut(
      TextVector* text_vector,
      std::vector<std::u16string>& accessible_name) {
    auto shortcut_result = CreateKeyboardShortcutResult();
    shortcut_result->PopulateTextVectorForNoShortcut(text_vector,
                                                     accessible_name);
  }

  std::unique_ptr<KeyboardShortcutResult> CreateKeyboardShortcutResult() {
    const auto& search_results =
        ash::shortcut_ui::fake_search_data::CreateFakeSearchResultList();
    return std::make_unique<KeyboardShortcutResult>(
        /* profile= */ nullptr, search_results.at(0));
  }

  void VerifyTextItem(const ash::SearchResultTextItem& item,
                      const std::u16string& expected_text,
                      TextType expected_type) {
    switch (expected_type) {
      case ash::SearchResultTextItemType::kString:
      case ash::SearchResultTextItemType::kIconifiedText:
        EXPECT_EQ(item.GetText(), expected_text);
        break;
      case ash::SearchResultTextItemType::kIconCode:
        EXPECT_NE(item.GetIconFromCode(), nullptr);
        break;
      case ash::SearchResultTextItemType::kCustomImage:
        break;
    }
    // Cast to int to see actual values when not matched.
    EXPECT_EQ(static_cast<int>(item.GetType()),
              static_cast<int>(expected_type));
  }

  ash::mojom::AcceleratorInfoPtr CreateFakeStandardAcceleratorInfo(
      const ui::Accelerator& accelerator) {
    return ash::mojom::AcceleratorInfo::New(
        /*type=*/ash::mojom::AcceleratorType::kDefault,
        /*state=*/ash::mojom::AcceleratorState::kEnabled,
        /*locked=*/true,
        /*layout_properties=*/
        ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
            ash::mojom::StandardAcceleratorProperties::New(
                accelerator, u"FakeKey", absl::nullopt)));
  }

  std::vector<ash::mojom::AcceleratorInfoPtr> CreateFakeAcceleratorInfoList(
      const std::vector<ui::Accelerator>& accelerators) {
    std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
    for (const auto& accelerator : accelerators) {
      accelerator_info_list.push_back(
          CreateFakeStandardAcceleratorInfo(accelerator));
    }
    return accelerator_info_list;
  }

  std::string GetCategory(
      const std::unique_ptr<KeyboardShortcutResult>& result) {
    return result->accelerator_category_;
  }

  std::string GetAction(const std::unique_ptr<KeyboardShortcutResult>& result) {
    return result->accelerator_action_;
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

// Test that KeyBoardShortCutResult can take search results with standard
// accelerators.
TEST_F(KeyboardShortcutResultTest, StandardAcceleratorToResult) {
  std::vector<ui::Accelerator> accelerators;
  accelerators.emplace_back(/*key_code=*/ui::KeyboardCode::VKEY_F,
                            /*modifiers=*/0);
  auto list = CreateFakeAcceleratorInfoList(accelerators);

  SearchResultPtr search_result_ptr = SearchResult::New(
      /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"first result",
          /*source=*/ash::mojom::AcceleratorSource::kAsh,
          /*action=*/
          ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      /*accelerator_infos=*/
      CreateFakeAcceleratorInfoList(accelerators),
      /*relevance_score=*/0.5);

  auto result = std::make_unique<KeyboardShortcutResult>(
      /* profile= */ nullptr, search_result_ptr);

  EXPECT_TRUE(search_result_ptr->accelerator_infos[0]
                  ->layout_properties->is_standard_accelerator());
  EXPECT_EQ("1", GetAction(result));
  EXPECT_EQ("6", GetCategory(result));
  // 1: kActionId1=1;  6: Category = kDebug.
  EXPECT_EQ("keyboard_shortcut://1/6", result->id());
  EXPECT_EQ(0.5, result->relevance());
  EXPECT_EQ(u"first result", result->title());
  EXPECT_EQ(KeyboardShortcutResult::ResultType::kKeyboardShortcut,
            result->result_type());
  EXPECT_EQ(ash::KEYBOARD_SHORTCUT, result->metrics_type());
  EXPECT_EQ(KeyboardShortcutResult::DisplayType::kList, result->display_type());
  EXPECT_EQ(ash::AppListSearchResultCategory::kHelp, result->category());
  EXPECT_EQ(u"Key Shortcuts", result->details());

  const std::u16string expected_accessible_name =
      u"first result, Key Shortcuts,  the f key";
  EXPECT_EQ(expected_accessible_name, result->accessible_name());
  // Verify TextVector size.
  const TextVector text_vector = result->keyboard_shortcut_text_vector();
  ASSERT_EQ(text_vector.size(), 1u);
}

TEST_F(KeyboardShortcutResultTest, PopulateTextVector_One_Key) {
  ui::Accelerator accelerator(/*key_code=*/ui::KeyboardCode::VKEY_SPACE,
                              /*modifiers=*/0);
  TextVector text_vector;
  std::vector<std::u16string> accessible_name;
  PopulateTextVector(&text_vector, accessible_name, accelerator);

  ASSERT_EQ(text_vector.size(), 1u);
  VerifyTextItem(text_vector[0], u"space", TextType::kIconifiedText);
}

// The shortcuts app uses the following order:
//  SEARCH, CTRL, ALT, SHIFT.
TEST_F(KeyboardShortcutResultTest, PopulateTextVector_ModifierKeysOrder) {
  ui::Accelerator accelerator(/*key_code=*/ui::KeyboardCode::VKEY_F,
                              /*modifiers=*/ui::EF_ALT_DOWN |
                                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN |
                                  ui::EF_CONTROL_DOWN);
  TextVector text_vector;
  std::vector<std::u16string> accessible_name;
  PopulateTextVector(&text_vector, accessible_name, accelerator);

  ASSERT_EQ(text_vector.size(), 5u);
  VerifyTextItem(text_vector[0], u"search", TextType::kIconCode);
  VerifyTextItem(text_vector[1], u"ctrl", TextType::kIconifiedText);
  VerifyTextItem(text_vector[2], u"alt", TextType::kIconifiedText);
  VerifyTextItem(text_vector[3], u"shift", TextType::kIconifiedText);
  VerifyTextItem(text_vector[4], u"f", TextType::kIconifiedText);
}

TEST_F(KeyboardShortcutResultTest,
       OneActionWithTwoAccelerators_ShouldBeSeparatedBy_Or) {
  std::vector<ui::Accelerator> accelerators;
  accelerators.emplace_back(/*key_code=*/ui::KeyboardCode::VKEY_F,
                            /*modifiers=*/ui::EF_ALT_DOWN);
  accelerators.emplace_back(/*key_code=*/ui::KeyboardCode::VKEY_G,
                            /*modifiers=*/ui::EF_CONTROL_DOWN);
  auto list = CreateFakeAcceleratorInfoList(accelerators);

  SearchResultPtr search_result_ptr = SearchResult::New(
      /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"fake action",
          /*source=*/ash::mojom::AcceleratorSource::kAsh,
          /*action=*/
          ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      /*accelerator_infos=*/
      CreateFakeAcceleratorInfoList(accelerators),
      /*relevance_score=*/0.9);

  auto result = std::make_unique<KeyboardShortcutResult>(
      /* profile= */ nullptr, search_result_ptr);
  const auto& text_vector = result->keyboard_shortcut_text_vector();
  ASSERT_EQ(text_vector.size(), 5u);
  VerifyTextItem(text_vector[0], u"alt", TextType::kIconifiedText);
  VerifyTextItem(text_vector[1], u"f", TextType::kIconifiedText);
  VerifyTextItem(text_vector[2], u" or ", TextType::kString);
  VerifyTextItem(text_vector[3], u"ctrl", TextType::kIconifiedText);
  VerifyTextItem(text_vector[4], u"g", TextType::kIconifiedText);

  std::u16string expected_accessible_name =
      u"fake action, Key Shortcuts,  the alt key the f key  or  the ctrl key "
      u"the g key";
  EXPECT_EQ(expected_accessible_name, result->accessible_name());
}

TEST_F(KeyboardShortcutResultTest,
       OneActionWithThreeAccelerators_ShouldDisplayTheFirstTwo) {
  std::vector<ui::Accelerator> accelerators;
  accelerators.emplace_back(/*key_code=*/ui::KeyboardCode::VKEY_F,
                            /*modifiers=*/ui::EF_ALT_DOWN);
  accelerators.emplace_back(/*key_code=*/ui::KeyboardCode::VKEY_A,
                            /*modifiers=*/ui::EF_SHIFT_DOWN);
  accelerators.emplace_back(/*key_code=*/ui::KeyboardCode::VKEY_G,
                            /*modifiers=*/ui::EF_CONTROL_DOWN);
  auto list = CreateFakeAcceleratorInfoList(accelerators);

  SearchResultPtr search_result_ptr = SearchResult::New(
      /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"fake action",
          /*source=*/ash::mojom::AcceleratorSource::kAsh,
          /*action=*/
          ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      /*accelerator_infos=*/
      CreateFakeAcceleratorInfoList(accelerators),
      /*relevance_score=*/0.9);

  auto result = std::make_unique<KeyboardShortcutResult>(
      /* profile= */ nullptr, search_result_ptr);
  const auto& text_vector = result->keyboard_shortcut_text_vector();
  ASSERT_EQ(text_vector.size(), 5u);
  VerifyTextItem(text_vector[0], u"alt", TextType::kIconifiedText);
  VerifyTextItem(text_vector[1], u"f", TextType::kIconifiedText);
  VerifyTextItem(text_vector[2], u" or ", TextType::kString);
  VerifyTextItem(text_vector[3], u"shift", TextType::kIconifiedText);
  VerifyTextItem(text_vector[4], u"a", TextType::kIconifiedText);

  std::u16string expected_accessible_name =
      u"fake action, Key Shortcuts,  the alt key the f key  or  the shift key "
      u"the a key";
  EXPECT_EQ(expected_accessible_name, result->accessible_name());
}

TEST_F(KeyboardShortcutResultTest, PopulateTextVectorWithText) {
  std::vector<ash::mojom::TextAcceleratorPartPtr> text_parts;
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"Press ", ash::mojom::TextAcceleratorPartType::kPlainText));
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"Ctrl", ash::mojom::TextAcceleratorPartType::kModifier));
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"A", ash::mojom::TextAcceleratorPartType::kKey));

  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"Or ", ash::mojom::TextAcceleratorPartType::kPlainText));
  // Add a key with icon code.
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"ArrowLeft", ash::mojom::TextAcceleratorPartType::kKey));

  TextVector text_vector;
  std::vector<std::u16string> accessible_names;
  PopulateTextVectorWithText(&text_vector, accessible_names, text_parts);

  ASSERT_EQ(text_vector.size(), 5u);
  VerifyTextItem(text_vector[0], u"Press ", TextType::kString);
  VerifyTextItem(text_vector[1], u"ctrl", TextType::kIconifiedText);
  VerifyTextItem(text_vector[2], u"a", TextType::kIconifiedText);
  VerifyTextItem(text_vector[3], u"Or ", TextType::kString);
  VerifyTextItem(text_vector[4], u"", TextType::kIconCode);

  std::u16string expected_accessible_name =
      u"Press  the ctrl key the a key Or  the arrow left key";
  EXPECT_EQ(expected_accessible_name, base::JoinString(accessible_names, u" "));
}

TEST_F(KeyboardShortcutResultTest, PopulateTextVectorForNoShortcut) {
  TextVector text_vector;
  std::vector<std::u16string> accessible_names;
  PopulateTextVectorForNoShortcut(&text_vector, accessible_names);

  ASSERT_EQ(text_vector.size(), 1u);
  VerifyTextItem(text_vector[0], u"No shortcut assigned", TextType::kString);
  std::u16string expected_accessible_name = u"No shortcut assigned";
  EXPECT_EQ(expected_accessible_name, base::JoinString(accessible_names, u" "));
}

}  // namespace app_list::test
