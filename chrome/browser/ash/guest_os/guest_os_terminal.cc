// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_terminal.h"

#include <string_view>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_installer_factory.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/native_theme.h"

namespace guest_os {

// web_app::GenerateAppId(/*manifest_id=*/std::nullopt,
//     GURL("chrome-untrusted://terminal/html/terminal.html"))
const char kTerminalSystemAppId[] = "fhicihalidkgcimdmhpohldehjmcabcf";

const char kTerminalHomePath[] = "html/terminal.html#home";

const char kShortcutKey[] = "shortcut";
const char kShortcutValueSSH[] = "ssh";
const char kShortcutValueTerminal[] = "terminal";
const char kProfileIdKey[] = "profileId";

namespace {

constexpr char kSettingPrefix[] = "/hterm/profiles/default/";
const size_t kSettingPrefixSize = std::size(kSettingPrefix) - 1;

constexpr char kSettingsProfileUrlParam[] = "settings_profile";
constexpr char kSettingsPrefixHterm[] = "hterm";
constexpr char kSettingsPrefixNassh[] = "nassh";
constexpr char kSettingsPrefixVsh[] = "vsh";
constexpr char kSettingsKeyBackgroundColor[] = "background-color";
constexpr char kSettingsKeyTerminalProfile[] = "terminal-profile";
constexpr char kSettingsProfileDefault[] = "default";
constexpr char kDefaultBackgroundColor[] = "#202124";

constexpr char kSettingPassCtrlW[] = "/hterm/profiles/default/pass-ctrl-w";
constexpr bool kDefaultPassCtrlW = false;

std::string GetSettingsKey(const std::string& prefix,
                           const std::string& profile,
                           const std::string& key) {
  return base::StrCat({"/", prefix, "/profiles/", profile, "/", key});
}

void LaunchTerminalImpl(Profile* profile,
                        const GURL& url,
                        apps::AppLaunchParams params) {
  // This function is called asynchronously, so we need to check whether
  // `profile` is still valid first.
  if (!g_browser_process) {
    LOG(WARNING) << "Abort launching terminal, invalid browser process.";
    return;
  }

  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager || !profile_manager->IsValidProfile(profile)) {
    LOG(WARNING) << "Abort launching terminal, invalid profile.";
    return;
  }

  // This LaunchSystemWebAppImpl call is necessary. Terminal App uses its
  // own CrostiniApps publisher for launching. Calling
  // LaunchSystemWebAppAsync would ask AppService to launch the App, which
  // routes the launch request to this function, resulting in a loop.
  //
  // System Web Apps managed by Web App publisher should call
  // LaunchSystemWebAppAsync.

  // Launch without a pinned home tab (settings page).
  if (params.disposition == WindowOpenDisposition::NEW_POPUP) {
    ash::LaunchSystemWebAppImpl(profile, ash::SystemWebAppType::TERMINAL, url,
                                params);
    return;
  }

  // TODO(crbug.com/1308961): Migrate to use PWA pinned home tab when ready.
  // If opening a new tab, first pin home tab.
  full_restore::FullRestoreSaveHandler::GetInstance();
  GURL home(GetTerminalHomeUrl());
  Browser* browser = ash::LaunchSystemWebAppImpl(
      profile, ash::SystemWebAppType::TERMINAL, home, params);
  if (!browser) {
    return;
  }
  if (url != home) {
    chrome::AddTabAt(browser, url, /*index=*/1, /*foreground=*/true);
  }
  auto info = std::make_unique<app_restore::AppLaunchInfo>(
      kTerminalSystemAppId, browser->session_id().id(), params.container,
      params.disposition, params.display_id, std::vector<base::FilePath>{},
      nullptr);
  full_restore::SaveAppLaunchInfo(browser->profile()->GetPath(),
                                  std::move(info));
}

}  // namespace

