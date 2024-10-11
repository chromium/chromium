// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/extension_keeplist_chromeos.h"

#include <stddef.h>

#include <string_view>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
// Additional ids of extensions and extension apps used for testing
// can be passed by ash commandline switches, but this is ONLY allowed
// for testing use.
std::vector<std::string> GetIdsFromCmdlineSwitch(std::string_view ash_switch) {
  std::vector<std::string> ids;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(ash_switch)) {
    ids = base::SplitString(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(ash_switch),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  return ids;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_DEVICE)

// For any extension running in both Ash and Lacros, if it needs to be published
// in app service, it must be added to one of app service block lists (Ash or
// Lacros), so that it won't be published by both.
base::span<const std::string_view>
ExtensionsRunInOSAndStandaloneBrowserAllowlist() {
  static const std::string_view kKeeplist[] = {
      extension_misc::kGnubbyV3ExtensionId,
      extension_misc::kPdfExtensionId,
  };

  static const base::NoDestructor<std::vector<std::string_view>> keep_list([] {
    std::vector<std::string_view> ids;
    for (const auto& id : kKeeplist) {
      ids.push_back(id);
    }
    if (ash::switches::IsAshDebugBrowserEnabled()) {
      ids.push_back(extension_misc::kPerfettoUIExtensionId);
    }
    return ids;
  }());
  return base::make_span(*keep_list);
}

// For any extension apps running in both Ash and Lacros, it must be added to
// one of app service block lists (Ash or Lacros), so that it won't be published
// by both.
base::span<const std::string_view>
ExtensionAppsRunInOSAndStandaloneBrowserAllowlist() {
  static const std::string_view kKeeplist[] = {
      extension_misc::kGnubbyAppId,
  };

  return base::make_span(kKeeplist);
}

base::span<const std::string_view> ExtensionsRunInOSOnlyAllowlist() {
  static const std::string_view kKeeplist[] = {
      extension_misc::kAccessibilityCommonExtensionId,
      extension_misc::kEnhancedNetworkTtsExtensionId,
      extension_misc::kEspeakSpeechSynthesisExtensionId,
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      extension_misc::kGuestModeTestExtensionId,
      extension_misc::kHelpAppExtensionId,
      extension_misc::kSelectToSpeakExtensionId,
      extension_misc::kSigninProfileTestExtensionId,
      extension_misc::kSwitchAccessExtensionId,
      file_manager::kImageLoaderExtensionId,
      extension_misc::kBruSecurityKeyForwarderExtensionId,
      extension_misc::kChromeVoxExtensionId,
      extension_misc::kKeyboardExtensionId,
  };

  return base::make_span(kKeeplist);
}

base::span<const std::string_view> ExtensionAppsRunInOSOnlyAllowlist() {
  static const std::string_view kKeeplist[] = {
      arc::kPlayStoreAppId,
      extension_misc::kFilesManagerAppId,
  };

  return base::make_span(kKeeplist);
}

// The list of the extension apps blocked for app service in Ash.
// The app on the block list can run in Ash but can't be published to app
// service by Ash. For an app running in both Ash and Lacros, if it should be
// published by Lacros, it must be blocked in Ash.
base::span<const std::string_view> ExtensionAppsAppServiceBlocklistInOS() {
  // Note: gnubbyd chrome app is running in both Ash and Lacros, but only the
  // app running in Lacros should be published in app service so that it can be
  // launched by users, the one running in Ash is blocked from app service and
  // is invisible to users.
  static const std::string_view kBlocklist[] = {
      extension_misc::kGnubbyAppId,
  };

  return base::make_span(kBlocklist);
}

// The list of the extensions blocked for app service in Ash.
// The extension on the block list can run in Ash but can't be published to app
// service by Ash. For an extension running in both Ash and Lacros, if it should
// be published by Lacros, it must be blocked in Ash.
const std::vector<std::string_view>& ExtensionsAppServiceBlocklistInOS() {
  // Note: Add extensions to be blocked if there are any in the future.
  static const base::NoDestructor<std::vector<std::string_view>> block_list;
  return *block_list;
}

// The list of the extension apps blocked for app service in Lacros.
// The app on the block list can run in Lacros but can't be published to app
// service by Lacros. For an app running in both Ash and Lacros, if it should be
// published by Ash, it must be blocked in Lacros.
const std::vector<std::string_view>&
ExtensionAppsAppServiceBlocklistInStandaloneBrowser() {
  // Note: Add extension apps to be blocked if there are any in the future.
  static const base::NoDestructor<std::vector<std::string_view>> block_list;
  return *block_list;
}

// The list of the extensions blocked for app service in Lacros.
// The extension on the block list can run in Lacros but can't be published to
// app service by Lacros. For an extension running in both Ash and Lacros, if it
// should be published by Ash, it must be blocked in Lacros.
const std::vector<std::string_view>&
ExtensionsAppServiceBlocklistInStandaloneBrowser() {
  // Note: Add extensions to be blocked if there are any in the future.
  static const base::NoDestructor<std::vector<std::string_view>> block_list;
  return *block_list;
}

}  // namespace

