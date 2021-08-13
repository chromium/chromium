// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"

LacrosExtensionAppsController::LacrosExtensionAppsController()
    : controller_{this} {}
LacrosExtensionAppsController::~LacrosExtensionAppsController() = default;

void LacrosExtensionAppsController::Initialize(
    mojo::Remote<crosapi::mojom::AppPublisher>& publisher) {
  // Could be unbound if ash is too old.
  if (!publisher.is_bound())
    return;
  publisher->RegisterAppController(controller_.BindNewPipeAndPassRemote());
}

void LacrosExtensionAppsController::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extension_apps_utility::DemuxId(app_id, &profile, &extension);
  if (!success)
    return;

  // UninstallExtension() asynchronously removes site data. |clear_site_data| is
  // unused as there is no way to avoid removing site data.
  std::string extension_id = extension->id();
  std::u16string error;
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UninstallExtension(extension_id,
                           apps::GetExtensionUninstallReason(uninstall_source),
                           &error);

  if (report_abuse) {
    constexpr char kReferrerId[] = "chrome-remove-extension-dialog";
    NavigateParams params(
        profile,
        extension_urls::GetWebstoreReportAbuseUrl(extension_id, kReferrerId),
        ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
}

void LacrosExtensionAppsController::PauseApp(const std::string& app_id) {
  // The concept of pausing and unpausing apps is in the context of time limit
  // enforcement for child accounts. There's currently no mechanism to pause a
  // single app or website. There only exists a mechanism to pause the entire
  // browser. And even that mechanism has an ash-only implementation.
  // TODO(https://crbug.com/1080693): Implement child account support for
  // Lacros.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::UnpauseApp(const std::string& app_id) {
  // The concept of pausing and unpausing apps is in the context of time limit
  // enforcement for child accounts. There's currently no mechanism to pause a
  // single app or website. There only exists a mechanism to pause the entire
  // browser. And even that mechanism has an ash-only implementation.
  // TODO(https://crbug.com/1080693): Implement child account support for
  // Lacros.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::GetMenuModel(
    const std::string& app_id,
    GetMenuModelCallback callback) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::LoadIcon(const std::string& app_id,
                                             apps::mojom::IconKeyPtr icon_key,
                                             apps::mojom::IconType icon_type,
                                             int32_t size_hint_in_dip,
                                             LoadIconCallback callback) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extension_apps_utility::DemuxId(app_id, &profile, &extension);
  if (success && icon_key) {
    LoadIconFromExtension(
        icon_type, size_hint_in_dip, profile, extension->id(),
        static_cast<apps::IconEffects>(icon_key->icon_effects),
        std::move(callback));
    return;
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LacrosExtensionAppsController::OpenNativeSettings(
    const std::string& app_id) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extension_apps_utility::DemuxId(app_id, &profile, &extension);
  if (!success)
    return;

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser) {
    browser =
        Browser::Create(Browser::CreateParams(profile, /*user_gesture=*/true));
  }

  chrome::ShowExtensions(browser, extension->id());
}

void LacrosExtensionAppsController::SetWindowMode(
    const std::string& app_id,
    apps::mojom::WindowMode window_mode) {
  // This method doesn't make sense for packaged v2 apps, which always run in a
  // standalone window.
  NOTIMPLEMENTED();
}
