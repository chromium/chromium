// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_enable_flow.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/events/event_constants.h"

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
      lacros_extensions_util::DemuxPlatformAppId(app_id, &profile, &extension);
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
  // The current implementation of chrome apps menu models never uses the
  // AppService GetMenuModel method.
  NOTREACHED();
}

void LacrosExtensionAppsController::LoadIcon(const std::string& app_id,
                                             apps::IconKeyPtr icon_key,
                                             apps::IconType icon_type,
                                             int32_t size_hint_in_dip,
                                             LoadIconCallback callback) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extensions_util::DemuxPlatformAppId(app_id, &profile, &extension);
  if (success && icon_key) {
    LoadIconFromExtension(
        icon_type, size_hint_in_dip, profile, extension->id(),
        static_cast<apps::IconEffects>(icon_key->icon_effects),
        std::move(callback));
    return;
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(std::make_unique<apps::IconValue>());
}

void LacrosExtensionAppsController::OpenNativeSettings(
    const std::string& app_id) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extensions_util::DemuxPlatformAppId(app_id, &profile, &extension);
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
    apps::WindowMode window_mode) {
  // This method doesn't make sense for packaged v2 apps, which always run in a
  // standalone window.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::Launch(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchCallback callback) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::DemuxPlatformAppId(
      launch_params->app_id, &profile, &extension);
  crosapi::mojom::LaunchResultPtr result = crosapi::mojom::LaunchResult::New();
  if (!success) {
    std::move(callback).Run(std::move(result));
    return;
  }

  if (!extensions::util::IsAppLaunchableWithoutEnabling(extension->id(),
                                                        profile)) {
    auto enable_flow = std::make_unique<apps::ExtensionAppsEnableFlow>(
        profile, extension->id());
    void* key = enable_flow.get();
    enable_flows_[key] = std::move(enable_flow);

    // Calling Run() can result in a synchronous callback. It must be the last
    // thing we do before returning.
    enable_flows_[key]->Run(
        base::BindOnce(&LacrosExtensionAppsController::FinishedEnableFlow,
                       weak_factory_.GetWeakPtr(), std::move(launch_params),
                       std::move(callback), key));
    return;
  }

  auto params = apps::ConvertCrosapiToLaunchParams(launch_params, profile);
  params.app_id = extension->id();

  OpenApplication(profile, std::move(params));

  // TODO(https://crbug.com/1225848): Store the resulting instance token, which
  // will be used to close the instance at a later point in time.
  result->instance_id = base::UnguessableToken::Create();
  std::move(callback).Run(std::move(result));
}

void LacrosExtensionAppsController::ExecuteContextMenuCommand(
    const std::string& app_id,
    const std::string& id,
    ExecuteContextMenuCommandCallback callback) {
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::StopApp(const std::string& app_id) {
  // Find the extension.
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extensions_util::DemuxPlatformAppId(app_id, &profile, &extension);
  if (!success)
    return;

  // Close all app windows.
  for (extensions::AppWindow* app_window :
       extensions::AppWindowRegistry::Get(profile)->GetAppWindowsForApp(
           extension->id())) {
    app_window->GetBaseWindow()->Close();
  }
}

void LacrosExtensionAppsController::SetPermission(
    const std::string& app_id,
    apps::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::FinishedEnableFlow(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchCallback callback,
    void* key,
    bool success) {
  DCHECK(enable_flows_.find(key) != enable_flows_.end());
  enable_flows_.erase(key);

  if (!success) {
    crosapi::mojom::LaunchResultPtr result =
        crosapi::mojom::LaunchResult::New();
    std::move(callback).Run(std::move(result));
    return;
  }

  // The extension was successfully enabled. Try to launch it again.
  Launch(std::move(launch_params), std::move(callback));
}
