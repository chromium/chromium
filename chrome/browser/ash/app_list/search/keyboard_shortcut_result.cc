// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace app_list {

namespace {

using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;
using ::ash::string_matching::TokenizedStringMatch;
using TextVector = ChromeSearchResult::TextVector;
using IconCode = ::ash::SearchResultTextItem::IconCode;
using ::ui::KeyboardCode;

constexpr char kKeyboardShortcutScheme[] = "keyboard_shortcut://";

// The icon labels used by the shortcuts app can be found here:
// https://crsrc.org/c/ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.cc;l=125.
std::optional<int> GetStringIdForIconCode(IconCode icon_code) {
  switch (icon_code) {
    case ash::SearchResultTextItem::kKeyboardShortcutPower:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_POWER;
    case ash::SearchResultTextItem::kKeyboardShortcutKeyboardBacklightToggle:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BACKLIGHT_TOGGLE;
    case ash::SearchResultTextItem::kKeyboardShortcutMicrophone:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MICROPHONE_MUTE_TOGGLE;
    case ash::SearchResultTextItem::kKeyboardShortcutAssistant:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_ASSISTANT;
    case ash::SearchResultTextItem::kKeyboardShortcutAllApps:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_VIEW_ALL_APPS;
    case ash::SearchResultTextItem::kKeyboardShortcutBrowserBack:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_BACK;
    case ash::SearchResultTextItem::kKeyboardShortcutBrowserForward:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_FORWARD;
    case ash::SearchResultTextItem::kKeyboardShortcutBrowserHome:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_HOME;
    case ash::SearchResultTextItem::kKeyboardShortcutBrowserRefresh:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_REFRESH;
    case ash::SearchResultTextItem::kKeyboardShortcutBrowserSearch:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_SEARCH;
    case ash::SearchResultTextItem::kKeyboardShortcutContextMenu:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_CONTEXT_MENU;
    case ash::SearchResultTextItem::kKeyboardShortcutCalculator:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_APPLICATION2;
    case ash::SearchResultTextItem::kKeyboardShortcutDictationToggle:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ENABLE_OR_TOGGLE_DICTATION;
    case ash::SearchResultTextItem::kKeyboardShortcutEmojiPicker:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_EMOJI_PICKER;
    case ash::SearchResultTextItem::kKeyboardShortcutInputModeChange:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MODE_CHANGE;
    case ash::SearchResultTextItem::kKeyboardShortcutZoom:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ZOOM_TOGGLE;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaLaunchApp1:
    case ash::SearchResultTextItem::kKeyboardShortcutMediaLaunchApp1Refresh:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_APPLICATION1;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaFastForward:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_FAST_FORWARD;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaLaunchMail:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_MAIL;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaPause:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PAUSE;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaPlay:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PLAY;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaPlayPause:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PLAY_PAUSE;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaTrackNext:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_TRACK_NEXT;
    case ash::SearchResultTextItem::kKeyboardShortcutMediaTrackPrevious:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_TRACK_PREVIOUS;
    case ash::SearchResultTextItem::kKeyboardShortcutBrightnessDown:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_DOWN;
    case ash::SearchResultTextItem::kKeyboardShortcutKeyboardBrightnessDown:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_DOWN;
    case ash::SearchResultTextItem::kKeyboardShortcutBrightnessUp:
    case ash::SearchResultTextItem::kKeyboardShortcutBrightnessUpRefresh:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BRIGHTNESS_UP;
    case ash::SearchResultTextItem::kKeyboardShortcutKeyboardBrightnessUp:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_UP;
    case ash::SearchResultTextItem::kKeyboardShortcutVolumeMute:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_MUTE;
    case ash::SearchResultTextItem::kKeyboardShortcutVolumeDown:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_DOWN;
    case ash::SearchResultTextItem::kKeyboardShortcutVolumeUp:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_UP;
    case ash::SearchResultTextItem::kKeyboardShortcutUp:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_UP;
    case ash::SearchResultTextItem::kKeyboardShortcutDown:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_DOWN;
    case ash::SearchResultTextItem::kKeyboardShortcutLeft:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_LEFT;
    case ash::SearchResultTextItem::kKeyboardShortcutRight:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_RIGHT;
    case ash::SearchResultTextItem::kKeyboardShortcutPrivacyScreenToggle:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_PRIVACY_SCREEN_TOGGLE;
    case ash::SearchResultTextItem::kKeyboardShortcutSettings:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_SETTINGS;
    case ash::SearchResultTextItem::kKeyboardShortcutSnapshot:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_PRINT_SCREEN;
    case ash::SearchResultTextItem::kKeyboardShortcutLauncher:
    case ash::SearchResultTextItem::kKeyboardShortcutLauncherRefresh:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_OPEN_LAUNCHER;
    case ash::SearchResultTextItem::kKeyboardShortcutSearch:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_OPEN_SEARCH;
    case ash::SearchResultTextItem::kKeyboardShortcutAccessibility:
      return IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ACCESSIBILITY;
    case ash::SearchResultTextItem::kKeyboardShortcutKeyboardRightAlt:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return IDS_KEYBOARD_RIGHT_ALT_LABEL;
#else
      return IDS_SHORTCUT_CUSTOMIZATION_INPUT_KEY_PLACEHOLDER;
#endif
  }
}

std::u16string GetAccessibleStringForIcon(IconCode icon_code) {
  const std::optional<int> icon_code_string_id =
      GetStringIdForIconCode(icon_code);
  CHECK(icon_code_string_id.has_value());

  return l10n_util::GetStringFUTF16(
      IDS_SHORTCUT_CUSTOMIZATION_ARIA_LABEL_FOR_A_KEY,
      l10n_util::GetStringUTF16(icon_code_string_id.value()));
}

std::u16string GetAccessibleStringForKey(const std::u16string& key_string) {
  return l10n_util::GetStringFUTF16(
      IDS_SHORTCUT_CUSTOMIZATION_ARIA_LABEL_FOR_A_KEY, key_string);
}

bool IsModifierKey(ui::KeyboardCode keycode) {
  switch (keycode) {
    case ui::KeyboardCode::VKEY_COMMAND:
    case ui::KeyboardCode::VKEY_LMENU:
    case ui::KeyboardCode::VKEY_RCONTROL:
    case ui::KeyboardCode::VKEY_CONTROL:
    case ui::KeyboardCode::VKEY_SHIFT:
      return true;
    default:
      return false;
  }
}

}  // namespace