const std::string& GetTerminalHomeUrl() {
  static const base::NoDestructor<std::string> url(
      base::StrCat({chrome::kChromeUIUntrustedTerminalURL, kTerminalHomePath}));
  return *url;
}

GURL GenerateTerminalURL(Profile* profile,
                         const std::string& settings_profile,
                         const guest_os::GuestId& container_id,
                         const std::string& cwd,
                         const std::vector<std::string>& terminal_args) {
  auto escape = [](std::string param) {
    return base::EscapeQueryParamValue(param, /*use_plus=*/true);
  };
  std::string settings_profile_param;
  if (!settings_profile.empty()) {
    settings_profile_param = base::StrCat(
        {"&", kSettingsProfileUrlParam, "=", escape(settings_profile)});
  }
  std::string start = base::StrCat({chrome::kChromeUIUntrustedTerminalURL,
                                    "html/terminal.html?command=vmshell",
                                    settings_profile_param});
  std::string vm_name_param =
      escape(base::StringPrintf("--vm_name=%s", container_id.vm_name.c_str()));
  std::string container_name_param = escape(base::StringPrintf(
      "--target_container=%s", container_id.container_name.c_str()));
  std::string owner_id_param = escape(base::StringPrintf(
      "--owner_id=%s", crostini::CryptohomeIdForProfile(profile).c_str()));

  std::vector<std::string> pieces = {start, vm_name_param, container_name_param,
                                     owner_id_param};
  if (!cwd.empty()) {
    pieces.push_back(escape(base::StringPrintf("--cwd=%s", cwd.c_str())));
  }
  if (!terminal_args.empty()) {
    // Separates the command args from the args we are passing into the
    // terminal to be executed.
    pieces.push_back("--");
    for (auto arg : terminal_args) {
      pieces.push_back(escape(arg));
    }
  }

  return GURL(base::JoinString(pieces, "&args[]="));
}

void LaunchTerminal(Profile* profile,
                    int64_t display_id,
                    const guest_os::GuestId& container_id,
                    const std::string& cwd,
                    const std::vector<std::string>& terminal_args) {
  GURL url = GenerateTerminalURL(profile, /*settings_profile=*/std::string(),
                                 container_id, cwd, terminal_args);
  LaunchTerminalWithUrl(profile, display_id, /*restore_id=*/0, url);
}

void LaunchTerminalHome(Profile* profile, int64_t display_id, int restore_id) {
  LaunchTerminalWithUrl(profile, display_id, restore_id,
                        GURL(GetTerminalHomeUrl()));
}

void LaunchTerminalWithUrl(Profile* profile,
                           int64_t display_id,
                           int restore_id,
                           const GURL& url) {
  if (url.DeprecatedGetOriginAsURL() != chrome::kChromeUIUntrustedTerminalURL) {
    LOG(ERROR) << "Trying to launch terminal with an invalid url: " << url;
    return;
  }

  crostini::RecordAppLaunchHistogram(
      crostini::CrostiniAppLaunchAppType::kTerminal);
  auto params = ash::CreateSystemWebAppLaunchParams(
      profile, ash::SystemWebAppType::TERMINAL, display_id);
  if (!params.has_value()) {
    LOG(WARNING) << "Empty launch params for terminal";
    return;
  }

  // Terminal Home page will be restored by app service.
  params->omit_from_session_restore = true;

  params->restore_id = restore_id;

  // Always launch asynchronously to avoid disturbing the caller. See
  // https://crbug.com/1262890#c12 for more details.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(LaunchTerminalImpl, profile, url, std::move(*params)));
}

