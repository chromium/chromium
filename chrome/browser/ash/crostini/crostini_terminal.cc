// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
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

constexpr char kShortcutKey[] = "shortcut";
constexpr char kShortcutValueSSH[] = "ssh";
constexpr char kShortcutValueTerminal[] = "terminal";

std::string ShortcutIdForSSH() {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kShortcutKey, base::Value(kShortcutValueSSH));
  std::string shortcut_id;
  base::JSONWriter::Write(dict, &shortcut_id);
  return shortcut_id;
}

std::string ShortcutIdFromContainerId(const crostini::ContainerId& id) {
  base::Value dict = id.ToDictValue();
  dict.SetKey(kShortcutKey, base::Value(kShortcutValueTerminal));
  std::string shortcut_id;
  base::JSONWriter::Write(dict, &shortcut_id);
  return shortcut_id;
}

base::flat_map<std::string, std::string> ExtrasFromShortcutId(
    const base::Value& shortcut) {
  base::flat_map<std::string, std::string> extras;
  for (const auto it : shortcut.DictItems()) {
    extras[it.first] = it.second.GetString();
  }
  return extras;
}

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

void LaunchTerminalImpl(Profile* profile,
                        const GURL& url,
                        const apps::AppLaunchParams& params) {
  // This function is called asynchronously, so we need to check whether
  // `profile` is still valid first.
  if (g_browser_process) {
    auto* profile_manager = g_browser_process->profile_manager();
    if (profile_manager && profile_manager->IsValidProfile(profile)) {
      // This LaunchSystemWebAppImpl call is necessary. Terminal App uses its
      // own CrostiniApps publisher for launching. Calling
      // LaunchSystemWebAppAsync would ask AppService to launch the App, which
      // routes the launch request to this function, resulting in a loop.
      //
      // System Web Apps managed by Web App publisher should call
      // LaunchSystemWebAppAsync.
      web_app::LaunchSystemWebAppImpl(profile, web_app::SystemAppType::TERMINAL,
                                      url, params);
      return;
    }
  }
  LOG(WARNING) << "Profile becomes invalid. Abort launching terminal.";
}

// Loads |resource_ids| and appends the gfx::ImageSkia results to |images|.
// Invokes |callback| with |images| when complete.
void LoadIconsFromResources(
    std::vector<int> resource_ids,
    std::vector<gfx::ImageSkia> images,
    base::OnceCallback<void(std::vector<gfx::ImageSkia>)> callback) {
  if (images.size() >= resource_ids.size()) {
    return std::move(callback).Run(std::move(images));
  }
  auto resource_id = resource_ids[images.size()];
  apps::LoadIconFromResource(
      apps::IconType::kStandard, apps::kAppShortcutIconSizeDip, resource_id,
      /*placeholder=*/false, apps::IconEffects::kNone,
      base::BindOnce(
          [](std::vector<int> resource_ids, std::vector<gfx::ImageSkia> images,
             base::OnceCallback<void(std::vector<gfx::ImageSkia>)> callback,
             apps::IconValuePtr icon) {
            images.emplace_back(std::move(icon->uncompressed));
            LoadIconsFromResources(std::move(resource_ids), std::move(images),
                                   std::move(callback));
          },
          std::move(resource_ids), std::move(images), std::move(callback)));
}

}  // namespace

void LaunchTerminal(Profile* profile,
                    int64_t display_id,
                    const ContainerId& container_id,
                    const std::string& cwd,
                    const std::vector<std::string>& terminal_args) {
  GURL vsh_in_crosh_url =
      GenerateVshInCroshUrl(profile, container_id, cwd, terminal_args);
  LaunchTerminalWithUrl(profile, display_id, vsh_in_crosh_url);
}

void LaunchTerminalForSSH(Profile* profile, int64_t display_id) {
  LaunchTerminalWithUrl(profile, display_id,
                        GURL("chrome-untrusted://terminal/html/nassh.html"));
}