std::optional<IconCode> KeyboardShortcutResult::GetIconCodeFromKeyboardCode(
    KeyboardCode keyboard_code) {
  switch (keyboard_code) {
    case (KeyboardCode::VKEY_APPS):
      return IconCode::kKeyboardShortcutContextMenu;
    case (KeyboardCode::VKEY_BROWSER_BACK):
      return IconCode::kKeyboardShortcutBrowserBack;
    case (KeyboardCode::VKEY_BROWSER_FORWARD):
      return IconCode::kKeyboardShortcutBrowserForward;
    case (KeyboardCode::VKEY_BROWSER_HOME):
      return IconCode::kKeyboardShortcutBrowserHome;
    case (KeyboardCode::VKEY_BROWSER_REFRESH):
      return IconCode::kKeyboardShortcutBrowserRefresh;
    case (KeyboardCode::VKEY_BROWSER_SEARCH):
      return IconCode::kKeyboardShortcutBrowserSearch;
    case (KeyboardCode::VKEY_DICTATE):
      return IconCode::kKeyboardShortcutDictationToggle;
    case (KeyboardCode::VKEY_EMOJI_PICKER):
      return IconCode::kKeyboardShortcutEmojiPicker;
    case (KeyboardCode::VKEY_ZOOM):
      return IconCode::kKeyboardShortcutZoom;
    case (KeyboardCode::VKEY_MEDIA_LAUNCH_APP1):
      return ash::Shell::Get()->keyboard_capability()->UseRefreshedIcons()
                 ? IconCode::kKeyboardShortcutMediaLaunchApp1Refresh
                 : IconCode::kKeyboardShortcutMediaLaunchApp1;
    case (KeyboardCode::VKEY_MEDIA_LAUNCH_MAIL):
      return IconCode::kKeyboardShortcutMediaLaunchMail;
    case (KeyboardCode::VKEY_MEDIA_NEXT_TRACK):
      return IconCode::kKeyboardShortcutMediaTrackNext;
    case (KeyboardCode::VKEY_MEDIA_PREV_TRACK):
      return IconCode::kKeyboardShortcutMediaTrackPrevious;
    case (KeyboardCode::VKEY_MEDIA_PLAY):
      return IconCode::kKeyboardShortcutMediaPlay;
    case (KeyboardCode::VKEY_MEDIA_PAUSE):
      return IconCode::kKeyboardShortcutMediaPause;
    case (KeyboardCode::VKEY_MEDIA_PLAY_PAUSE):
      return IconCode::kKeyboardShortcutMediaPlayPause;
    case (KeyboardCode::VKEY_KBD_BACKLIGHT_TOGGLE):
      return IconCode::kKeyboardShortcutKeyboardBacklightToggle;
    case (KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN):
      return IconCode::kKeyboardShortcutKeyboardBrightnessDown;
    case (KeyboardCode::VKEY_KBD_BRIGHTNESS_UP):
      return IconCode::kKeyboardShortcutKeyboardBrightnessUp;
    case (KeyboardCode::VKEY_OEM_104):
      return IconCode::kKeyboardShortcutMediaFastForward;
    case (KeyboardCode::VKEY_BRIGHTNESS_DOWN):
      return IconCode::kKeyboardShortcutBrightnessDown;
    case (KeyboardCode::VKEY_BRIGHTNESS_UP):
      return ash::Shell::Get()->keyboard_capability()->UseRefreshedIcons()
                 ? IconCode::kKeyboardShortcutBrightnessUpRefresh
                 : IconCode::kKeyboardShortcutBrightnessUp;
    case (KeyboardCode::VKEY_VOLUME_MUTE):
      return IconCode::kKeyboardShortcutVolumeMute;
    case (KeyboardCode::VKEY_VOLUME_DOWN):
      return IconCode::kKeyboardShortcutVolumeDown;
    case (KeyboardCode::VKEY_VOLUME_UP):
      return IconCode::kKeyboardShortcutVolumeUp;
    case (KeyboardCode::VKEY_UP):
      return IconCode::kKeyboardShortcutUp;
    case (KeyboardCode::VKEY_DOWN):
      return IconCode::kKeyboardShortcutDown;
    case (KeyboardCode::VKEY_LEFT):
      return IconCode::kKeyboardShortcutLeft;
    case (KeyboardCode::VKEY_RIGHT):
      return IconCode::kKeyboardShortcutRight;
    case (KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE):
      return IconCode::kKeyboardShortcutPrivacyScreenToggle;
    case (KeyboardCode::VKEY_SETTINGS):
      return IconCode::kKeyboardShortcutSettings;
    case (KeyboardCode::VKEY_SNAPSHOT):
      return IconCode::kKeyboardShortcutSnapshot;
    case (KeyboardCode::VKEY_LWIN):
    case (KeyboardCode::VKEY_RWIN):
      // The search and launcher are the same. The icon we display is dependent
      // on a best-attempt heuristic on whether the chromebook internal keyboard
      // is a launcher or magnifier icon.
      switch (ash::Shell::Get()->keyboard_capability()->GetMetaKeyToDisplay()) {
        case ui::mojom::MetaKey::kSearch:
          return IconCode::kKeyboardShortcutSearch;
        case ui::mojom::MetaKey::kLauncher:
          return IconCode::kKeyboardShortcutLauncher;
        case ui::mojom::MetaKey::kLauncherRefresh:
          return IconCode::kKeyboardShortcutLauncherRefresh;
        case ui::mojom::MetaKey::kExternalMeta:
        case ui::mojom::MetaKey::kCommand:
          NOTREACHED();
      }
    case (KeyboardCode::VKEY_MEDIA_LAUNCH_APP2):
      return IconCode::kKeyboardShortcutCalculator;
    case (KeyboardCode::VKEY_ALL_APPLICATIONS):
      return IconCode::kKeyboardShortcutAllApps;
    case (KeyboardCode::VKEY_ASSISTANT):
      return IconCode::kKeyboardShortcutAssistant;
    case (KeyboardCode::VKEY_MODECHANGE):
      return IconCode::kKeyboardShortcutInputModeChange;
    case (KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE):
      return IconCode::kKeyboardShortcutMicrophone;
    case (KeyboardCode::VKEY_ACCESSIBILITY):
      return IconCode::kKeyboardShortcutAccessibility;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case (KeyboardCode::VKEY_RIGHT_ALT):
      return IconCode::kKeyboardShortcutKeyboardRightAlt;
#endif
    default:
      return std::nullopt;
  }
}

