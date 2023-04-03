// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keeplist_chromeos.h"

#include <stddef.h>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace extensions {

namespace {

// For backward compatibility, we still expose the static ash extension keeplist
// to Lacros to support the older Ash versions which do not pass ash extension
// keeplist data to Lacros via crosapi::mojom::BrowserInitParams (introduced in
// M109).
// TODO(crbug/1371661): Do not expose the static ash extension keeplist data
// in Lacros build after M112.

// For any extension running in both Ash and Lacros, if it needs to be published
// in app service, it must be added to one of app service block lists (Ash or
// Lacros), so that it won't be published by both.
base::span<const base::StringPiece>
ExtensionsRunInOSAndStandaloneBrowserAllowlist() {
  static const base::StringPiece kKeeplist[] = {
      extension_misc::kGCSEExtensionId,
      extension_misc::kGnubbyV3ExtensionId,
      extension_misc::kPdfExtensionId,
  };
  return base::make_span(kKeeplist);
}

// For any extension apps running in both Ash and Lacros, it must be added to
// one of app service block lists (Ash or Lacros), so that it won't be published
// by both.
base::span<const base::StringPiece>
ExtensionAppsRunInOSAndStandaloneBrowserAllowlist() {
  static const base::StringPiece kKeeplist[] = {
      extension_misc::kGnubbyAppId,
  };

  return base::make_span(kKeeplist);
}

base::span<const base::StringPiece> ExtensionsRunInOSOnlyAllowlist() {
  static const base::StringPiece kKeeplist[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    extension_misc::kEspeakSpeechSynthesisExtensionId,
    extension_misc::kGoogleSpeechSynthesisExtensionId,
    extension_misc::kEnhancedNetworkTtsExtensionId,
    extension_misc::kSelectToSpeakExtensionId,
    extension_misc::kAccessibilityCommonExtensionId,
    extension_misc::kSwitchAccessExtensionId,
    extension_misc::kSigninProfileTestExtensionId,
    extension_misc::kGuestModeTestExtensionId,
    extension_misc::kHelpAppExtensionId,
    file_manager::kImageLoaderExtensionId,
#endif
    extension_misc::kKeyboardExtensionId,
    extension_misc::kChromeVoxExtensionId,
    extension_misc::kBruSecurityKeyForwarderExtensionId,
  };

  return base::make_span(kKeeplist);
}

base::span<const base::StringPiece> ExtensionAppsRunInOSOnlyAllowlist() {
  static const base::StringPiece kKeeplist[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc::kPlayStoreAppId,
    extension_misc::kFilesManagerAppId,
#endif
    extension_misc::kGoogleKeepAppId,
    extension_misc::kCalculatorAppId,
    extension_misc::kInAppPaymentsSupportAppId,
    extension_misc::kIdentityApiUiAppId
  };

  return base::make_span(kKeeplist);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The list of the extension apps blocked for app service in Ash.
// The app on the block list can run in Ash but can't be published to app
// service by Ash. For an app running in both Ash and Lacros, if it should be
// published by Lacros, it must be blocked in Ash.
base::span<const base::StringPiece> ExtensionAppsAppServiceBlocklistInOS() {
  // Note: gnubbyd chrome app is running in both Ash and Lacros, but only the
  // app running in Lacros should be published in app service so that it can be
  // launched by users, the one running in Ash is blocked from app service and
  // is invisible to users.
  static const base::StringPiece kBlocklist[] = {
      extension_misc::kGnubbyAppId,
  };

  return base::make_span(kBlocklist);
}

// The list of the extensions blocked for app service in Ash.
// The extension on the block list can run in Ash but can't be published to app
// service by Ash. For an extension running in both Ash and Lacros, if it should
// be published by Lacros, it must be blocked in Ash.
const std::vector<base::StringPiece>& ExtensionsAppServiceBlocklistInOS() {
  // Note: Add extensions to be blocked if there are any in the future.
  static const base::NoDestructor<std::vector<base::StringPiece>> block_list;
  return *block_list;
}

// The list of the extension apps blocked for app service in Lacros.
// The app on the block list can run in Lacros but can't be published to app
// service by Lacros. For an app running in both Ash and Lacros, if it should be
// published by Ash, it must be blocked in Lacros.
const std::vector<base::StringPiece>&
ExtensionAppsAppServiceBlocklistInStandaloneBrowser() {
  // Note: Add extension apps to be blocked if there are any in the future.
  static const base::NoDestructor<std::vector<base::StringPiece>> block_list;
  return *block_list;
}

// The list of the extensions blocked for app service in Lacros.
// The extension on the block list can run in Lacros but can't be published to
// app service by Lacros. For an extension running in both Ash and Lacros, if it
// should be published by Ash, it must be blocked in Lacros.
const std::vector<base::StringPiece>&
ExtensionsAppServiceBlocklistInStandaloneBrowser() {
  // Note: Add extensions to be blocked if there are any in the future.
  static const base::NoDestructor<std::vector<base::StringPiece>> block_list;
  return *block_list;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const std::vector<base::StringPiece>&
ExtensionsRunInOSAndStandaloneBrowserFromBrowserInitParams() {
  // Cache the ash extension keeplist data (passed from Ash to Lacros) provided
  // by chromeos::BrowserParamsProxy. chromeos::BrowserParamsProxy::Get()
  // accesses a static object constructed with base::NoDestructor, which is
  // guaranteed not to be destroyed when it is accessed.
  static const base::NoDestructor<std::vector<base::StringPiece>> keep_list([] {
    std::vector<base::StringPiece> ids;
    auto& ash_keep_list_param =
        chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();
    DCHECK(!ash_keep_list_param.is_null());
    for (const auto& id :
         ash_keep_list_param->extensions_run_in_os_and_standalonebrowser) {
      ids.push_back(id);
    }
    return ids;
  }());
  return *keep_list;
}

const std::vector<base::StringPiece>&
ExtensionAppsRunInOSAndStandaloneBrowserFromBrowserInitParams() {
  // Cache the ash extension keeplist data (passed from Ash to Lacros) provided
  // by chromeos::BrowserParamsProxy. chromeos::BrowserParamsProxy::Get()
  // accesses a static object constructed with base::NoDestructor, which is
  // guaranteed not to be destroyed when it is accessed.
  static const base::NoDestructor<std::vector<base::StringPiece>> keep_list([] {
    std::vector<base::StringPiece> ids;
    auto& ash_keep_list_param =
        chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();
    DCHECK(!ash_keep_list_param.is_null());
    for (const auto& id :
         ash_keep_list_param->extension_apps_run_in_os_and_standalonebrowser) {
      ids.push_back(id);
    }
    return ids;
  }());
  return *keep_list;
}

const std::vector<base::StringPiece>&
ExtensionsRunInOSOnlyFromBrowserInitParams() {
  // Cache the ash extension keeplist data (passed from Ash to Lacros) provided
  // by chromeos::BrowserParamsProxy. chromeos::BrowserParamsProxy::Get()
  // accesses a static object constructed with base::NoDestructor, which is
  // guaranteed not to be destroyed when it is accessed.
  static const base::NoDestructor<std::vector<base::StringPiece>> keep_list([] {
    std::vector<base::StringPiece> ids;
    auto& ash_keep_list_param =
        chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();
    DCHECK(!ash_keep_list_param.is_null());
    for (const auto& id : ash_keep_list_param->extensions_run_in_os_only) {
      ids.push_back(id);
    }
    return ids;
  }());
  return *keep_list;
}

const std::vector<base::StringPiece>&
ExtensionAppsRunInOSOnlyFromBrowserInitParams() {
  // Cache the ash extension keeplist data (passed from Ash to Lacros) provided
  // by chromeos::BrowserParamsProxy. chromeos::BrowserParamsProxy::Get()
  // accesses a static object constructed with base::NoDestructor, which is
  // guaranteed not to be destroyed when it is accessed.
  static const base::NoDestructor<std::vector<base::StringPiece>> keep_list([] {
    std::vector<base::StringPiece> ids;
    auto& ash_keep_list_param =
        chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();
    DCHECK(!ash_keep_list_param.is_null());
    for (const auto& id : ash_keep_list_param->extension_apps_run_in_os_only) {
      ids.push_back(id);
    }
    return ids;
  }());
  return *keep_list;
}

base::span<const base::StringPiece>
GetExtensionsRunInOSAndStandaloneBrowserLacros() {
  auto& ash_keep_list_param =
      chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();

  // For ash in older version which does not support passing ash extension
  // keeplist via crosapi::mojom::BrowserInitParams introduced in M109, fallback
  // to use static compiled allowlist.
  // TODO(crbug/1371661): Remove the backward compatibility handling code in
  // M112.
  if (ash_keep_list_param.is_null()) {
    return ExtensionsRunInOSAndStandaloneBrowserAllowlist();
  }

  return base::make_span(
      ExtensionsRunInOSAndStandaloneBrowserFromBrowserInitParams().data(),
      ExtensionsRunInOSAndStandaloneBrowserFromBrowserInitParams().size());
}

base::span<const base::StringPiece>
GetExtensionAppsRunInOSAndStandaloneBrowserLacros() {
  auto& ash_keep_list_param =
      chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();

  // For ash in older version which does not support passing ash extension
  // keeplist via crosapi::mojom::BrowserInitParams introduced in M109, fallback
  // to use static compiled allowlist.
  // TODO(crbug/1371661): Remove the backward compatibility handling code in
  // M112.
  if (ash_keep_list_param.is_null()) {
    return ExtensionAppsRunInOSAndStandaloneBrowserAllowlist();
  }

  return base::make_span(
      ExtensionAppsRunInOSAndStandaloneBrowserFromBrowserInitParams().data(),
      ExtensionAppsRunInOSAndStandaloneBrowserFromBrowserInitParams().size());
}

base::span<const base::StringPiece> GetExtensionsRunInOSOnlyLacros() {
  auto& ash_keep_list_param =
      chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();

  // For ash in older version which does not support passing ash extension
  // keeplist via crosapi::mojom::BrowserInitParams introduced in M109, fallback
  // to use static compiled allowlist.
  // TODO(crbug/1371661): Remove the backward compatibility handling code in
  // M112.
  if (ash_keep_list_param.is_null()) {
    return ExtensionsRunInOSOnlyAllowlist();
  }

  return base::make_span(ExtensionsRunInOSOnlyFromBrowserInitParams().data(),
                         ExtensionsRunInOSOnlyFromBrowserInitParams().size());
}

base::span<const base::StringPiece> GetExtensionAppsRunInOSOnlyLacros() {
  auto& ash_keep_list_param =
      chromeos::BrowserParamsProxy::Get()->ExtensionKeepList();

  // For ash in older version which does not support passing ash extension
  // keeplist via crosapi::mojom::BrowserInitParams introduced in M109, fallback
  // to use static compiled allowlist.
  // TODO(crbug/1371661): Remove the backward compatibility handling code in
  // M112.
  if (ash_keep_list_param.is_null()) {
    return ExtensionAppsRunInOSOnlyAllowlist();
  }

  return base::make_span(
      ExtensionAppsRunInOSOnlyFromBrowserInitParams().data(),
      ExtensionAppsRunInOSOnlyFromBrowserInitParams().size());
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::span<const base::StringPiece>
GetExtensionAppsRunInOSAndStandaloneBrowser() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ExtensionAppsRunInOSAndStandaloneBrowserAllowlist();
#else  // IS_CHROMEOS_LACROS
  return GetExtensionAppsRunInOSAndStandaloneBrowserLacros();
#endif
}

base::span<const base::StringPiece> GetExtensionAppsRunInOSOnly() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ExtensionAppsRunInOSOnlyAllowlist();
#else  // IS_CHROMEOS_LACROS
  return GetExtensionAppsRunInOSOnlyLacros();
#endif
}

base::span<const base::StringPiece> GetExtensionsRunInOSAndStandaloneBrowser() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ExtensionsRunInOSAndStandaloneBrowserAllowlist();
#else  // IS_CHROMEOS_LACROS
  return GetExtensionsRunInOSAndStandaloneBrowserLacros();
#endif
}

base::span<const base::StringPiece> GetExtensionsRunInOSOnly() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ExtensionsRunInOSOnlyAllowlist();
#else  // IS_CHROMEOS_LACROS
  return GetExtensionsRunInOSOnlyLacros();
#endif
}