void LaunchTerminalWithIntent(
    Profile* profile,
    int64_t display_id,
    apps::IntentPtr intent,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  // Look for vm_name and container_name in intent->extras, and for backcompat
  // reasons default to the original crostini container if nothing is specified.
  guest_os::GuestId guest_id = crostini::DefaultContainerId();

  // We only have vm and container name, so don't usually know the type. Don't
  // need it though so leave it as unknown.
  guest_id.vm_type = guest_os::VmType::UNKNOWN;
  std::string settings_profile;
  if (intent && !intent->extras.empty()) {
    for (const auto& extra : intent->extras) {
      if (extra.first == "vm_name") {
        guest_id.vm_name = extra.second;
      } else if (extra.first == "container_name") {
        guest_id.container_name = extra.second;
      } else if (extra.first == kSettingsProfileUrlParam) {
        settings_profile = extra.second;
      }
    }
  }

  auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile)
                       ->TerminalProviderRegistry();
  auto* provider = registry->Get(guest_id);

  if (!provider) {
    if (guest_id.vm_name == crostini::DefaultContainerId().vm_name &&
        !crostini::CrostiniFeatures::Get()->IsEnabled(profile)) {
      // It used to be that running the terminal without Crostini installed
      // would bring up the installer, so keep that behaviour. Only applies to
      // the default Crostini VM, anything else is only accessible if the target
      // VM is installed.
      auto* installer =
          crostini::CrostiniInstallerFactory::GetForProfile(profile);
      if (installer) {
        installer->ShowDialog(crostini::CrostiniUISurface::kAppList);
      }
      return std::move(callback).Run(false, "Crostini not installed");
    } else {
      // Could happen if, e.g. a guest got disabled between listing and
      // selecting targets.
      return std::move(callback).Run(false, "Unrecognised Guest Id");
    }
  }

  std::string message;
  if (provider->RecoveryRequired(display_id)) {
    return std::move(callback).Run(false, "Recovery required");
  }

  // Use first file (if any) as cwd.
  std::string cwd;
  if (intent && !intent->files.empty()) {
    GURL gurl = intent->files[0]->url;
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    storage::FileSystemURL url = mount_points->CrackURL(
        gurl, blink::StorageKey::CreateFirstParty(url::Origin::Create(gurl)));

    cwd = provider->PrepareCwd(url);
  }

  GURL url = GenerateTerminalURL(profile, settings_profile, guest_id, cwd,
                                 /*terminal_args=*/{});
  LaunchTerminalWithUrl(profile, display_id, /*restore_id=*/0, url);
  std::move(callback).Run(true, "");
}

void LaunchTerminalSettings(Profile* profile, int64_t display_id) {
  auto params = ash::CreateSystemWebAppLaunchParams(
      profile, ash::SystemWebAppType::TERMINAL, display_id);
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
  static constexpr auto kSettingsMap = base::MakeFixedFlatMap<std::string_view,
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
      {"find-result-color", TerminalSetting::kFindResultColor},
      {"find-result-selected-color", TerminalSetting::kFindResultSelectedColor},
      {"line-height-padding-size", TerminalSetting::kLineHeightPaddingSize},
      {"keybindings-os-defaults", TerminalSetting::kKeybindingsOsDefaults},
      {"screen-padding-size", TerminalSetting::kScreenPaddingSize},
      {"screen-border-size", TerminalSetting::kScreenBorderSize},
      {"screen-border-color", TerminalSetting::kScreenBorderColor},
      {"line-height", TerminalSetting::kLineHeight},
  });

  const base::Value::Dict& settings =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsTerminalSettings);
  for (const auto item : settings) {
    // Only record settings for /hterm/profiles/default/.
    if (!base::StartsWith(item.first, kSettingPrefix,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }
    const auto it = kSettingsMap.find(
        std::string_view(item.first).substr(kSettingPrefixSize));
    base::UmaHistogramEnumeration(
        "Crostini.TerminalSettingsChanged",
        it != kSettingsMap.end() ? it->second : TerminalSetting::kUnknown);
  }
}

