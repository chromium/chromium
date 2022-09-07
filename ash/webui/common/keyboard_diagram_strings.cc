// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/keyboard_diagram_strings.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

namespace common {

void AddKeyboardDiagramStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"keyboardDiagramAriaLabelNotPressed",
       IDS_KEYBOARD_DIAGRAM_ARIA_LABEL_NOT_PRESSED},
      {"keyboardDiagramAriaLabelPressed",
       IDS_KEYBOARD_DIAGRAM_ARIA_LABEL_PRESSED},
      {"keyboardDiagramAriaLabelTested",
       IDS_KEYBOARD_DIAGRAM_ARIA_LABEL_TESTED},
      {"keyboardDiagramAriaNameArrowDown",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_ARROW_DOWN},
      {"keyboardDiagramAriaNameArrowLeft",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_ARROW_LEFT},
      {"keyboardDiagramAriaNameArrowRight",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_ARROW_RIGHT},
      {"keyboardDiagramAriaNameArrowUp",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_ARROW_UP},
      {"keyboardDiagramAriaNameAssistant",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_ASSISTANT},
      {"keyboardDiagramAriaNameBack", IDS_KEYBOARD_DIAGRAM_ARIA_NAME_BACK},
      {"keyboardDiagramAriaNameBackspace",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_BACKSPACE},
      {"keyboardDiagramAriaNameControlPanel",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_CONTROL_PANEL},
      {"keyboardDiagramAriaNameEnter", IDS_KEYBOARD_DIAGRAM_ARIA_NAME_ENTER},
      {"keyboardDiagramAriaNameForward",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_FORWARD},
      {"keyboardDiagramAriaNameFullscreen",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_FULLSCREEN},
      {"keyboardDiagramAriaNameJisLetterSwitch",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_JIS_LETTER_SWITCH},
      {"keyboardDiagramAriaNameKeyboardBacklightDown",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_KEYBOARD_BACKLIGHT_DOWN},
      {"keyboardDiagramAriaNameKeyboardBacklightToggle",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_KEYBOARD_BACKLIGHT_TOGGLE},
      {"keyboardDiagramAriaNameKeyboardBacklightUp",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_KEYBOARD_BACKLIGHT_UP},
      {"keyboardDiagramAriaNameLauncher",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_LAUNCHER},
      {"keyboardDiagramAriaNameLayoutSwitch",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_LAYOUT_SWITCH},
      {"keyboardDiagramAriaNameLock", IDS_KEYBOARD_DIAGRAM_ARIA_NAME_LOCK},
      {"keyboardDiagramAriaNameMicrophoneMute",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_MICROPHONE_MUTE},
      {"keyboardDiagramAriaNameMute", IDS_KEYBOARD_DIAGRAM_ARIA_NAME_MUTE},
      {"keyboardDiagramAriaNameOverview",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_OVERVIEW},
      {"keyboardDiagramAriaNamePlayPause",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_PLAY_PAUSE},
      {"keyboardDiagramAriaNamePower", IDS_KEYBOARD_DIAGRAM_ARIA_NAME_POWER},
      {"keyboardDiagramAriaNamePrivacyScreenToggle",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_PRIVACY_SCREEN_TOGGLE},
      {"keyboardDiagramAriaNameRefresh",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_REFRESH},
      {"keyboardDiagramAriaNameScreenBrightnessDown",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_SCREEN_BRIGHTNESS_DOWN},
      {"keyboardDiagramAriaNameScreenBrightnessUp",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_SCREEN_BRIGHTNESS_UP},
      {"keyboardDiagramAriaNameScreenMirror",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_SCREEN_MIRROR},
      {"keyboardDiagramAriaNameScreenshot",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_SCREENSHOT},
      {"keyboardDiagramAriaNameShiftLeft",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_SHIFT_LEFT},
      {"keyboardDiagramAriaNameShiftRight",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_SHIFT_RIGHT},
      {"keyboardDiagramAriaNameTab", IDS_KEYBOARD_DIAGRAM_ARIA_NAME_TAB},
      {"keyboardDiagramAriaNameTrackNext",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_TRACK_NEXT},
      {"keyboardDiagramAriaNameTrackPrevious",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_TRACK_PREVIOUS},
      {"keyboardDiagramAriaNameVolumeDown",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_VOLUME_DOWN},
      {"keyboardDiagramAriaNameVolumeUp",
       IDS_KEYBOARD_DIAGRAM_ARIA_NAME_VOLUME_UP},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->UseStringsJs();
}

}  // namespace common

}  // namespace ash