// This map matches the `keyToIconNameMap` of the shortcuts app frontend:
// https://crsrc.org/c/ash/webui/shortcut_customization_ui/resources/js/input_key.ts;l=30.
std::optional<ash::SearchResultTextItem::IconCode>
KeyboardShortcutResult::GetIconCodeByKeyString(std::u16string_view key_string) {
  static constexpr auto kIconCodes = base::MakeFixedFlatMap<std::u16string_view,
                                                            IconCode>(
      {{u"Accessibility", IconCode::kKeyboardShortcutAccessibility},
       {u"ArrowDown", IconCode::kKeyboardShortcutDown},
       {u"ArrowLeft", IconCode::kKeyboardShortcutLeft},
       {u"ArrowRight", IconCode::kKeyboardShortcutRight},
       {u"ArrowUp", IconCode::kKeyboardShortcutUp},
       {u"AudioVolumeDown", IconCode::kKeyboardShortcutVolumeDown},
       {u"AudioVolumeMute", IconCode::kKeyboardShortcutVolumeMute},
       {u"AudioVolumeUp", IconCode::kKeyboardShortcutVolumeUp},
       {u"BrightnessDown", IconCode::kKeyboardShortcutBrightnessDown},
       {u"BrightnessUp", IconCode::kKeyboardShortcutBrightnessUp},
       {u"BrowserBack", IconCode::kKeyboardShortcutBrowserBack},
       {u"BrowserForward", IconCode::kKeyboardShortcutBrowserForward},
       {u"BrowserHome", IconCode::kKeyboardShortcutBrowserHome},
       {u"BrowserRefresh", IconCode::kKeyboardShortcutBrowserRefresh},
       {u"BrowserSearch", IconCode::kKeyboardShortcutBrowserSearch},
       {u"EmojiPicker", IconCode::kKeyboardShortcutEmojiPicker},
       {u"EnableOrToggleDictation", IconCode::kKeyboardShortcutDictationToggle},
       {u"KeyboardBacklightToggle",
        IconCode::kKeyboardShortcutKeyboardBacklightToggle},
       {u"KeyboardBrightnessDown",
        IconCode::kKeyboardShortcutKeyboardBrightnessDown},
       {u"KeyboardBrightnessUp",
        IconCode::kKeyboardShortcutKeyboardBrightnessUp},
       {u"LaunchApplication1", IconCode::kKeyboardShortcutMediaLaunchApp1},
       {u"LaunchApplication2", IconCode::kKeyboardShortcutCalculator},
       {u"LaunchAssistant", IconCode::kKeyboardShortcutAssistant},
       {u"MediaFastForward", IconCode::kKeyboardShortcutMediaFastForward},
       {u"MediaLaunchMail", IconCode::kKeyboardShortcutMediaLaunchMail},
       {u"MediaPause", IconCode::kKeyboardShortcutMediaPause},
       {u"MediaPlay", IconCode::kKeyboardShortcutMediaPlay},
       {u"MediaPlayPause", IconCode::kKeyboardShortcutMediaPlayPause},
       {u"MediaTrackNext", IconCode::kKeyboardShortcutMediaTrackNext},
       {u"MediaTrackPrevious", IconCode::kKeyboardShortcutMediaTrackPrevious},
       {u"Menu", IconCode::kKeyboardShortcutContextMenu},
       {u"MicrophoneMuteToggle", IconCode::kKeyboardShortcutMicrophone},
       {u"ModeChange", IconCode::kKeyboardShortcutInputModeChange},
       {u"Power", IconCode::kKeyboardShortcutPower},
       {u"PrintScreen", IconCode::kKeyboardShortcutSnapshot},
       {u"PrivacyScreenToggle", IconCode::kKeyboardShortcutPrivacyScreenToggle},
       {u"RightAlt", IconCode::kKeyboardShortcutKeyboardRightAlt},
       {u"Settings", IconCode::kKeyboardShortcutSettings},
       {u"ViewAllApps", IconCode::kKeyboardShortcutAllApps},
       {u"ZoomToggle", IconCode::kKeyboardShortcutZoom}});

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr auto kRefreshIconCodes =
      base::MakeFixedFlatMap<std::u16string_view, IconCode>(
          {{u"LaunchApplication1",
            IconCode::kKeyboardShortcutMediaLaunchApp1Refresh},
           {u"BrightnessUp", IconCode::kKeyboardShortcutBrightnessUpRefresh}});

  // If there is a refreshed version of the given key, give priority to the new
  // icons.
  auto it_refresh = kRefreshIconCodes.find(key_string);
  if (ash::Shell::Get()->keyboard_capability()->UseRefreshedIcons() &&
      it_refresh != kRefreshIconCodes.end()) {
    return it_refresh->second;
  }
#endif

  auto it = kIconCodes.find(key_string);
  if (it == kIconCodes.end()) {
    return std::nullopt;
  }
  return it->second;
}