bool ExtensionRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id) {
  return base::Contains(GetExtensionsRunInOSAndStandaloneBrowser(),
                        extension_id);
}

bool ExtensionAppRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id) {
  return base::Contains(GetExtensionAppsRunInOSAndStandaloneBrowser(),
                        extension_id);
}

bool ExtensionRunsInOS(const std::string& extension_id) {
  // Note: IME component extensions are available in Ash build only,therefore,
  // we don't need to pass them to Lacros.
  bool is_ime = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_ime = ash::input_method::ComponentExtensionIMEManagerDelegateImpl::
      IsIMEExtensionID(extension_id);
#endif

  return base::Contains(GetExtensionsRunInOSOnly(), extension_id) ||
         ExtensionRunsInBothOSAndStandaloneBrowser(extension_id) || is_ime;
}

bool ExtensionAppRunsInOS(const std::string& app_id) {
  return base::Contains(GetExtensionAppsRunInOSAndStandaloneBrowser(),
                        app_id) ||
         base::Contains(GetExtensionAppsRunInOSOnly(), app_id);
}

bool ExtensionAppRunsInOSOnly(base::StringPiece app_id) {
  return base::Contains(GetExtensionAppsRunInOSOnly(), app_id);
}

bool ExtensionRunsInOSOnly(base::StringPiece extension_id) {
  return base::Contains(GetExtensionsRunInOSOnly(), extension_id);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsAppServiceBlocklistCrosapiSupported() {
  const auto* block_list = chromeos::BrowserParamsProxy::Get()
                               ->StandaloneBrowserAppServiceBlockList();
  return block_list != nullptr;
}

bool ExtensionAppBlockListedForAppServiceInStandaloneBrowser(
    base::StringPiece app_id) {
  const auto* block_list = chromeos::BrowserParamsProxy::Get()
                               ->StandaloneBrowserAppServiceBlockList();
  DCHECK(block_list);
  return base::Contains(block_list->extension_apps, app_id);
}

bool ExtensionBlockListedForAppServiceInStandaloneBrowser(
    base::StringPiece extension_id) {
  const auto* block_list = chromeos::BrowserParamsProxy::Get()
                               ->StandaloneBrowserAppServiceBlockList();
  DCHECK(block_list);
  return base::Contains(block_list->extensions, extension_id);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool ExtensionAppBlockListedForAppServiceInOS(base::StringPiece app_id) {
  return base::Contains(ExtensionAppsAppServiceBlocklistInOS(), app_id);
}

bool ExtensionBlockListedForAppServiceInOS(base::StringPiece extension_id) {
  return base::Contains(ExtensionsAppServiceBlocklistInOS(), extension_id);
}
#endif

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
