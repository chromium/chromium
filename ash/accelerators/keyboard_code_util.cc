// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/keyboard_code_util.h"

#include "ash/public/cpp/accelerators_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {

namespace {

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
      msg_id = Shell::Get()->keyboard_capability()->HasLauncherButton()
                   ? IDS_KSV_MODIFIER_LAUNCHER
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

}  // namespace

std::u16string GetStringForKeyboardCode(ui::KeyboardCode key_code,
                                        bool remap_positional_key) {
  const absl::optional<std::u16string> key_label =
      GetSpecialStringForKeyboardCode(key_code);
  if (key_label)
    return key_label.value();

  return ash::KeycodeToKeyString(key_code, remap_positional_key);
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

}  // namespace ash