crosapi::mojom::ExtensionKeepListPtr BuildExtensionKeeplistInitParam() {
  auto keep_list_param = crosapi::mojom::ExtensionKeepList::New();
  for (const auto& id : ExtensionAppsRunInOSAndStandaloneBrowserAllowlist()) {
    keep_list_param->extension_apps_run_in_os_and_standalonebrowser.push_back(
        std::string(id));
  }

  for (const auto& id : ExtensionAppsRunInOSOnlyAllowlist()) {
    keep_list_param->extension_apps_run_in_os_only.push_back(std::string(id));
  }

  for (const auto& id : ExtensionsRunInOSAndStandaloneBrowserAllowlist()) {
    keep_list_param->extensions_run_in_os_and_standalonebrowser.push_back(
        std::string(id));
  }

  for (const auto& id : ExtensionsRunInOSOnlyAllowlist()) {
    keep_list_param->extensions_run_in_os_only.push_back(std::string(id));
  }

#if !BUILDFLAG(IS_CHROMEOS_DEVICE)  // IN-TEST
  // Append additional ids of the testing extensions and extension apps.
  std::vector<std::string> ids = GetIdsFromCmdlineSwitch(
      ash::switches::kExtensionAppsRunInBothAshAndLacros);
  keep_list_param->extension_apps_run_in_os_and_standalonebrowser.insert(
      keep_list_param->extension_apps_run_in_os_and_standalonebrowser.end(),
      ids.begin(), ids.end());

  ids = GetIdsFromCmdlineSwitch(ash::switches::kExtensionAppsRunInAshOnly);
  keep_list_param->extension_apps_run_in_os_only.insert(
      keep_list_param->extension_apps_run_in_os_only.end(), ids.begin(),
      ids.end());

  ids =
      GetIdsFromCmdlineSwitch(ash::switches::kExtensionsRunInBothAshAndLacros);
  keep_list_param->extensions_run_in_os_and_standalonebrowser.insert(
      keep_list_param->extensions_run_in_os_and_standalonebrowser.end(),
      ids.begin(), ids.end());

  ids = GetIdsFromCmdlineSwitch(ash::switches::kExtensionsRunInAshOnly);
  keep_list_param->extensions_run_in_os_only.insert(
      keep_list_param->extensions_run_in_os_only.end(), ids.begin(), ids.end());
#endif  // !BUILDFLAG(IS_CHROMEOS_DEVICE)

  return keep_list_param;
}

crosapi::mojom::StandaloneBrowserAppServiceBlockListPtr
BuildStandaloneBrowserAppServiceBlockListInitParam() {
  auto app_service_block_list =
      crosapi::mojom::StandaloneBrowserAppServiceBlockList::New();
  for (const auto& id : ExtensionAppsAppServiceBlocklistInStandaloneBrowser()) {
    app_service_block_list->extension_apps.emplace_back(id);
  }
  for (const auto& id : ExtensionsAppServiceBlocklistInStandaloneBrowser()) {
    app_service_block_list->extensions.emplace_back(id);
  }

  return app_service_block_list;
}

base::span<const std::string_view>
GetExtensionAppsRunInOSAndStandaloneBrowser() {
  return ExtensionAppsRunInOSAndStandaloneBrowserAllowlist();
}

base::span<const std::string_view> GetExtensionAppsRunInOSOnly() {
  return ExtensionAppsRunInOSOnlyAllowlist();
}

base::span<const std::string_view> GetExtensionsRunInOSAndStandaloneBrowser() {
  return ExtensionsRunInOSAndStandaloneBrowserAllowlist();
}

base::span<const std::string_view> GetExtensionsRunInOSOnly() {
  return ExtensionsRunInOSOnlyAllowlist();
}