void KeyboardShortcutResult::PopulateTextVector(
    TextVector* text_vector,
    std::vector<std::u16string>& accessible_strings,
    const ui::Accelerator& accelerator) {
  CHECK(text_vector);

  std::vector<KeyboardCode> key_codes;
  // Insert keys by the order of SEARCH, CTRL, ALT, SHIFT, and then key, to be
  // consistent with the shortcuts app.
  if (accelerator.IsCmdDown()) {
    key_codes.push_back(KeyboardCode::VKEY_COMMAND);
  }
  if (accelerator.IsCtrlDown()) {
    key_codes.push_back(KeyboardCode::VKEY_CONTROL);
  }
  if (accelerator.IsAltDown()) {
    key_codes.push_back(KeyboardCode::VKEY_LMENU);
  }
  if (accelerator.IsShiftDown()) {
    key_codes.push_back(KeyboardCode::VKEY_SHIFT);
  }
  key_codes.push_back(accelerator.key_code());

  for (auto key_code : key_codes) {
    const std::optional<IconCode> icon_code =
        GetIconCodeFromKeyboardCode(key_code);
    bool use_alternative_styling = IsModifierKey(key_code);
    if (icon_code) {
      // The KeyboardCode has a corresponding IconCode, and therefore an
      // icon image is supported by the front-end.
      ash::SearchResultTextItem result_item =
          CreateIconCodeTextItem(icon_code.value());
      result_item.SetAlternateIconAndTextStyling(use_alternative_styling);
      text_vector->push_back(result_item);
      accessible_strings.push_back(
          GetAccessibleStringForIcon(icon_code.value()));
    } else {
      // KeyboardCode does not have a corresponding IconCode. The
      // key text will be styled to look like an icon ("iconified
      // text").
      //
      // All keys including modifiers should be displayed in lower case.
      const std::u16string key_string =
          base::ToLowerASCII(ash::GetStringForKeyboardCode(key_code));
      ash::SearchResultTextItem result_item =
          CreateIconifiedTextTextItem(key_string);

      result_item.SetAlternateIconAndTextStyling(use_alternative_styling);
      text_vector->push_back(result_item);
      accessible_strings.push_back(GetAccessibleStringForKey(key_string));
    }
  }
}

