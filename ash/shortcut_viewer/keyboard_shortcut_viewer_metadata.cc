// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/vector_icon_types.h"

namespace keyboard_shortcut_viewer {

using ash::ShortcutCategory;

namespace {

// Gets the keyboard codes for modifiers.
ui::KeyboardCode GetKeyCodeForModifier(ui::EventFlags modifier) {
  switch (modifier) {
    case ui::EF_CONTROL_DOWN:
      return ui::VKEY_CONTROL;
    case ui::EF_ALT_DOWN:
      return ui::VKEY_LMENU;
    case ui::EF_SHIFT_DOWN:
      return ui::VKEY_SHIFT;
    case ui::EF_COMMAND_DOWN:
      return ui::VKEY_COMMAND;
    default:
      NOTREACHED();
      return ui::VKEY_UNKNOWN;
  }
}

// Provides I18n string for key codes which have no mapping to a meaningful
// description or they require a special one we explicitly specify. For example,
// ui::VKEY_COMMAND could return a string "Meta", but we want to display it as
// "Search" or "Launcher".
absl::optional<std::u16string> GetSpecialStringForKeyboardCode(
    ui::KeyboardCode key_code) {
  int msg_id = 0;
  switch (key_code) {
    case ui::VKEY_CONTROL:
      msg_id = IDS_KSV_MODIFIER_CONTROL;
      break;
    case ui::VKEY_LMENU:
      msg_id = IDS_KSV_MODIFIER_ALT;
      break;
    case ui::VKEY_SHIFT:
      msg_id = IDS_KSV_MODIFIER_SHIFT;
      break;
    case ui::VKEY_COMMAND:
      // DeviceUsesKeyboardLayout2() relies on DeviceDataManager.
      DCHECK(ui::DeviceDataManager::HasInstance());
      DCHECK(ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete());
      msg_id = ui::DeviceUsesKeyboardLayout2() ? IDS_KSV_MODIFIER_LAUNCHER
                                               : IDS_KSV_MODIFIER_SEARCH;
      break;
    case ui::VKEY_ESCAPE:
      msg_id = IDS_KSV_KEY_ESCAPE;
      break;
    case ui::VKEY_SPACE:
      msg_id = IDS_KSV_KEY_SPACE;
      break;
    case ui::VKEY_MEDIA_LAUNCH_APP1:
      msg_id = IDS_KSV_KEY_OVERVIEW;
      break;
    case ui::VKEY_ZOOM:
      msg_id = IDS_KSV_KEY_FULLSCREEN;
      break;
    case ui::VKEY_SNAPSHOT:
      msg_id = IDS_KSV_KEY_SNAPSHOT;
      break;
    case ui::VKEY_UNKNOWN:
      // TODO(wutao): make this reliable.
      // If this is VKEY_UNKNOWN, it indicates to insert a "+" separator. Use
      // one plus and one space to replace the string resourece's placeholder so
      // that the separator will not conflict with the replacement string for
      // "VKEY_OEM_PLUS", which is "+" and "VKEY_SPACE", which is "Space".
      return u"+ ";
    default:
      return absl::nullopt;
  }
  return l10n_util::GetStringUTF16(msg_id);
}

// Dead keys work by combining two consecutive keystrokes together. The first
// keystroke does not produce an output character, it acts as a one-shot
// modifier for a subsequent keystroke. So for example on a German keyboard,
// pressing the acute ´ dead key, then pressing the letter e will produce é.
// The first character is called the combining character and does not produce
// an output glyph. This table maps the combining character to a string
// containing the non-combining equivalent that can be displayed.
std::u16string GetStringForDeadKey(ui::DomKey dom_key) {
  DCHECK(dom_key.IsDeadKey());
  int32_t ch = dom_key.ToDeadKeyCombiningCharacter();
  switch (ch) {
    // Combining grave.
    case 0x300:
      return u"`";
    // Combining acute.
    case 0x301:
      return u"´";
    // Combining circumflex.
    case 0x302:
      return u"^";
    // Combining tilde.
    case 0x303:
      return u"~";
    // Combining diaeresis.
    case 0x308:
      return u"¨";
    default:
      break;
  }

  LOG(WARNING) << "No mapping for dead key shortcut " << ch;
  return base::UTF8ToUTF16(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
}

}  // namespace

std::u16string GetStringForCategory(ShortcutCategory category) {
  int msg_id = 0;
  switch (category) {
    case ShortcutCategory::kPopular:
      msg_id = IDS_KSV_CATEGORY_POPULAR;
      break;
    case ShortcutCategory::kTabAndWindow:
      msg_id = IDS_KSV_CATEGORY_TAB_WINDOW;
      break;
    case ShortcutCategory::kPageAndBrowser:
      msg_id = IDS_KSV_CATEGORY_PAGE_BROWSER;
      break;
    case ShortcutCategory::kSystemAndDisplay:
      msg_id = IDS_KSV_CATEGORY_SYSTEM_DISPLAY;
      break;
    case ShortcutCategory::kTextEditing:
      msg_id = IDS_KSV_CATEGORY_TEXT_EDITING;
      break;
    case ShortcutCategory::kAccessibility:
      msg_id = IDS_KSV_CATEGORY_ACCESSIBILITY;
      break;
    default:
      NOTREACHED();
      return std::u16string();
  }
  return l10n_util::GetStringUTF16(msg_id);
}

std::u16string GetStringForKeyboardCode(ui::KeyboardCode key_code,
                                        bool remap_positional_key) {
  const absl::optional<std::u16string> key_label =
      GetSpecialStringForKeyboardCode(key_code);
  if (key_label)
    return key_label.value();

  ui::DomKey dom_key;
  ui::KeyboardCode key_code_to_compare = ui::VKEY_UNKNOWN;
  const ui::KeyboardLayoutEngine* layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();

  // The input |key_code| is the |KeyboardCode| aka VKEY of the shortcut in
  // the US layout which is registered from the shortcut table. |key_code|
  // is first mapped to the |DomCode| this key is on in the US layout. If
  // the key is not positional, this processing is skipped and it is handled
  // normally in the loop below. For the positional keys, the |DomCode| is
  // then mapped to the |DomKey| in the current layout which represents the
  // glyph/character that appears on the key (and usually when typed).
  if (remap_positional_key &&
      ::features::IsImprovedKeyboardShortcutsEnabled()) {
    ui::DomCode dom_code =
        ui::KeycodeConverter::MapUSPositionalShortcutKeyToDomCode(key_code);
    if (dom_code != ui::DomCode::NONE) {
      if (layout_engine->Lookup(dom_code, /*flags=*/ui::EF_NONE, &dom_key,
                                &key_code_to_compare)) {
        if (dom_key.IsDeadKey()) {
          return GetStringForDeadKey(dom_key);
        }
        if (!dom_key.IsValid()) {
          return std::u16string();
        }
        return base::UTF8ToUTF16(
            ui::KeycodeConverter::DomKeyToKeyString(dom_key));
      }
      return std::u16string();
    }
  }

  for (const auto& dom_code : ui::kDomCodesArray) {
    if (!layout_engine->Lookup(dom_code, /*flags=*/ui::EF_NONE, &dom_key,
                               &key_code_to_compare)) {
      continue;
    }
    if (key_code_to_compare != key_code || !dom_key.IsValid() ||
        dom_key.IsDeadKey()) {
      continue;
    }
    return base::UTF8ToUTF16(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
  }
  return std::u16string();
}

std::u16string GetAccessibleNameForKeyboardCode(ui::KeyboardCode key_code) {
  int msg_id = 0;
  switch (key_code) {
    case ui::VKEY_OEM_PERIOD:
      msg_id = IDS_KSV_KEY_PERIOD_ACCESSIBILITY_NAME;
      break;
    case ui::VKEY_OEM_COMMA:
      msg_id = IDS_KSV_KEY_COMMA_ACCESSIBILITY_NAME;
      break;
    case ui::VKEY_OEM_MINUS:
      msg_id = IDS_KSV_KEY_HYPHEN_ACCESSIBILITY_NAME;
      break;
    case ui::VKEY_OEM_4:
      msg_id = IDS_KSV_KEY_BRACKET_LEFT_ACCESSIBILITY_NAME;
      break;
    case ui::VKEY_OEM_6:
      msg_id = IDS_KSV_KEY_BRACKET_RIGHT_ACCESSIBILITY_NAME;
      break;
    default:
      break;
  }
  return msg_id ? l10n_util::GetStringUTF16(msg_id) : std::u16string();
}

const gfx::VectorIcon* GetVectorIconForKeyboardCode(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_BROWSER_BACK:
      return &ash::kKsvBrowserBackIcon;
    case ui::VKEY_BROWSER_FORWARD:
      return &ash::kKsvBrowserForwardIcon;
    case ui::VKEY_BROWSER_REFRESH:
      return &ash::kKsvReloadIcon;
    case ui::VKEY_ZOOM:
      return &ash::kKsvFullscreenIcon;
    case ui::VKEY_MEDIA_LAUNCH_APP1:
      return &ash::kKsvOverviewIcon;
    case ui::VKEY_BRIGHTNESS_DOWN:
      return &ash::kKsvBrightnessDownIcon;
    case ui::VKEY_BRIGHTNESS_UP:
      return &ash::kKsvBrightnessUpIcon;
    case ui::VKEY_VOLUME_MUTE:
      return &ash::kKsvMuteIcon;
    case ui::VKEY_VOLUME_DOWN:
      return &ash::kKsvVolumeDownIcon;
    case ui::VKEY_VOLUME_UP:
      return &ash::kKsvVolumeUpIcon;
    case ui::VKEY_UP:
      return &ash::kKsvArrowUpIcon;
    case ui::VKEY_DOWN:
      return &ash::kKsvArrowDownIcon;
    case ui::VKEY_LEFT:
      return &ash::kKsvArrowLeftIcon;
    case ui::VKEY_RIGHT:
      return &ash::kKsvArrowRightIcon;
    case ui::VKEY_PRIVACY_SCREEN_TOGGLE:
      return &ash::kKsvPrivacyScreenToggleIcon;
    case ui::VKEY_SNAPSHOT:
      return &ash::kKsvSnapshotIcon;
    default:
      return nullptr;
  }
}

const std::vector<ash::KeyboardShortcutItem>& GetKeyboardShortcutItemList() {
  static base::NoDestructor<std::vector<ash::KeyboardShortcutItem>> item_list({
      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_TOGGLE_DOCKED_MAGNIFIER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_D, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_TOGGLE_FULLSCREEN_MAGNIFIER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_LOCK_SCREEN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_L, ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_DISPLAY_ZOOM_OUT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_DISPLAY_ZOOM_IN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DRAG_LINK_IN_SAME_TAB,
       IDS_KSV_SHORTCUT_DRAG_LINK_IN_SAME_TAB},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DRAG_LINK_IN_NEW_TAB,
       IDS_KSV_SHORTCUT_DRAG_LINK_IN_NEW_TAB},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_HIGHLIGHT_NEXT_ITEM_ON_SHELF,
       IDS_KSV_SHORTCUT_HIGHLIGHT_NEXT_ITEM_ON_SHELF,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_LMENU, ui::VKEY_UNKNOWN,
        ui::VKEY_L, ui::VKEY_TAB, ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_CYCLE_BACKWARD_MRU,
       IDS_KSV_SHORTCUT_CYCLE_BACKWARD_MRU,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_LMENU, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_TAB}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_CYCLE_FORWARD_MRU,
       IDS_KSV_SHORTCUT_CYCLE_FORWARD_MRU,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_LMENU, ui::VKEY_TAB}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_FOCUS_SHELF,
       {},
       // |accelerator_ids|
       {{ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_FOCUS_PIP,
       {},
       // |accelerator_ids|
       {{ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_BOOKMARK_ALL_TABS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_BOOKMARK_THIS_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_D, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_CLOSE_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_W, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_CLOSE_WINDOW,
       {},
       // |accelerator_ids|
       {{ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_IDC_CONTENT_CONTEXT_SELECTALL,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_A}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_DEV_TOOLS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_DEV_TOOLS_CONSOLE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_FIND,
       {},
       // |accelerator_ids|
       {{ui::VKEY_F, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_FIND_NEXT,
       IDS_KSV_SHORTCUT_IDC_FIND_NEXT,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_G, ui::VKEY_RETURN}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_FIND_PREVIOUS,
       IDS_KSV_SHORTCUT_IDC_FIND_PREVIOUS,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN,
        ui::VKEY_G, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_RETURN}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser, ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_IDC_FOCUS_BOOKMARKS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_IDC_FOCUS_LOCATION,
       IDS_KSV_SHORTCUT_IDC_FOCUS_LOCATION,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_L, ui::VKEY_LMENU,
        ui::VKEY_UNKNOWN, ui::VKEY_D}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_FOCUS_SEARCH,
       IDS_KSV_SHORTCUT_IDC_FOCUS_SEARCH,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_K, ui::VKEY_E}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_IDC_FOCUS_TOOLBAR,
       {},
       // |accelerator_ids|
       {{ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_KEYBOARD_BRIGHTNESS_DOWN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_KEYBOARD_BRIGHTNESS_UP,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_LAUNCH_LAST_APP,
       {},
       // |accelerator_ids|
       {{ui::VKEY_9, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_NEW_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_T, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_NEW_WINDOW,
       {},
       // |accelerator_ids|
       {{ui::VKEY_N, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_NEW_INCOGNITO_WINDOW,
       {},
       // |accelerator_ids|
       {{ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_OPEN_FILE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_O, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_PRINT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_P, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_RELOAD,
       IDS_KSV_SHORTCUT_IDC_RELOAD,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_BROWSER_REFRESH, ui::VKEY_CONTROL, ui::VKEY_UNKNOWN,
        ui::VKEY_R}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_RELOAD_BYPASSING_CACHE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_R, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_RESTORE_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_BACK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_LEFT, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_FORWARD,
       {},
       // |accelerator_ids|
       {{ui::VKEY_RIGHT, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_SAVE_PAGE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_S, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_SEARCH_TABS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_IDC_SELECT_LAST_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_9, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_IDC_SELECT_NEXT_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_TAB, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_IDC_SELECT_PREVIOUS_TAB,
       {},
       // |accelerator_ids|
       {{ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_SHOW_BOOKMARK_BAR,
       {},
       // |accelerator_ids|
       {{ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_SHOW_HISTORY,
       {},
       // |accelerator_ids|
       {{ui::VKEY_H, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_CLOSE_FIND_OR_STOP,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_ESCAPE}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_VIEW_SOURCE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_U, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_ZOOM_MINUS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_ZOOM_NORMAL,
       {},
       // |accelerator_ids|
       {{ui::VKEY_0, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_ZOOM_PLUS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SWITCH_TO_NEXT_IME,
       {},
       // |accelerator_ids|
       {{ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_OPEN_FILE_MANAGER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SWITCH_TO_LAST_USED_IME,
       {},
       // |accelerator_ids|
       {{ui::VKEY_SPACE, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_ROTATE_SCREEN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SCALE_UI_RESET,
       {},
       // |accelerator_ids|
       {{ui::VKEY_0, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SWITCH_TO_NEXT_USER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_PERIOD, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SWITCH_TO_PREVIOUS_USER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_COMMA, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_TAKE_PARTIAL_SCREENSHOT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_TAKE_SCREENSHOT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_SNAPSHOT, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_TAKE_FULLSCREEN_SCREENSHOT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_TOGGLE_CAPS_LOCK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_LWIN, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_TOGGLE_HIGH_CONTRAST,
       {},
       // |accelerator_ids|
       {{ui::VKEY_H, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_CARET_BROWSING_TOGGLE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_7, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_TOGGLE_MESSAGE_CENTER_BUBBLE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_TOGGLE_OVERVIEW,
       {},
       // |accelerator_ids|
       {{ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_TOGGLE_RESIZE_LOCK_MENU,
       {},
       // |accelerator_ids|
       {{ui::VKEY_C, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_TOGGLE_SPOKEN_FEEDBACK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_TOGGLE_SYSTEM_TRAY_BUBBLE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_WINDOW_CYCLE_SNAP_LEFT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_4, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_WINDOW_CYCLE_SNAP_RIGHT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_6, ui::EF_ALT_DOWN}}},
      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_CLIPBOARD_HISTORY,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_V}},
      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_COPY,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_C}},

      {// |categories|
       {ShortcutCategory::kPopular, ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_PASTE,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_V}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_SELECT_NUMBERED_TAB,
       IDS_KSV_SHORTCUT_SELECT_NUMBERED_TAB,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_KEYBOARD_SHORTCUT_HELPER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_OPEN_LINK_IN_TAB_BACKGROUND,
       IDS_KSV_SHORTCUT_OPEN_LINK_IN_TAB_BACKGROUND,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_OPEN_LINK_IN_TAB,
       IDS_KSV_SHORTCUT_OPEN_LINK_IN_TAB,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_OPEN_LINK_IN_WINDOW,
       IDS_KSV_SHORTCUT_OPEN_LINK_IN_WINDOW,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_OPEN_PAGE_IN_NEW_TAB,
       IDS_KSV_SHORTCUT_OPEN_PAGE_IN_NEW_TAB,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_LMENU, ui::VKEY_UNKNOWN, ui::VKEY_RETURN}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_STOP_DRAG_TAB,
       IDS_KSV_SHORTCUT_STOP_DRAG_TAB,
       // |accelerator_ids|
       {{ui::VKEY_ESCAPE, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_PAGE_UP,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_UP}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_PAGE_DOWN,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_DOWN}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_SCROLL_DOWN_PAGE,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SPACE}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_GO_TO_TOP,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_GO_TO_BOTTOM,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_RIGHT_CLICK,
       IDS_KSV_SHORTCUT_RIGHT_CLICK,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_LMENU}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_SAVE_LINK_AS_BOOKMARK,
       IDS_KSV_SHORTCUT_SAVE_LINK_AS_BOOKMARK,
       // |accelerator_ids|
       {}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_AUTO_COMPLETE,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_RETURN}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_OPEN_DOWNLOADS_PAGE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_J, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_OPEN_FILE,
       IDS_KSV_SHORTCUT_OPEN_FILE,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SPACE}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_DISPLAY_HIDDEN_FILES,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_OEM_PERIOD}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_LAUNCH_NUMBERED_APP,
       IDS_KSV_SHORTCUT_LAUNCH_NUMBERED_APP,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_LMENU, ui::VKEY_UNKNOWN}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SELECT_ADDRESS_BAR,
       IDS_KSV_SHORTCUT_SELECT_ADDRESS_BAR,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_L, ui::VKEY_LMENU,
        ui::VKEY_UNKNOWN, ui::VKEY_D}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SELECT_NEXT_WORD,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN,
        ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SELECT_TEXT_TO_END_OF_LINE,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_COMMAND, ui::VKEY_UNKNOWN,
        ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SELECT_TEXT_TO_BEGINNING,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_COMMAND, ui::VKEY_UNKNOWN,
        ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SELECT_PREVIOUS_WORD,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN,
        ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_MOVE_TO_END_OF_WORD,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_MOVE_TO_START_OF_PREVIOUS_WORD,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_GO_TO_END_OF_DOCUMENT,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_COMMAND, ui::VKEY_UNKNOWN,
        ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_GO_TO_END_OF_LINE,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_RIGHT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_GO_TO_BEGINNING_OF_DOCUMENT,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_COMMAND, ui::VKEY_UNKNOWN,
        ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_GO_TO_BEGINNING_OF_LINE,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_PASTE_CONTENT_AS_TEXT,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN,
        ui::VKEY_V}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_CUT,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_X}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_DELETE_PREVIOUS_WORD,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_BACK}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_DELETE_NEXT_WORD,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_BACK}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_UNDO,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_Z}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_REDO,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN,
        ui::VKEY_Z}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_HIGHLIGHT_PREVIOUS_ITEM_ON_SHELF,
       IDS_KSV_SHORTCUT_HIGHLIGHT_PREVIOUS_ITEM_ON_SHELF,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_LMENU, ui::VKEY_UNKNOWN,
        ui::VKEY_L, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_TAB,
        ui::VKEY_LEFT}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_OPEN_HIGHLIGHTED_ITEM_ON_SHELF,
       IDS_KSV_SHORTCUT_OPEN_HIGHLIGHTED_ITEM_ON_SHELF,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_LMENU, ui::VKEY_UNKNOWN,
        ui::VKEY_L, ui::VKEY_SPACE, ui::VKEY_RETURN}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_REMOVE_HIGHLIGHT_ON_SHELF,
       IDS_KSV_SHORTCUT_REMOVE_HIGHLIGHT_ON_SHELF,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_LMENU, ui::VKEY_UNKNOWN,
        ui::VKEY_L, ui::VKEY_ESCAPE}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_SWITCH_FOCUS,
       IDS_KSV_SHORTCUT_SWITCH_FOCUS,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_BROWSER_BACK,
        ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_UNKNOWN,
        ui::VKEY_BROWSER_BACK}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_OPEN_RIGHT_CLICK_MENU_FOR_HIGHLIGHTED_ITEM,
       {},
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_COMMAND, ui::VKEY_UNKNOWN,
        ui::VKEY_VOLUME_UP}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_TOGGLE_MIRROR_MODE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SWAP_PRIMARY_DISPLAY,
       {},
       // |accelerator_ids|
       {{ui::VKEY_ZOOM, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_BRIGHTNESS_DOWN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_BRIGHTNESS_UP,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_MAGNIFY_SCREEN_ZOOM_OUT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_MAGNIFY_SCREEN_ZOOM_IN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_VOLUME_MUTE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_VOLUME_MUTE, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_VOLUME_DOWN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_VOLUME_DOWN, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_VOLUME_UP,
       {},
       // |accelerator_ids|
       {{ui::VKEY_VOLUME_UP, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SHOW_TASK_MANAGER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_TOGGLE_FULLSCREEN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_ZOOM, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_WINDOW_MINIMIZE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_FOCUS_NEXT_PANE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_FOCUS_PREVIOUS_PANE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_FOCUS_WEB_CONTENTS_PANE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_F6, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_TOGGLE_APP_LIST,
       {},
       // |accelerator_ids|
       {{ui::VKEY_LWIN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_TOGGLE_APP_LIST_FULLSCREEN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_LWIN, ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_TAKE_WINDOW_SCREENSHOT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SUSPEND,
       {},
       // |accelerator_ids|
       {{ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_OPEN_GET_HELP,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_OPEN_FEEDBACK_PAGE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_I, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_ROTATE_WINDOW,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BROWSER_REFRESH,
         ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SHOW_STYLUS_TOOLS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_P, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_TOGGLE_MAXIMIZED,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_OPEN_CROSH,
       {},
       // |accelerator_ids|
       {{ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_EXIT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_Q, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_UNPIN,
       {},
       // |accelerator_ids|
       {{ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_SHOW_IME_MENU_BUBBLE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DESKS_NEW_DESK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},
      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DESKS_REMOVE_CURRENT_DESK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DESKS_ACTIVATE_LEFT_DESK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN}}},
      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DESKS_ACTIVATE_RIGHT_DESK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DESKS_MOVE_ACTIVE_ITEM_LEFT_DESK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_4, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},
      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_DESKS_MOVE_ACTIVE_ITEM_RIGHT_DESK,
       {},
       // |accelerator_ids|
       {{ui::VKEY_OEM_6, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kTabAndWindow},
       IDS_KSV_DESCRIPTION_FLOAT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_F, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_SHOW_IDC_FOCUS_MENU_BAR,
       {},
       // |accelerator_ids|
       {{ui::VKEY_F10}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_SHOW_IDC_HOME,
       {},
       // |accelerator_ids|
       {{ui::VKEY_HOME, ui::EF_ALT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_SHOW_IDC_CLEAR_BROWSING_DATA,
       {},
       // |accelerator_ids|
       {{ui::VKEY_BACK, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_SHOW_IDC_SHOW_BOOKMARK_MANAGER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_O, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_DEV_TOOLS_INSPECT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_C, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY,
       {},
       // |accelerator_ids|
       {{ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN}}},

      {// |categories|
       {ShortcutCategory::kPageAndBrowser},
       IDS_KSV_DESCRIPTION_IDC_SHOW_APP_MENU,
       IDS_KSV_SHORTCUT_IDC_SHOW_APP_MENU,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_LMENU, ui::VKEY_UNKNOWN, ui::VKEY_E, ui::VKEY_F}},

      {// |categories|
       {ShortcutCategory::kPopular},
       IDS_KSV_DESCRIPTION_OPEN_GOOGLE_ASSISTANT,
       {},
       // |accelerator_ids|
       {{ui::VKEY_A, ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_MOVE_APPS_IN_GRID,
       IDS_KSV_SHORTCUT_MOVE_APPS_IN_GRID,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_LEFT, ui::VKEY_RIGHT, ui::VKEY_UP,
        ui::VKEY_DOWN}},

      {// |categories|
       {ShortcutCategory::kAccessibility},
       IDS_KSV_DESCRIPTION_MOVE_APPS_IN_OUT_FOLDER,
       IDS_KSV_SHORTCUT_MOVE_APPS_IN_OUT_FOLDER,
       // |accelerator_ids|
       {},
       // |shortcut_key_codes|
       {ui::VKEY_CONTROL, ui::VKEY_UNKNOWN, ui::VKEY_SHIFT, ui::VKEY_LEFT,
        ui::VKEY_RIGHT, ui::VKEY_UP, ui::VKEY_DOWN}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_PRIVACY_SCREEN_TOGGLE,
       {},
       // |accelerator_ids|
       {{ui::VKEY_PRIVACY_SCREEN_TOGGLE, ui::EF_NONE}}},

      {// |categories|
       {ShortcutCategory::kTextEditing},
       IDS_KSV_DESCRIPTION_SHOW_EMOJI_PICKER,
       {},
       // |accelerator_ids|
       {{ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN}}},

      {// |categories|
       {ShortcutCategory::kSystemAndDisplay},
       IDS_KSV_DESCRIPTION_OPEN_DIAGNOSTICS,
       {},
       // |accelerator_ids|
       {{ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN}}},
  });

  static bool is_initialized = false;
  // If the item's |shortcut_key_codes| is empty, we need to dynamically
  // populate the keycodes with |accelerator_ids| to construct the shortcut
  // string.
  if (!is_initialized) {
    is_initialized = true;

    // The improved desks keyboard shortcuts should only be enabled if the
    // improved keyboard shortcuts flag is also enabled.
    if (::features::IsImprovedKeyboardShortcutsEnabled() &&
        ash::features::IsImprovedDesksKeyboardShortcutsEnabled()) {
      const ash::KeyboardShortcutItem indexed_activation_shortcut = {
          // |categories|
          {ShortcutCategory::kTabAndWindow},
          IDS_KSV_DESCRIPTION_DESKS_ACTIVATE_INDEXED_DESK,
          IDS_KSV_SHORTCUT_DESKS_ACTIVATE_INDEXED_DESK,
          // |accelerator_ids|
          {},
          // |shortcut_key_codes|
          {{ui::VKEY_SHIFT, ui::VKEY_UNKNOWN, ui::VKEY_COMMAND,
            ui::VKEY_UNKNOWN}}};

      const ash::KeyboardShortcutItem toggle_all_desks_shortcut = {
          // |categories|
          {ShortcutCategory::kTabAndWindow},
          IDS_KSV_DESCRIPTION_DESKS_TOGGLE_WINDOW_ASSIGNED_TO_ALL_DESKS,
          {},
          // |accelerator_ids|
          {{ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN}}};

      item_list->emplace_back(indexed_activation_shortcut);
      item_list->emplace_back(toggle_all_desks_shortcut);
    }

    if (ash::features::IsCalendarViewEnabled()) {
      const ash::KeyboardShortcutItem toggle_calendar = {
          // |categories|
          {ShortcutCategory::kSystemAndDisplay},
          IDS_KSV_DESCRIPTION_TOGGLE_CALENDAR,
          {},
          // |accelerator_ids|
          {},
          // |shortcut_key_codes|
          {{ui::VKEY_COMMAND, ui::VKEY_UNKNOWN, ui::VKEY_C}}};

      item_list->emplace_back(toggle_calendar);
    }

    for (auto& item : *item_list) {
      if (item.shortcut_key_codes.empty() && !item.accelerator_ids.empty()) {
        // Only use the first |accelerator_id| because the modifiers are the
        // same even if it is a grouped accelerators.
        const ash::AcceleratorId& accelerator_id = item.accelerator_ids[0];
        // Insert |shortcut_key_codes| by the order of CTRL, ALT, SHIFT, SEARCH,
        // and then key, to be consistent with how we describe it in the
        // |shortcut_message_id| associated string template.
        for (auto modifier : {ui::EF_CONTROL_DOWN, ui::EF_ALT_DOWN,
                              ui::EF_SHIFT_DOWN, ui::EF_COMMAND_DOWN}) {
          if (accelerator_id.modifiers & modifier) {
            // ui::VKEY_UNKNOWN is used as a separator and will be shown as a
            // highlighted "+" sign between the bubble views and the rest of the
            // text.
            if (!item.shortcut_key_codes.empty())
              item.shortcut_key_codes.push_back(ui::VKEY_UNKNOWN);
            item.shortcut_key_codes.push_back(GetKeyCodeForModifier(modifier));
          }
        }
        // For non grouped accelerators, we need to populate the key as well.
        if (item.accelerator_ids.size() == 1) {
          if (!item.shortcut_key_codes.empty())
            item.shortcut_key_codes.push_back(ui::VKEY_UNKNOWN);
          item.shortcut_key_codes.push_back(accelerator_id.keycode);
        }
      }
    }
  }

  return *item_list;
}

}  // namespace keyboard_shortcut_viewer