bool ExtensionRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  return base::Contains(GetExtensionsRunInOSAndStandaloneBrowser(),
                        extension_id);
#else
  return base::Contains(GetExtensionsRunInOSAndStandaloneBrowser(),
                        extension_id) ||
         base::Contains(GetIdsFromCmdlineSwitch(  // IN-TEST
                            ash::switches::kExtensionsRunInBothAshAndLacros),
                        extension_id);
#endif
}

bool ExtensionAppRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  return base::Contains(GetExtensionAppsRunInOSAndStandaloneBrowser(),
                        extension_id);
#else
  return base::Contains(GetExtensionAppsRunInOSAndStandaloneBrowser(),
                        extension_id) ||
         base::Contains(GetIdsFromCmdlineSwitch(  // IN-TEST
                            ash::switches::kExtensionAppsRunInBothAshAndLacros),
                        extension_id);
#endif
}

bool ExtensionRunsInOS(const std::string& extension_id) {
  bool is_ime = false;
  is_ime = ash::input_method::ComponentExtensionIMEManagerDelegateImpl::
      IsIMEExtensionID(extension_id);

  return base::Contains(GetExtensionsRunInOSOnly(), extension_id) ||
         ExtensionRunsInBothOSAndStandaloneBrowser(extension_id) || is_ime;
}

bool ExtensionAppRunsInOS(const std::string& app_id) {
  return base::Contains(GetExtensionAppsRunInOSAndStandaloneBrowser(),
                        app_id) ||
         base::Contains(GetExtensionAppsRunInOSOnly(), app_id);
}

bool ExtensionAppRunsInOSOnly(std::string_view app_id) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  return base::Contains(GetExtensionAppsRunInOSOnly(), app_id);
#else
  return base::Contains(GetExtensionAppsRunInOSOnly(), app_id) ||
         base::Contains(  // IN-TEST
             GetIdsFromCmdlineSwitch(ash::switches::kExtensionAppsRunInAshOnly),
             app_id);
#endif
}

bool ExtensionRunsInOSOnly(std::string_view extension_id) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  return base::Contains(GetExtensionsRunInOSOnly(), extension_id);
#else
  return base::Contains(GetExtensionsRunInOSOnly(), extension_id) ||
         base::Contains(  // IN-TEST
             GetIdsFromCmdlineSwitch(ash::switches::kExtensionsRunInAshOnly),
             extension_id);
#endif
}

bool ExtensionAppBlockListedForAppServiceInOS(std::string_view app_id) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  return base::Contains(ExtensionAppsAppServiceBlocklistInOS(), app_id);
#else
  return base::Contains(ExtensionAppsAppServiceBlocklistInOS(), app_id) ||
         base::Contains(  // IN-TEST
             GetIdsFromCmdlineSwitch(
                 ash::switches::kExtensionAppsBlockForAppServiceInAsh),
             app_id);
#endif
}

bool ExtensionBlockListedForAppServiceInOS(std::string_view extension_id) {
  return base::Contains(ExtensionsAppServiceBlocklistInOS(), extension_id);
}

base::span<const std::string_view>
GetExtensionsAndAppsRunInOSAndStandaloneBrowser() {
  static const base::NoDestructor<std::vector<std::string_view>> keep_list([] {
    std::vector<std::string_view> ids;
    for (const auto& id : ExtensionsRunInOSAndStandaloneBrowserAllowlist()) {
      ids.push_back(id);
    }
    for (const auto& id : ExtensionAppsRunInOSAndStandaloneBrowserAllowlist()) {
      ids.push_back(id);
    }
    return ids;
  }());

  return base::make_span(*keep_list);
}

size_t ExtensionsRunInOSAndStandaloneBrowserAllowlistSizeForTest() {
  return ExtensionsRunInOSAndStandaloneBrowserAllowlist().size();
}

size_t ExtensionAppsRunInOSAndStandaloneBrowserAllowlistSizeForTest() {
  return ExtensionAppsRunInOSAndStandaloneBrowserAllowlist().size();
}

size_t ExtensionsRunInOSOnlyAllowlistSizeForTest() {
  return ExtensionsRunInOSOnlyAllowlist().size();
}

size_t ExtensionAppsRunInOSOnlyAllowlistSizeForTest() {
  return ExtensionAppsRunInOSOnlyAllowlist().size();
}

}  // namespace extensions
