// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/shortcut_input_key_strings.h"

#include "ash/shell.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/events/ash/keyboard_capability.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace common {

void AddShortcutInputKeyStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"iconLabelAccessibility",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ACCESSIBILITY},
      {"iconLabelArrowDown", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_DOWN},
      {"iconLabelArrowLeft", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_LEFT},
      {"iconLabelArrowRight",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_RIGHT},
      {"iconLabelArrowUp", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_UP},
      {"iconLabelAudioVolumeDown",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_DOWN},
      {"iconLabelAudioVolumeMute",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_MUTE},
      {"iconLabelAudioVolumeUp",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_UP},
      {"iconLabelBrightnessDown",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BRIGHTNESS_DOWN},
      {"iconLabelBrightnessUp",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BRIGHTNESS_UP},
      {"iconLabelBrowserBack",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_BACK},
      {"iconLabelBrowserForward",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_FORWARD},
      {"iconLabelBrowserHome",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_HOME},
      {"iconLabelBrowserRefresh",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_REFRESH},
      {"iconLabelBrowserSearch",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_SEARCH},
      {"iconLabelContextMenu",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_CONTEXT_MENU},
      {"iconLabelEnableOrToggleDictation",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ENABLE_OR_TOGGLE_DICTATION},
      {"iconLabelEmojiPicker",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_EMOJI_PICKER},
      {"iconLabelKeyboardBacklightToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BACKLIGHT_TOGGLE},
      {"iconLabelKeyboardBrightnessUp",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_UP},
      {"iconLabelKeyboardBrightnessDown",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_DOWN},
      {"iconLabelLaunchApplication1",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_APPLICATION1},
      {"iconLabelLaunchApplication2",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_APPLICATION2},
      {"iconLabelLaunchAssistant",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_ASSISTANT},
      {"iconLabelLaunchMail",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_MAIL},
      {"iconLabelMediaFastForward",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_FAST_FORWARD},
      {"iconLabelMediaPause",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PAUSE},
      {"iconLabelMediaPlay", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PLAY},
      {"iconLabelMediaPlayPause",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PLAY_PAUSE},
      {"iconLabelMediaTrackNext",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_TRACK_NEXT},
      {"iconLabelMediaTrackPrevious",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_TRACK_PREVIOUS},
      {"iconLabelMicrophoneMuteToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MICROPHONE_MUTE_TOGGLE},
      {"iconLabelModeChange",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MODE_CHANGE},
      {"iconLabelOpenLauncher",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_OPEN_LAUNCHER},
      {"iconLabelOpenSearch",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_OPEN_SEARCH},
      {"iconLabelPower", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_POWER},
      {"iconLabelPrintScreen",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_PRINT_SCREEN},
      {"iconLabelPrivacyScreenToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_PRIVACY_SCREEN_TOGGLE},
      {"iconLabelSettings", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_SETTINGS},
      {"iconLabelStandby", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_STAND_BY},
      {"iconLabelViewAllApps",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_VIEW_ALL_APPS},
      {"iconLabelZoomToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ZOOM_TOGGLE},
      {"inputKeyPlaceholder", IDS_SHORTCUT_CUSTOMIZATION_INPUT_KEY_PLACEHOLDER},
  };

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (Shell::Get()->keyboard_capability()->IsModifierSplitEnabled()) {
    html_source->AddLocalizedString("iconLabelRightAlt",
                                    IDS_KEYBOARD_RIGHT_ALT_LABEL);
  } else {
    html_source->AddLocalizedString(
        "iconLabelRightAlt", IDS_SHORTCUT_CUSTOMIZATION_INPUT_KEY_PLACEHOLDER);
  }
#else
  html_source->AddLocalizedString(
      "iconLabelRightAlt", IDS_SHORTCUT_CUSTOMIZATION_INPUT_KEY_PLACEHOLDER);
#endif

  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->UseStringsJs();
}

}  // namespace common

}  // namespace ash
