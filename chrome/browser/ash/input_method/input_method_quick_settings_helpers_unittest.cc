// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_quick_settings_helpers.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ash {
namespace input_method {
namespace {

namespace mojom = ime::mojom;

TEST(InputMethodQuickSettings, CreateChineseMenuItems) {
  auto quick_settings = mojom::ChineseQuickSettings::New();
  quick_settings->mode = mojom::ChineseLanguageMode::kRaw;
  quick_settings->full_width_characters = true;
  quick_settings->full_width_punctuation = true;

  std::vector<ui::ime::InputMethodMenuItem> items =
      CreateMenuItemsFromQuickSettings(
          *mojom::InputMethodQuickSettings::NewChineseSettings(
              std::move(quick_settings)));

  ASSERT_EQ(items.size(), 3U);
  EXPECT_EQ(items[0].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_CHINESE));
  EXPECT_FALSE(items[0].is_selection_item_checked);
  EXPECT_EQ(items[1].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_FULL_WIDTH_CHARACTER));
  EXPECT_TRUE(items[1].is_selection_item_checked);
  EXPECT_EQ(
      items[2].label,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_FULL_WIDTH_PUNCTUATION));
  EXPECT_TRUE(items[2].is_selection_item_checked);
}

TEST(InputMethodQuickSettings, CreateJapaneseMenuItems) {
  auto quick_settings = mojom::JapaneseQuickSettings::New();
  quick_settings->mode = mojom::JapaneseInputMode::kWideLatin;

  std::vector<ui::ime::InputMethodMenuItem> items =
      CreateMenuItemsFromQuickSettings(
          *mojom::InputMethodQuickSettings::NewJapaneseSettings(
              std::move(quick_settings)));

  ASSERT_EQ(items.size(), 6U);
  EXPECT_EQ(items[0].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_HIRAGANA));
  EXPECT_FALSE(items[0].is_selection_item_checked);
  EXPECT_EQ(items[1].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_KATAKANA));
  EXPECT_FALSE(items[1].is_selection_item_checked);
  EXPECT_EQ(items[2].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_WIDE_LATIN));
  EXPECT_TRUE(items[2].is_selection_item_checked);
  EXPECT_EQ(items[3].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_HALF_WIDTH_KATAKANA));
  EXPECT_FALSE(items[3].is_selection_item_checked);
  EXPECT_EQ(
      items[4].label,
      l10n_util::GetStringUTF8(IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_LATIN));
  EXPECT_FALSE(items[4].is_selection_item_checked);
  EXPECT_EQ(items[5].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_DIRECT_INPUT));
  EXPECT_FALSE(items[5].is_selection_item_checked);
}

TEST(InputMethodQuickSettings, ToggleChineseQuickSettings) {
  auto quick_settings = mojom::ChineseQuickSettings::New();
  quick_settings->mode = mojom::ChineseLanguageMode::kRaw;
  quick_settings->full_width_characters = false;
  quick_settings->full_width_punctuation = false;
  std::vector<ui::ime::InputMethodMenuItem> items =
      CreateMenuItemsFromQuickSettings(
          *mojom::InputMethodQuickSettings::NewChineseSettings(
              std::move(quick_settings)));

  // Toggle full width characters.
  ASSERT_EQ(items.size(), 3U);
  ASSERT_EQ(items[1].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_FULL_WIDTH_CHARACTER));
  const auto new_quick_settings =
      GetQuickSettingsAfterToggle(items, items[1].key);

  ASSERT_TRUE(new_quick_settings);
  ASSERT_TRUE(new_quick_settings->is_chinese_settings());
  const auto new_chinese_settings = *new_quick_settings->get_chinese_settings();
  EXPECT_EQ(new_chinese_settings.mode, mojom::ChineseLanguageMode::kRaw);
  EXPECT_TRUE(new_chinese_settings.full_width_characters);
  EXPECT_FALSE(new_chinese_settings.full_width_punctuation);
}

TEST(InputMethodQuickSettings, ToggleJapaneseQuickSettings) {
  auto quick_settings = mojom::JapaneseQuickSettings::New();
  quick_settings->mode = mojom::JapaneseInputMode::kHalfWidthKatakana;
  std::vector<ui::ime::InputMethodMenuItem> items =
      CreateMenuItemsFromQuickSettings(
          *mojom::InputMethodQuickSettings::NewJapaneseSettings(
              std::move(quick_settings)));

  // Toggle the Katakana menu item.
  ASSERT_EQ(items.size(), 6U);
  ASSERT_EQ(items[1].label,
            l10n_util::GetStringUTF8(
                IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_KATAKANA));
  const auto new_quick_settings =
      GetQuickSettingsAfterToggle(items, items[1].key);

  ASSERT_TRUE(new_quick_settings);
  ASSERT_TRUE(new_quick_settings->is_japanese_settings());
  EXPECT_EQ(new_quick_settings->get_japanese_settings()->mode,
            mojom::JapaneseInputMode::kKatakana);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