// Example shortcuts:
//   Title: Highlight next item on shelf
//   Parts:
//     type: kModifier,  text: alt
//     type: kModifier,  text: shift
//     type: kKey,       text: l
//     type: kPlainText, text:  then
//     type: kKey,       text: tab
//     type: kPlainText, text:  or
//     type: kKey,       text: ArrowRight
//
void KeyboardShortcutResult::PopulateTextVectorWithTextParts(
    TextVector* text_vector,
    std::vector<std::u16string>& accessible_strings,
    const std::vector<ash::mojom::TextAcceleratorPartPtr>& accelerator_parts) {
  for (auto& part : accelerator_parts) {
    switch (part->type) {
      case ash::mojom::TextAcceleratorPartType::kPlainText:
      case ash::mojom::TextAcceleratorPartType::kDelimiter:
        text_vector->push_back(CreateStringTextItem(part->text));
        accessible_strings.push_back(part->text);
        break;
      case ash::mojom::TextAcceleratorPartType::kKey:
      case ash::mojom::TextAcceleratorPartType::kModifier:
        const auto icon_code = GetIconCodeByKeyString(part->text);
        bool use_alternative_styling =
            part->type == ash::mojom::TextAcceleratorPartType::kModifier;
        if (icon_code) {
          ash::SearchResultTextItem result_item =
              CreateIconCodeTextItem(icon_code.value());
          result_item.SetAlternateIconAndTextStyling(use_alternative_styling);
          text_vector->push_back(result_item);
          accessible_strings.push_back(
              GetAccessibleStringForIcon(icon_code.value()));
        } else {
          // All keys including modifiers should be displayed in lower case.
          const std::u16string key_string = base::ToLowerASCII(part->text);
          ash::SearchResultTextItem result_item =
              CreateIconifiedTextTextItem(key_string);
          result_item.SetAlternateIconAndTextStyling(use_alternative_styling);
          text_vector->push_back(result_item);
          accessible_strings.push_back(GetAccessibleStringForKey(key_string));
        }
        break;
    }
  }
}