void LaunchTerminalWithUrl(Profile* profile,
                           int64_t display_id,
                           const GURL& url) {
  if (url.DeprecatedGetOriginAsURL() != chrome::kChromeUIUntrustedTerminalURL) {
    LOG(ERROR) << "Trying to launch terminal with an invalid url: " << url;
    return;
  }

  crostini::RecordAppLaunchHistogram(
      crostini::CrostiniAppLaunchAppType::kTerminal);
  auto params = web_app::CreateSystemWebAppLaunchParams(
      profile, web_app::SystemAppType::TERMINAL, display_id);
  if (!params.has_value()) {
    LOG(WARNING) << "Empty launch params for terminal";
    return;
  }

  // Do not track Crostini apps or terminal in session restore. Apps will fail
  // since VMs are not restarted on restore, and we don't want terminal to
  // force the VM to start.
  params->omit_from_session_restore = true;

  // Always launch asynchronously to avoid disturbing the caller. See
  // https://crbug.com/1262890#c12 for more details.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(LaunchTerminalImpl, profile, url, std::move(*params)));
}

void LaunchTerminalWithIntent(Profile* profile,
                              int64_t display_id,
                              apps::mojom::IntentPtr intent,
                              CrostiniSuccessCallback callback) {
  // Check if crostini is installed.
  if (!CrostiniFeatures::Get()->IsEnabled(profile)) {
    crostini::CrostiniInstaller::GetForProfile(profile)->ShowDialog(
        CrostiniUISurface::kAppList);
    return std::move(callback).Run(false, "Crostini not installed");
  }

  // Look for vm_name and container_name in intent->extras.
  ContainerId container_id = ContainerId::GetDefault();
  if (intent && intent->extras.has_value()) {
    for (const auto& extra : intent->extras.value()) {
      if (extra.first == "vm_name") {
        container_id.vm_name = extra.second;
      } else if (extra.first == "container_name") {
        container_id.container_name = extra.second;
      }
    }
  }

  // Check if we need to show recovery.
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  if (crostini_manager->IsUncleanStartup()) {
    ShowCrostiniRecoveryView(profile, crostini::CrostiniUISurface::kAppList,
                             kCrostiniTerminalSystemAppId, display_id, {},
                             base::DoNothing());
    return std::move(callback).Run(false, "Recovery required");
  }

  // Use first file (if any) as cwd.
  std::string cwd;
  CrostiniManager::RestartOptions options;
  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(profile);
  if (intent && intent->files && intent->files->size()) {
    GURL gurl = intent->files.value()[0]->url;
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    storage::FileSystemURL url = mount_points->CrackURL(
        gurl, blink::StorageKey(url::Origin::Create(gurl)));
    base::FilePath path;
    if (file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
            profile, url, &path)) {
      cwd = path.value();
      if (url.mount_filesystem_id() !=
              file_manager::util::GetCrostiniMountPointName(profile) &&
          !share_path->IsPathShared(container_id.vm_name, url.path())) {
        options.share_paths.push_back(url.path());
      }
    } else {
      LOG(WARNING) << "Failed to parse: " << gurl;
    }
  }

  CrostiniManager::GetForProfile(profile)->RestartCrostiniWithOptions(
      container_id, std::move(options), base::DoNothing());
  LaunchTerminal(profile, display_id, container_id, cwd);
  std::move(callback).Run(true, "");
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

  // Always launch asynchronously to avoid disturbing the caller. See
  // https://crbug.com/1262890#c12 for more details.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          LaunchTerminalImpl, profile,
          GURL(base::StrCat({chrome::kChromeUIUntrustedTerminalURL, path})),
          std::move(*params)));
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

  const base::Value* settings = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);
  for (const auto item : settings->DictItems()) {
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
  const base::Value* value = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);
  const std::string* result = value->FindStringKey(kSettingBackgroundColor);
  return result ? *result : kDefaultBackgroundColor;
}

bool GetTerminalSettingPassCtrlW(Profile* profile) {
  const base::Value* value = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);
  return value->FindBoolKey(kSettingPassCtrlW).value_or(kDefaultPassCtrlW);
}

