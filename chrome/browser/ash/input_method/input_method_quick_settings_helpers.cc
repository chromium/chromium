// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_quick_settings_helpers.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ash {
namespace input_method {
namespace {

namespace mojom = ash::ime::mojom;

// The values of these keys don't matter as long as they are unique.
// They are set to these values for readability.
constexpr char kChineseQuickSettingsChineseKey[] = "Chinese.Chinese";
constexpr char kChineseQuickSettingsFullWidthCharacterKey[] =
    "Chinese.FullWidthCharacter";
constexpr char kChineseQuickSettingsFullWidthPunctuationKey[] =
    "Chinese.FullWidthPunctuation";
constexpr char kJapaneseQuickSettingsHiragana[] = "Japanese.Hiragana";
constexpr char kJapaneseQuickSettingsKatakana[] = "Japanese.Katakana";
constexpr char kJapaneseQuickSettingsWideLatin[] = "Japanese.WideLatin";
constexpr char kJapaneseQuickSettingsHalfWidthKatakana[] =
    "Japanese.HalfWideKatakana";
constexpr char kJapaneseQuickSettingsLatin[] = "Japanese.Latin";
constexpr char kJapaneseQuickSettingsDirectInput[] = "Japanese.DirectInput";

std::vector<ui::ime::InputMethodMenuItem> CreateChineseMenuItems(
    const mojom::ChineseQuickSettings& quick_settings) {
  std::vector<ui::ime::InputMethodMenuItem> menu_items;
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kChineseQuickSettingsChineseKey,
      l10n_util::GetStringUTF8(IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_CHINESE),
      quick_settings.mode == mojom::ChineseLanguageMode::kChinese));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kChineseQuickSettingsFullWidthCharacterKey,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_FULL_WIDTH_CHARACTER),
      quick_settings.full_width_characters));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kChineseQuickSettingsFullWidthPunctuationKey,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_CHINESE_QUICK_SETTINGS_FULL_WIDTH_PUNCTUATION),
      quick_settings.full_width_punctuation));
  return menu_items;
}

std::vector<ui::ime::InputMethodMenuItem> CreateJapaneseMenuItems(
    const mojom::JapaneseQuickSettings& quick_settings) {
  std::vector<ui::ime::InputMethodMenuItem> menu_items;
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kJapaneseQuickSettingsHiragana,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_HIRAGANA),
      quick_settings.mode == mojom::JapaneseInputMode::kHiragana));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kJapaneseQuickSettingsKatakana,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_KATAKANA),
      quick_settings.mode == mojom::JapaneseInputMode::kKatakana));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kJapaneseQuickSettingsWideLatin,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_WIDE_LATIN),
      quick_settings.mode == mojom::JapaneseInputMode::kWideLatin));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kJapaneseQuickSettingsHalfWidthKatakana,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_HALF_WIDTH_KATAKANA),
      quick_settings.mode == mojom::JapaneseInputMode::kHalfWidthKatakana));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kJapaneseQuickSettingsLatin,
      l10n_util::GetStringUTF8(IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_LATIN),
      quick_settings.mode == mojom::JapaneseInputMode::kLatin));
  menu_items.push_back(ui::ime::InputMethodMenuItem(
      kJapaneseQuickSettingsDirectInput,
      l10n_util::GetStringUTF8(
          IDS_CHROMEOS_IME_JAPANESE_QUICK_SETTINGS_DIRECT_INPUT),
      quick_settings.mode == mojom::JapaneseInputMode::kDirectInput));
  return menu_items;
}

mojom::ChineseQuickSettingsPtr GetChineseQuickSettingsAfterToggle(
    const std::vector<ui::ime::InputMethodMenuItem>& menu_items,
    std::string toggled_item_key) {
  CHECK_EQ(menu_items.size(), 3u);
  auto quick_settings = mojom::ChineseQuickSettings::New();
  quick_settings->mode = menu_items[0].is_selection_item_checked
                             ? mojom::ChineseLanguageMode::kChinese
                             : mojom::ChineseLanguageMode::kRaw;
  quick_settings->full_width_characters =
      menu_items[1].is_selection_item_checked;
  quick_settings->full_width_punctuation =
      menu_items[2].is_selection_item_checked;

  if (toggled_item_key == kChineseQuickSettingsChineseKey) {
    quick_settings->mode =
        quick_settings->mode == mojom::ChineseLanguageMode::kChinese
            ? mojom::ChineseLanguageMode::kRaw
            : mojom::ChineseLanguageMode::kChinese;
  } else if (toggled_item_key == kChineseQuickSettingsFullWidthCharacterKey) {
    quick_settings->full_width_characters =
        !quick_settings->full_width_characters;
  } else if (toggled_item_key == kChineseQuickSettingsFullWidthPunctuationKey) {
    quick_settings->full_width_punctuation =
        !quick_settings->full_width_punctuation;
  }

  return quick_settings;
}

mojom::JapaneseInputMode JapaneseInputModeFromMenuItemKey(
    const std::string& key) {
  if (key == kJapaneseQuickSettingsHiragana) {
    return mojom::JapaneseInputMode::kHiragana;
  }
  if (key == kJapaneseQuickSettingsKatakana) {
    return mojom::JapaneseInputMode::kKatakana;
  }
  if (key == kJapaneseQuickSettingsWideLatin) {
    return mojom::JapaneseInputMode::kWideLatin;
  }
  if (key == kJapaneseQuickSettingsHalfWidthKatakana) {
    return mojom::JapaneseInputMode::kHalfWidthKatakana;
  }
  if (key == kJapaneseQuickSettingsLatin) {
    return mojom::JapaneseInputMode::kLatin;
  }
  if (key == kJapaneseQuickSettingsDirectInput) {
    return mojom::JapaneseInputMode::kDirectInput;
  }
  return mojom::JapaneseInputMode::kHiragana;
}

mojom::JapaneseQuickSettingsPtr GetJapaneseQuickSettingsAfterToggle(
    const std::string& toggled_item_key) {
  auto quick_settings = mojom::JapaneseQuickSettings::New();
  quick_settings->mode = JapaneseInputModeFromMenuItemKey(toggled_item_key);
  return quick_settings;
}

}  // namespace

std::vector<ui::ime::InputMethodMenuItem> CreateMenuItemsFromQuickSettings(
    const mojom::InputMethodQuickSettings& quick_settings) {
  switch (quick_settings.which()) {
    case mojom::InputMethodQuickSettings::Tag::kChineseSettings:
      return CreateChineseMenuItems(*quick_settings.get_chinese_settings());
    case mojom::InputMethodQuickSettings::Tag::kJapaneseSettings:
      return CreateJapaneseMenuItems(*quick_settings.get_japanese_settings());
  }
}

mojom::InputMethodQuickSettingsPtr GetQuickSettingsAfterToggle(
    const std::vector<ui::ime::InputMethodMenuItem>& menu_items,
    std::string toggled_item_key) {
  if (toggled_item_key == kChineseQuickSettingsChineseKey ||
      toggled_item_key == kChineseQuickSettingsFullWidthCharacterKey ||
      toggled_item_key == kChineseQuickSettingsFullWidthPunctuationKey) {
    return mojom::InputMethodQuickSettings::NewChineseSettings(
        GetChineseQuickSettingsAfterToggle(menu_items, toggled_item_key));
  }
  return mojom::InputMethodQuickSettings::NewJapaneseSettings(
      GetJapaneseQuickSettingsAfterToggle(toggled_item_key));
}

}  // namespace input_method
}  // namespace ash