void KeyboardShortcutResult::PopulateTextVector(
    TextVector* text_vector,
    std::vector<std::u16string>& accessible_strings,
    const ash::mojom::AcceleratorInfoPtr& accelerator_info) {
  if (accelerator_info->layout_properties->is_standard_accelerator()) {
    const ash::mojom::StandardAcceleratorPropertiesPtr& standard_accelerator =
        accelerator_info->layout_properties->get_standard_accelerator();
    PopulateTextVector(text_vector, accessible_strings,
                       standard_accelerator->accelerator);
  } else {
    const ash::mojom::TextAcceleratorPropertiesPtr& text_accelerator =
        accelerator_info->layout_properties->get_text_accelerator();
    PopulateTextVectorWithTextParts(text_vector, accessible_strings,
                                    text_accelerator->parts);
  }
}

void KeyboardShortcutResult::PopulateTextVectorWithTwoShortcuts(
    TextVector* text_vector,
    std::vector<std::u16string>& accessible_strings,
    const ash::mojom::AcceleratorInfoPtr& accelerator_1,
    const ash::mojom::AcceleratorInfoPtr& accelerator_2) {
  CHECK(accelerator_1);
  CHECK(accelerator_2);

  // The default for IDS_SHORTCUT_CUSTOMIZATION_ONE_OF_TWO_CHOICES is "$1 or
  // $2". In other languages, the string may be different.
  const std::u16string template_string =
      l10n_util::GetStringUTF16(IDS_SHORTCUT_CUSTOMIZATION_ONE_OF_TWO_CHOICES);
  // Find placeholder' positions.
  const size_t first_index = template_string.find_first_of(u"$");
  CHECK(first_index != std::u16string::npos);
  const size_t second_index = template_string.find_last_of(u"$");
  CHECK(second_index != std::u16string::npos);
  CHECK(second_index > first_index);

  // Add text before the first placeholder if any.
  if (first_index > 0) {
    text_vector->push_back(
        CreateStringTextItem(template_string.substr(0, first_index)));
  }
  // Add first shortcut.
  PopulateTextVector(text_vector, accessible_strings, accelerator_1);
  // Add text between the two placeholders if any.
  // Since first_index points to the first char of "$1", the text we are
  // interested in starts at first_index + 2.
  const size_t between_len = second_index - first_index - 2;
  if (between_len > 0) {
    const auto between_text =
        template_string.substr(first_index + 2, between_len);
    text_vector->push_back(CreateStringTextItem(between_text));
    accessible_strings.push_back(between_text);
  }
  // Add second shortcut.
  PopulateTextVector(text_vector, accessible_strings, accelerator_2);
  // Add text after the second placeholder if any.
  if (second_index + 2 < template_string.size()) {
    text_vector->push_back(
        CreateStringTextItem(template_string.substr(second_index + 2)));
    accessible_strings.push_back(template_string.substr(second_index + 2));
  }
}