std::string GetTerminalSettingBackgroundColor(
    Profile* profile,
    GURL url,
    std::optional<SkColor> opener_background_color) {
  auto key = [](const std::string& profile) {
    return GetSettingsKey(kSettingsPrefixHterm, profile,
                          kSettingsKeyBackgroundColor);
  };
  const base::Value::Dict& settings =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsTerminalSettings);
  // 1. Use 'settings_profile' url param.
  std::string settings_profile;
  if (net::GetValueForKeyInQuery(url, kSettingsProfileUrlParam,
                                 &settings_profile)) {
    const std::string* result = settings.FindString(key(settings_profile));
    if (result) {
      return *result;
    }
  }

  // 2. Use same color as opener.
  if (opener_background_color) {
    return ui::ConvertSkColorToCSSColor(*opener_background_color);
  }

  // 3. Use 'default' profile color, or default color.
  const std::string* result = settings.FindString(key(kSettingsProfileDefault));
  return result ? *result : kDefaultBackgroundColor;
}

bool GetTerminalSettingPassCtrlW(Profile* profile) {
  const base::Value::Dict& value =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsTerminalSettings);
  return value.FindBool(kSettingPassCtrlW).value_or(kDefaultPassCtrlW);
}

std::string ShortcutIdForSSH(const std::string& profileId) {
  base::Value::Dict dict;
  dict.Set(kShortcutKey, base::Value(kShortcutValueSSH));
  dict.Set(kProfileIdKey, base::Value(profileId));
  std::string shortcut_id;
  base::JSONWriter::Write(dict, &shortcut_id);
  return shortcut_id;
}

std::string ShortcutIdFromContainerId(Profile* profile,
                                      const guest_os::GuestId& id) {
  base::Value::Dict dict = id.ToDictValue();
  dict.Set(kShortcutKey, base::Value(kShortcutValueTerminal));

  // Find terminal profile from prefs.
  const base::Value::Dict& settings =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsTerminalSettings);
  const base::Value::List* vsh_ids = settings.FindList("/vsh/profile-ids");
  if (vsh_ids) {
    for (const auto& vsh_id : *vsh_ids) {
      if (!vsh_id.is_string()) {
        continue;
      }
      const std::string* vm_name = settings.FindString(
          GetSettingsKey(kSettingsPrefixVsh, vsh_id.GetString(), "vm-name"));
      const std::string* container_name = settings.FindString(GetSettingsKey(
          kSettingsPrefixVsh, vsh_id.GetString(), "container-name"));
      const std::string* settings_profile = settings.FindString(GetSettingsKey(
          kSettingsPrefixVsh, vsh_id.GetString(), "terminal-profile"));
      if (vm_name && *vm_name == id.vm_name && container_name &&
          *container_name == id.container_name && settings_profile) {
        dict.Set(kSettingsProfileUrlParam, *settings_profile);
      }
    }
  }

  std::string shortcut_id;
  base::JSONWriter::Write(dict, &shortcut_id);
  return shortcut_id;
}

base::flat_map<std::string, std::string> ExtrasFromShortcutId(
    const base::Value::Dict& shortcut) {
  base::flat_map<std::string, std::string> extras;
  for (const auto it : shortcut) {
    if (it.second.is_string()) {
      extras[it.first] = it.second.GetString();
    }
  }
  return extras;
}

std::vector<std::pair<std::string, std::string>> GetSSHConnections(
    Profile* profile) {
  std::vector<std::pair<std::string, std::string>> result;
  const base::Value::Dict& settings =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsTerminalSettings);
  const base::Value::List* ids = settings.FindList("/nassh/profile-ids");
  if (!ids) {
    return result;
  }
  for (const auto& id : *ids) {
    if (!id.is_string()) {
      continue;
    }
    const std::string* description = settings.FindString(
        GetSettingsKey(kSettingsPrefixNassh, id.GetString(), "description"));
    if (description) {
      result.emplace_back(id.GetString(), *description);
    }
  }
  return result;
}

