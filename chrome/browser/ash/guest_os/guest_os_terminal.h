// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TERMINAL_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TERMINAL_H_

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {
struct MenuItems;
}

class Profile;

namespace guest_os {

struct GuestId;

// web_app::GenerateAppId(/*manifest_id=*/std::nullopt,
//     GURL("chrome-untrusted://terminal/html/terminal.html"))
extern const char kTerminalSystemAppId[];

extern const char kTerminalHomePath[];

extern const char kShortcutKey[];
extern const char kShortcutValueSSH[];
extern const char kShortcutValueTerminal[];
extern const char kProfileIdKey[];

// Settings items that can be user-modified for terminal.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TerminalSetting {
  kUnknown = 0,
  kAltGrMode = 1,
  kAltBackspaceIsMetaBackspace = 2,
  kAltIsMeta = 3,
  kAltSendsWhat = 4,
  kAudibleBellSound = 5,
  kDesktopNotificationBell = 6,
  kBackgroundColor = 7,
  kBackgroundImage = 8,
  kBackgroundSize = 9,
  kBackgroundPosition = 10,
  kBackspaceSendsBackspace = 11,
  kCharacterMapOverrides = 12,
  kCloseOnExit = 13,
  kCursorBlink = 14,
  kCursorBlinkCycle = 15,
  kCursorShape = 16,
  kCursorColor = 17,
  kColorPaletteOverrides = 18,
  kCopyOnSelect = 19,
  kUseDefaultWindowCopy = 20,
  kClearSelectionAfterCopy = 21,
  kCtrlPlusMinusZeroZoom = 22,
  kCtrlCCopy = 23,
  kCtrlVPaste = 24,
  kEastAsianAmbiguousAsTwoColumn = 25,
  kEnable8BitControl = 26,
  kEnableBold = 27,
  kEnableBoldAsBright = 28,
  kEnableBlink = 29,
  kEnableClipboardNotice = 30,
  kEnableClipboardWrite = 31,
  kEnableDec12 = 32,
  kEnableCsiJ3 = 33,
  kEnvironment = 34,
  kFontFamily = 35,
  kFontSize = 36,
  kFontSmoothing = 37,
  kForegroundColor = 38,
  kEnableResizeStatus = 39,
  kHideMouseWhileTyping = 40,
  kHomeKeysScroll = 41,
  kKeybindings = 42,
  kMediaKeysAreFkeys = 43,
  kMetaSendsEscape = 44,
  kMouseRightClickPaste = 45,
  kMousePasteButton = 46,
  kWordBreakMatchLeft = 47,
  kWordBreakMatchRight = 48,
  kWordBreakMatchMiddle = 49,
  kPageKeysScroll = 50,
  kPassAltNumber = 51,
  kPassCtrlNumber = 52,
  kPassCtrlN = 53,
  kPassCtrlT = 54,
  kPassCtrlTab = 55,
  kPassCtrlW = 56,
  kPassMetaNumber = 57,
  kPassMetaV = 58,
  kPasteOnDrop = 59,
  kReceiveEncoding = 60,
  kScrollOnKeystroke = 61,
  kScrollOnOutput = 62,
  kScrollbarVisible = 63,
  kScrollWheelMaySendArrowKeys = 64,
  kScrollWheelMoveMultiplier = 65,
  kTerminalEncoding = 66,
  kShiftInsertPaste = 67,
  kUserCss = 68,
  kUserCssText = 69,
  kAllowImagesInline = 70,
  kTheme = 71,
  kThemeVariations = 72,
  kFindResultColor = 73,
  kFindResultSelectedColor = 74,
  kLineHeightPaddingSize = 75,
  kKeybindingsOsDefaults = 76,
  kScreenPaddingSize = 77,
  kScreenBorderSize = 78,
  kScreenBorderColor = 79,
  kLineHeight = 80,
  kMaxValue = kLineHeight,
};

const std::string& GetTerminalHomeUrl();

// Generate URL to launch terminal.
GURL GenerateTerminalURL(Profile* profile,
                         const std::string& settings_profile,
                         const guest_os::GuestId& container_id,
                         const std::string& cwd = "",
                         const std::vector<std::string>& terminal_args = {});

// Launches the terminal tabbed app.
void LaunchTerminal(Profile* profile,
                    int64_t display_id,
                    const guest_os::GuestId& container_id,
                    const std::string& cwd = "",
                    const std::vector<std::string>& terminal_args = {});

void LaunchTerminalHome(Profile* profile, int64_t display_id, int restore_id);

void LaunchTerminalWithUrl(Profile* profile,
                           int64_t display_id,
                           int restore_id,
                           const GURL& url);

void LaunchTerminalWithIntent(
    Profile* profile,
    int64_t display_id,
    apps::IntentPtr intent,
    base::OnceCallback<void(bool success, const std::string& msg)> callback);

// Launches the terminal settings popup window.
void LaunchTerminalSettings(Profile* profile,
                            int64_t display_id = display::kInvalidDisplayId);

// Record which terminal settings have been changed by users.
void RecordTerminalSettingsChangesUMAs(Profile* profile);

// Returns terminal setting 'background-color' to use for |url|.
std::string GetTerminalSettingBackgroundColor(
    Profile* profile,
    GURL url,
    std::optional<SkColor> opener_background_color);

// Returns terminal setting 'pass-ctrl-w'.
bool GetTerminalSettingPassCtrlW(Profile* profile);

// Menu shortcut ID for SSH with specified nassh profile-id.
std::string ShortcutIdForSSH(const std::string& profileId);

// Menu shortcut ID for Linux container.
std::string ShortcutIdFromContainerId(Profile* profile,
                                      const guest_os::GuestId& id);

// Parse Intent extras from shortcut ID.
base::flat_map<std::string, std::string> ExtrasFromShortcutId(
    const base::Value::Dict& shortcut);

// Returns list of SSH connections {<profile-id>, <description>}.
std::vector<std::pair<std::string, std::string>> GetSSHConnections(
    Profile* profile);

// Add terminal menu items (Settings, Shut down Linux).
void AddTerminalMenuItems(Profile* profile, apps::MenuItems& menu_items);

// Add terminal shortcut items in menu.
void AddTerminalMenuShortcuts(
    Profile* profile,
    int next_command_id,
    apps::MenuItems menu_items,
    base::OnceCallback<void(apps::MenuItems)> callback,
    std::vector<gfx::ImageSkia> icons = {});

// Called when user clicks on terminal menu items. Returns true if |shortcut_id|
// is recognized and handled.
bool ExecuteTerminalMenuShortcutCommand(Profile* profile,
                                        const std::string& shortcut_id,
                                        int64_t display_id);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TERMINAL_H_