void KeyboardShortcutResult::PopulateTextVectorForNoShortcut(
    TextVector* text_vector,
    std::vector<std::u16string>& accessible_strings) {
  std::vector<ash::mojom::TextAcceleratorPartPtr> text_parts;
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      l10n_util::GetStringUTF16(
          IDS_SHORTCUT_CUSTOMIZATION_NO_SHORTCUT_ASSIGNED),
      ash::mojom::TextAcceleratorPartType::kPlainText));
  PopulateTextVectorWithTextParts(text_vector, accessible_strings, text_parts);
}

KeyboardShortcutResult::KeyboardShortcutResult(
    Profile* profile,
    const ash::shortcut_customization::mojom::SearchResultPtr& search_result)
    : profile_(profile) {
  accelerator_action_ =
      base::NumberToString(search_result->accelerator_layout_info->action);
  accelerator_category_ = base::NumberToString(
      static_cast<int>(search_result->accelerator_layout_info->category));
  // The ID needs to be unique among all results. The combination of action and
  // its category uniquely identifies a shortcut.
  set_id(base::StrCat({kKeyboardShortcutScheme, accelerator_action_, "/",
                       accelerator_category_}));
  set_relevance(search_result->relevance_score);
  SetTitle(search_result->accelerator_layout_info->description);
  SetResultType(ResultType::kKeyboardShortcut);
  SetMetricsType(ash::KEYBOARD_SHORTCUT);
  SetDisplayType(DisplayType::kList);
  SetCategory(Category::kHelp);
  UpdateIcon();

  // Set the details to the display name of the Keyboard Shortcuts app.
  std::u16string sanitized_name = base::CollapseWhitespace(
      l10n_util::GetStringUTF16(IDS_ASH_SHORTCUT_CUSTOMIZATION_APP_TITLE),
      true);
  base::i18n::SanitizeUserSuppliedString(&sanitized_name);
  SetDetails(sanitized_name);

  std::vector<std::u16string> accessible_strings;
  accessible_strings.push_back(
      base::StrCat({title(), u", ", details(), u", "}));

  TextVector text_vector;

  switch (search_result->accelerator_infos.size()) {
    case 0:
      // No shortcut assigned case:
      PopulateTextVectorForNoShortcut(&text_vector, accessible_strings);
      break;
    case 1:
      // Only one shortcut for the accelerator.
      PopulateTextVector(&text_vector, accessible_strings,
                         search_result->accelerator_infos[0]);
      break;
    default:
      // When there are more than one shortcuts, we only show the first two.
      PopulateTextVectorWithTwoShortcuts(&text_vector, accessible_strings,
                                         search_result->accelerator_infos[0],
                                         search_result->accelerator_infos[1]);
      break;
  }

  SetAccessibleName(base::JoinString(accessible_strings, u" "));
  SetKeyboardShortcutTextVector(text_vector);
}

KeyboardShortcutResult::~KeyboardShortcutResult() = default;

void KeyboardShortcutResult::Open(int event_flags) {
  // Pass the action and category of the selected shortcuts to the app so that
  // the same shortcuts will be displayed in the app.
  if (ash::features::IsSearchCustomizableShortcutsInLauncherEnabled()) {
    chrome::ShowShortcutCustomizationApp(profile_, accelerator_action_,
                                         accelerator_category_);
  } else {
    chrome::ShowShortcutCustomizationApp(profile_);
  }
}

void KeyboardShortcutResult::UpdateIcon() {
  ui::ImageModel icon = ui::ImageModel::FromVectorIcon(
      chromeos::kKeyboardShortcutsIcon, SK_ColorTRANSPARENT, kAppIconDimension);
  SetIcon(IconInfo(icon, kAppIconDimension));
}

}  // namespace app_list