void AddTerminalMenuItems(Profile* profile, apps::MenuItems& menu_items) {
  apps::AddCommandItem(ash::SETTINGS, IDS_INTERNAL_APP_SETTINGS, menu_items);
  if (crostini::IsCrostiniRunning(profile)) {
    apps::AddCommandItem(ash::SHUTDOWN_GUEST_OS,
                         IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM, menu_items);
  }
  if (bruschetta::IsBruschettaRunning(profile)) {
    apps::AddCommandItem(ash::SHUTDOWN_BRUSCHETTA_OS,
                         IDS_BRUSCHETTA_SHUT_DOWN_LINUX_MENU_ITEM, menu_items);
  }
}

void AddTerminalMenuShortcuts(
    Profile* profile,
    int next_command_id,
    apps::MenuItems menu_items,
    base::OnceCallback<void(apps::MenuItems)> callback,
    std::vector<gfx::ImageSkia> images) {
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(
          ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
              nullptr));
  auto icon = [color_provider](const gfx::VectorIcon& icon) {
    return ui::ImageModel::FromVectorIcon(icon,
                                          apps::GetColorIdForMenuItemIcon(),
                                          apps::kAppShortcutIconSizeDip)
        .Rasterize(color_provider);
  };
  gfx::ImageSkia terminal_ssh_icon = icon(kTerminalSshIcon);
  gfx::ImageSkia crostini_mascot_icon = icon(kCrostiniMascotIcon);
  std::vector<std::pair<std::string, std::string>> connections =
      GetSSHConnections(profile);
  auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile)
                       ->TerminalProviderRegistry();
  if (connections.size() > 0 || registry->List().size() > 0) {
    apps::AddSeparator(ui::DOUBLE_SEPARATOR, menu_items);
  }

  for (auto id : registry->List()) {
    auto* provider = registry->Get(id);
    apps::AddShortcutCommandItem(
        next_command_id++,
        ShortcutIdFromContainerId(profile, provider->GuestId()),
        provider->Label(), crostini_mascot_icon, menu_items);
  }

  for (const auto& connection : connections) {
    apps::AddShortcutCommandItem(
        next_command_id++, ShortcutIdForSSH(connection.first),
        connection.second, terminal_ssh_icon, menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

bool ExecuteTerminalMenuShortcutCommand(Profile* profile,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  std::optional<base::Value::Dict> shortcut =
      base::JSONReader::ReadDict(shortcut_id);
  if (!shortcut) {
    return false;
  }
  const std::string* shortcut_value = shortcut->FindString(kShortcutKey);
  if (shortcut_value && *shortcut_value == kShortcutValueSSH) {
    const std::string* profileId = shortcut->FindString(kProfileIdKey);
    if (!profileId) {
      return false;
    }
    const base::Value::Dict& settings =
        profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsTerminalSettings);
    const std::string* settings_profile = settings.FindString(GetSettingsKey(
        kSettingsPrefixNassh, *profileId, kSettingsKeyTerminalProfile));
    auto escape = [](const std::string& v) {
      return base::EscapeQueryParamValue(v, /*use_plus=*/true);
    };
    std::string settings_profile_param;
    if (settings_profile && !settings_profile->empty() &&
        *settings_profile != kSettingsProfileDefault) {
      settings_profile_param = base::StrCat(
          {"?", kSettingsProfileUrlParam, "=", escape(*settings_profile)});
    }
    LaunchTerminalWithUrl(
        profile, display_id, /*restore_id=*/0,
        GURL(base::StrCat({chrome::kChromeUIUntrustedTerminalURL,
                           "html/terminal_ssh.html", settings_profile_param,
                           "#profile-id:", escape(*profileId)})));
    return true;
  }

  if (!shortcut_value || *shortcut_value != kShortcutValueTerminal) {
    return false;
  }
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView);
  intent->extras = ExtrasFromShortcutId(*shortcut);
  LaunchTerminalWithIntent(profile, display_id, std::move(intent),
                           base::DoNothing());
  return true;
}

}  // namespace guest_os
