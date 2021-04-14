// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_terminal.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "net/base/escape.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

namespace crostini {

namespace {
constexpr char kSettingPrefix[] = "/hterm/profiles/default/";
const size_t kSettingPrefixSize = base::size(kSettingPrefix) - 1;

constexpr char kSettingBackgroundColor[] =
    "/hterm/profiles/default/background-color";
constexpr char kDefaultBackgroundColor[] = "#202124";

constexpr char kSettingPassCtrlW[] = "/hterm/profiles/default/pass-ctrl-w";
constexpr bool kDefaultPassCtrlW = false;

GURL GenerateVshInCroshUrl(Profile* profile,
                           const ContainerId& container_id,
                           const std::string& cwd,
                           const std::vector<std::string>& terminal_args) {
  std::string vsh_crosh = base::StrCat({chrome::kChromeUIUntrustedTerminalURL,
                                        "html/terminal.html?command=vmshell"});
  std::string vm_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--vm_name=%s", container_id.vm_name.c_str()),
      /*use_plus=*/true);
  std::string container_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--target_container=%s",
                         container_id.container_name.c_str()),
      /*use_plus=*/true);
  std::string owner_id_param = net::EscapeQueryParamValue(
      base::StringPrintf("--owner_id=%s",
                         CryptohomeIdForProfile(profile).c_str()),
      /*use_plus=*/true);

  std::vector<std::string> pieces = {vsh_crosh, vm_name_param,
                                     container_name_param, owner_id_param};
  if (!cwd.empty()) {
    pieces.push_back(net::EscapeQueryParamValue(
        base::StringPrintf("--cwd=%s", cwd.c_str()), /*use_plus=*/true));
  }
  if (!terminal_args.empty()) {
    // Separates the command args from the args we are passing into the
    // terminal to be executed.
    pieces.push_back("--");
    for (auto arg : terminal_args) {
      pieces.push_back(net::EscapeQueryParamValue(arg, /*use_plus=*/true));
    }
  }

  return GURL(base::JoinString(pieces, "&args[]="));
}

}  // namespace

void LaunchTerminal(Profile* profile,
                    int64_t display_id,
                    const ContainerId& container_id,
                    const std::string& cwd,
                    const std::vector<std::string>& terminal_args) {
  crostini::RecordAppLaunchHistogram(
      crostini::CrostiniAppLaunchAppType::kTerminal);
  GURL vsh_in_crosh_url =
      GenerateVshInCroshUrl(profile, container_id, cwd, terminal_args);
  auto params = web_app::CreateSystemWebAppLaunchParams(
      profile, web_app::SystemAppType::TERMINAL, display_id);
  if (!params.has_value()) {
    LOG(WARNING) << "Empty launch params for terminal";
    return;
  }

  // This LaunchSystemWebAppImpl call is necessary. Terminal App uses its own
  // CrostiniApps publisher for launching. Calling LaunchSystemWebAppAsync
  // would ask AppService to launch the App, which routes the launch request to
  // this function, resulting in a loop.
  //
  // System Web Apps managed by Web App publisher should call
  // LaunchSystemWebAppAsync.
  web_app::LaunchSystemWebAppImpl(profile, web_app::SystemAppType::TERMINAL,
                                  vsh_in_crosh_url, *params);
}

void LaunchTerminalSettings(Profile* profile, int64_t display_id) {
  auto params = web_app::CreateSystemWebAppLaunchParams(
      profile, web_app::SystemAppType::TERMINAL, display_id);
  if (!params.has_value()) {
    LOG(WARNING) << "Empty launch params for terminal";
    return;
  }
  std::string path = "html/terminal_settings.html";
  // Use an app pop window to host the settings page.
  params->disposition = WindowOpenDisposition::NEW_POPUP;

  // This LaunchSystemWebAppImpl call is necessary. Terminal App uses its own
  // CrostiniApps publisher for launching. Calling LaunchSystemWebAppAsync
  // would ask AppService to launch the App, which routes the launch request to
  // this function, resulting in a loop.
  //
  // System Web Apps managed by Web App publisher should call
  // LaunchSystemWebAppAsync.
  web_app::LaunchSystemWebAppImpl(
      profile, web_app::SystemAppType::TERMINAL,
      GURL(base::StrCat({chrome::kChromeUIUntrustedTerminalURL, path})),
      *params);
}