void AddTerminalMenuItems(Profile* profile,
                          apps::mojom::MenuItemsPtr* menu_items) {
  apps::AddCommandItem(ash::SETTINGS, IDS_INTERNAL_APP_SETTINGS, menu_items);
  if (IsCrostiniRunning(profile)) {
    apps::AddCommandItem(ash::SHUTDOWN_GUEST_OS,
                         IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM, menu_items);
  }
}

void AddTerminalMenuShortcuts(
    Profile* profile,
    int next_command_id,
    apps::mojom::MenuItemsPtr menu_items,
    apps::mojom::Publisher::GetMenuModelCallback callback,
    std::vector<gfx::ImageSkia> images) {
  constexpr bool kIconIndexSSH = 0;
  constexpr bool kIconIndexTerminal = 1;
  if (images.empty()) {
    std::vector<int> resource_ids = {IDR_LOGO_CROSTINI_TERMINAL_SSH,
                                     IDR_CROSTINI_MASCOT};
    return LoadIconsFromResources(
        std::move(resource_ids), std::vector<gfx::ImageSkia>(),
        base::BindOnce(
            [](Profile* profile, int next_command_id,
               apps::mojom::MenuItemsPtr menu_items,
               apps::mojom::Publisher::GetMenuModelCallback callback,
               std::vector<gfx::ImageSkia> images) {
              AddTerminalMenuShortcuts(profile, next_command_id,
                                       std::move(menu_items),
                                       std::move(callback), std::move(images));
            },
            profile, next_command_id, std::move(menu_items),
            std::move(callback)));
  }

  DCHECK_EQ(2, images.size());
  if (base::FeatureList::IsEnabled(chromeos::features::kTerminalSSH)) {
    apps::AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);
    apps::AddShortcutCommandItem(
        next_command_id++, ShortcutIdForSSH(),
        l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_CONNECT_TO_SSH),
        images[kIconIndexSSH], &menu_items);
  }

  if (!CrostiniFeatures::Get()->IsEnabled(profile)) {
    return std::move(callback).Run(std::move(menu_items));
  }

  if (crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile)) {
    const base::Value* container_list =
        profile->GetPrefs()->GetList(crostini::prefs::kCrostiniContainers);
    if (container_list && container_list->GetList().size() > 1) {
      // Shortcuts for each container.
      for (const auto& dict : container_list->GetList()) {
        crostini::ContainerId id(dict);
        if (!id.vm_name.empty() && !id.container_name.empty()) {
          std::string shortcut_id = ShortcutIdFromContainerId(id);
          std::string label =
              base::StrCat({id.vm_name, ":", id.container_name});
          apps::AddShortcutCommandItem(next_command_id++, shortcut_id, label,
                                       images[kIconIndexTerminal], &menu_items);
        }
      }
      return std::move(callback).Run(std::move(menu_items));
    }
  }

  // Single shortcut: 'Connect to Linux'.
  if (base::FeatureList::IsEnabled(chromeos::features::kTerminalSSH)) {
    std::string shortcut_id =
        ShortcutIdFromContainerId(ContainerId::GetDefault());
    apps::AddShortcutCommandItem(
        next_command_id++, shortcut_id,
        l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_CONNECT_TO_LINUX),
        images[kIconIndexTerminal], &menu_items);
  }
  std::move(callback).Run(std::move(menu_items));
}

bool ExecuteTerminalMenuShortcutCommand(Profile* profile,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  auto shortcut = base::JSONReader::Read(shortcut_id);
  if (!shortcut || !shortcut->is_dict()) {
    return false;
  }
  const std::string* shortcut_value = shortcut->FindStringKey(kShortcutKey);
  if (shortcut_value && *shortcut_value == kShortcutValueSSH) {
    LaunchTerminalForSSH(profile, display_id);
    return true;
  }

  if (!shortcut_value || *shortcut_value != kShortcutValueTerminal) {
    return false;
  }
  apps::mojom::IntentPtr intent = apps::mojom::Intent::New();
  intent->extras = ExtrasFromShortcutId(std::move(*shortcut));
  LaunchTerminalWithIntent(profile, display_id, std::move(intent),
                           base::DoNothing());
  return true;
}

}  // namespace crostini