void RecordTerminalSettingsChangesUMAs(Profile* profile) {
  static constexpr auto kSettingsMap = base::MakeFixedFlatMap<base::StringPiece,
                                                              TerminalSetting>({
      {"alt-gr-mode", TerminalSetting::kAltGrMode},
      {"alt-backspace-is-meta-backspace",
       TerminalSetting::kAltBackspaceIsMetaBackspace},
      {"alt-is-meta", TerminalSetting::kAltIsMeta},
      {"alt-sends-what", TerminalSetting::kAltSendsWhat},
      {"audible-bell-sound", TerminalSetting::kAudibleBellSound},
      {"desktop-notification-bell", TerminalSetting::kDesktopNotificationBell},
      {"background-color", TerminalSetting::kBackgroundColor},
      {"background-image", TerminalSetting::kBackgroundImage},
      {"background-size", TerminalSetting::kBackgroundSize},
      {"background-position", TerminalSetting::kBackgroundPosition},
      {"backspace-sends-backspace", TerminalSetting::kBackspaceSendsBackspace},
      {"character-map-overrides", TerminalSetting::kCharacterMapOverrides},
      {"close-on-exit", TerminalSetting::kCloseOnExit},
      {"cursor-blink", TerminalSetting::kCursorBlink},
      {"cursor-blink-cycle", TerminalSetting::kCursorBlinkCycle},
      {"cursor-shape", TerminalSetting::kCursorShape},
      {"cursor-color", TerminalSetting::kCursorColor},
      {"color-palette-overrides", TerminalSetting::kColorPaletteOverrides},
      {"copy-on-select", TerminalSetting::kCopyOnSelect},
      {"use-default-window-copy", TerminalSetting::kUseDefaultWindowCopy},
      {"clear-selection-after-copy", TerminalSetting::kClearSelectionAfterCopy},
      {"ctrl-plus-minus-zero-zoom", TerminalSetting::kCtrlPlusMinusZeroZoom},
      {"ctrl-c-copy", TerminalSetting::kCtrlCCopy},
      {"ctrl-v-paste", TerminalSetting::kCtrlVPaste},
      {"east-asian-ambiguous-as-two-column",
       TerminalSetting::kEastAsianAmbiguousAsTwoColumn},
      {"enable-8-bit-control", TerminalSetting::kEnable8BitControl},
      {"enable-bold", TerminalSetting::kEnableBold},
      {"enable-bold-as-bright", TerminalSetting::kEnableBoldAsBright},
      {"enable-blink", TerminalSetting::kEnableBlink},
      {"enable-clipboard-notice", TerminalSetting::kEnableClipboardNotice},
      {"enable-clipboard-write", TerminalSetting::kEnableClipboardWrite},
      {"enable-dec12", TerminalSetting::kEnableDec12},
      {"enable-csi-j-3", TerminalSetting::kEnableCsiJ3},
      {"environment", TerminalSetting::kEnvironment},
      {"font-family", TerminalSetting::kFontFamily},
      {"font-size", TerminalSetting::kFontSize},
      {"font-smoothing", TerminalSetting::kFontSmoothing},
      {"foreground-color", TerminalSetting::kForegroundColor},
      {"enable-resize-status", TerminalSetting::kEnableResizeStatus},
      {"hide-mouse-while-typing", TerminalSetting::kHideMouseWhileTyping},
      {"home-keys-scroll", TerminalSetting::kHomeKeysScroll},
      {"keybindings", TerminalSetting::kKeybindings},
      {"media-keys-are-fkeys", TerminalSetting::kMediaKeysAreFkeys},
      {"meta-sends-escape", TerminalSetting::kMetaSendsEscape},
      {"mouse-right-click-paste", TerminalSetting::kMouseRightClickPaste},
      {"mouse-paste-button", TerminalSetting::kMousePasteButton},
      {"word-break-match-left", TerminalSetting::kWordBreakMatchLeft},
      {"word-break-match-right", TerminalSetting::kWordBreakMatchRight},
      {"word-break-match-middle", TerminalSetting::kWordBreakMatchMiddle},
      {"page-keys-scroll", TerminalSetting::kPageKeysScroll},
      {"pass-alt-number", TerminalSetting::kPassAltNumber},
      {"pass-ctrl-number", TerminalSetting::kPassCtrlNumber},
      {"pass-ctrl-n", TerminalSetting::kPassCtrlN},
      {"pass-ctrl-t", TerminalSetting::kPassCtrlT},
      {"pass-ctrl-tab", TerminalSetting::kPassCtrlTab},
      {"pass-ctrl-w", TerminalSetting::kPassCtrlW},
      {"pass-meta-number", TerminalSetting::kPassMetaNumber},
      {"pass-meta-v", TerminalSetting::kPassMetaV},
      {"paste-on-drop", TerminalSetting::kPasteOnDrop},
      {"receive-encoding", TerminalSetting::kReceiveEncoding},
      {"scroll-on-keystroke", TerminalSetting::kScrollOnKeystroke},
      {"scroll-on-output", TerminalSetting::kScrollOnOutput},
      {"scrollbar-visible", TerminalSetting::kScrollbarVisible},
      {"scroll-wheel-may-send-arrow-keys",
       TerminalSetting::kScrollWheelMaySendArrowKeys},
      {"scroll-wheel-move-multiplier",
       TerminalSetting::kScrollWheelMoveMultiplier},
      {"terminal-encoding", TerminalSetting::kTerminalEncoding},
      {"shift-insert-paste", TerminalSetting::kShiftInsertPaste},
      {"user-css", TerminalSetting::kUserCss},
      {"user-css-text", TerminalSetting::kUserCssText},
      {"allow-images-inline", TerminalSetting::kAllowImagesInline},
      {"theme", TerminalSetting::kTheme},
      {"theme-variations", TerminalSetting::kThemeVariations},
  });

  const base::DictionaryValue* settings = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);
  for (const auto& item : settings->DictItems()) {
    // Only record settings for /hterm/profiles/default/.
    if (!base::StartsWith(item.first, kSettingPrefix,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }
    const auto* it = kSettingsMap.find(
        base::StringPiece(item.first).substr(kSettingPrefixSize));
    base::UmaHistogramEnumeration(
        "Crostini.TerminalSettingsChanged",
        it != kSettingsMap.end() ? it->second : TerminalSetting::kUnknown);
  }
}

std::string GetTerminalSettingBackgroundColor(Profile* profile) {
  const base::DictionaryValue* value = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);
  const std::string* result = value->FindStringKey(kSettingBackgroundColor);
  return result ? *result : kDefaultBackgroundColor;
}

bool GetTerminalSettingPassCtrlW(Profile* profile) {
  const base::DictionaryValue* value = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);
  return value->FindBoolKey(kSettingPassCtrlW).value_or(kDefaultPassCtrlW);
}

}  // namespace crostini
