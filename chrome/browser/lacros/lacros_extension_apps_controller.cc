// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_enable_flow.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/chromeos/extensions/web_file_handlers/intent_util.h"
#include "chrome/browser/extensions/api/file_browser_handler/file_browser_handler_flow_lacros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/web_file_handlers/multiclient_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"

namespace {

// Returns true if |launch_params| has all data reqiured by fileBrowserHandler.
bool CanLaunchWithFileBrowserHandler(
    const crosapi::mojom::LaunchParams& launch_params) {
  return launch_params.intent &&
         launch_params.intent->activity_name.has_value() &&
         launch_params.intent->files.has_value();
}

}  // namespace

// static
std::unique_ptr<LacrosExtensionAppsController>
LacrosExtensionAppsController::MakeForChromeApps() {
  return std::make_unique<LacrosExtensionAppsController>(InitForChromeApps());
}

// static
std::unique_ptr<LacrosExtensionAppsController>
LacrosExtensionAppsController::MakeForExtensions() {
  return std::make_unique<LacrosExtensionAppsController>(InitForExtensions());
}

LacrosExtensionAppsController::LacrosExtensionAppsController(
    const ForWhichExtensionType& which_type)
    : which_type_(which_type),
      controller_{this},
      web_file_handlers_permission_handler_(
          std::make_unique<extensions::WebFileHandlersPermissionHandler>(
              ProfileManager::GetPrimaryUserProfile())) {}

LacrosExtensionAppsController::~LacrosExtensionAppsController() = default;

void LacrosExtensionAppsController::Initialize(
    mojo::Remote<crosapi::mojom::AppPublisher>& publisher) {
  // Could be unbound if ash is too old.
  if (!publisher.is_bound())
    return;
  publisher->RegisterAppController(controller_.BindNewPipeAndPassRemote());
}

void LacrosExtensionAppsController::SetPublisher(
    LacrosExtensionAppsPublisher* publisher) {
  publisher_ = publisher;
}

void LacrosExtensionAppsController::Uninstall(
    const std::string& app_id,
    apps::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::GetProfileAndExtension(
      app_id, &profile, &extension);
  if (!success)
    return;
  DCHECK(which_type_.Matches(extension));

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
  // TODO(crbug.com/40690944): Implement child account support for
  // Lacros.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::UnpauseApp(const std::string& app_id) {
  // The concept of pausing and unpausing apps is in the context of time limit
  // enforcement for child accounts. There's currently no mechanism to pause a
  // single app or website. There only exists a mechanism to pause the entire
  // browser. And even that mechanism has an ash-only implementation.
  // TODO(crbug.com/40690944): Implement child account support for
  // Lacros.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::GetMenuModel(
    const std::string& app_id,
    GetMenuModelCallback callback) {
  // The current implementation of chrome apps menu models never uses the
  // AppService GetMenuModel method.
  NOTREACHED_IN_MIGRATION();
}

void LacrosExtensionAppsController::DEPRECATED_LoadIcon(
    const std::string& app_id,
    apps::IconKeyPtr icon_key,
    apps::IconType icon_type,
    int32_t size_hint_in_dip,
    apps::LoadIconCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void LacrosExtensionAppsController::GetCompressedIcon(
    const std::string& app_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::GetProfileAndExtension(
      app_id, &profile, &extension);
  if (success) {
    GetChromeAppCompressedIconData(profile, app_id, size_in_dip, scale_factor,
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
  bool success = lacros_extensions_util::GetProfileAndExtension(
      app_id, &profile, &extension);
  if (!success)
    return;
  DCHECK(which_type_.Matches(extension));

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser) {
    browser =
        Browser::Create(Browser::CreateParams(profile, /*user_gesture=*/true));
  }

  chrome::ShowExtensions(browser, extension->id());
}

void LacrosExtensionAppsController::UpdateAppSize(const std::string& app_id) {
  DCHECK(publisher_);
  return publisher_->UpdateAppSize(app_id);
}

void LacrosExtensionAppsController::SetWindowMode(
    const std::string& app_id,
    apps::WindowMode window_mode) {
  DCHECK(publisher_);
  publisher_->UpdateAppWindowMode(app_id, window_mode);
}

void LacrosExtensionAppsController::Launch(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchCallback callback) {
  crosapi::mojom::LaunchResultPtr result = crosapi::mojom::LaunchResult::New();
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::GetProfileAndExtension(
      launch_params->app_id, &profile, &extension);
  if (!success) {
    std::move(callback).Run(std::move(result));
    return;
  }
  DCHECK(which_type_.Matches(extension));

  if (!extensions::util::IsAppLaunchableWithoutEnabling(extension->id(),
                                                        profile)) {
    auto enable_flow = std::make_unique<apps::ExtensionAppsEnableFlow>(
        profile, extension->id());
    void* key = enable_flow.get();
    enable_flows_[key] = std::move(enable_flow);

    enable_flows_[key]->Run(
        base::BindOnce(&LacrosExtensionAppsController::FinishedEnableFlow,
                       weak_factory_.GetWeakPtr(), std::move(launch_params),
                       std::move(callback), std::move(result), key));
    return;
  }

  // The extension was successfully enabled. Now try to lanch.
  FinallyLaunch(std::move(launch_params), std::move(callback),
                std::move(result));
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
  bool success = lacros_extensions_util::GetProfileAndExtension(
      app_id, &profile, &extension);
  if (!success)
    return;
  DCHECK(which_type_.Matches(extension));

  if (extension->is_platform_app()) {
    // Close all app windows.
    for (extensions::AppWindow* app_window :
         extensions::AppWindowRegistry::Get(profile)->GetAppWindowsForApp(
             extension->id())) {
      app_window->GetBaseWindow()->Close();
    }
  } else if (extension->is_hosted_app()) {
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppInstanceTracker()
        ->StopInstancesOfApp(app_id);
  } else {
    NOTREACHED_IN_MIGRATION();
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
    crosapi::mojom::LaunchResultPtr result,
    void* key,
    bool success) {
  DCHECK(enable_flows_.find(key) != enable_flows_.end());
  enable_flows_.erase(key);

  if (!success) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // The extension was successfully enabled. Now try to lanch.
  FinallyLaunch(std::move(launch_params), std::move(callback),
                std::move(result));
}

void LacrosExtensionAppsController::FinallyLaunch(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchCallback callback,
    crosapi::mojom::LaunchResultPtr result) {
  // instance_id is required as defined in mojom for app service.
  result->instance_id = base::UnguessableToken::Create();

  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::GetProfileAndExtension(
      launch_params->app_id, &profile, &extension);
  if (!success) {
    std::move(callback).Run(std::move(result));
    return;
  }

  auto params = apps::ConvertCrosapiToLaunchParams(launch_params, profile);
  params.app_id = extension->id();

  if (which_type_.IsChromeApps() ||
      extensions::IsLegacyQuickOfficeExtension(*extension)) {
    OpenApplication(profile, std::move(params));

    // TODO(crbug.com/40188614): Store the resulting instance token,
    // which will be used to close the instance at a later point in time.
    result->state = crosapi::mojom::LaunchResultState::kSuccess;
    std::move(callback).Run(std::move(result));
    return;
  }

  if (which_type_.IsExtensions()) {
    // Web File Handlers use the `file_handlers` manifest key for registration.
    if (extensions::WebFileHandlers::SupportsWebFileHandlers(*extension)) {
      std::vector<base::SafeBaseName> base_names =
          extensions::GetBaseNamesForIntent(*params.intent);

      // This vector cannot be empty because this is reached after explicitly
      // opening one or more files.
      if (base_names.empty()) {
        result->state = crosapi::mojom::LaunchResultState::kFailed;
        std::move(callback).Run(std::move(result));
        return;
      }

      // Confirm that the extension can open the file and then call the
      // callback.
      web_file_handlers_permission_handler_->Confirm(
          *extension, base_names,
          base::BindOnce(
              &LacrosExtensionAppsController::LaunchAppWithArgumentsCallback,
              weak_factory_.GetWeakPtr(), std::move(profile), std::move(params),
              std::move(callback), std::move(result)));
      return;
    }

    // This code path is used only by fileBrowserHandler to open Lacros
    // extension, and is triggered by user using a Lacros extension to handle
    // file open. Therefore we check |launch_params| first, and if that passes,
    // show the (Lacros) browser window, which might not already be open.
    if (!CanLaunchWithFileBrowserHandler(*launch_params) ||
        !ShowBrowserForProfile(profile, params)) {
      // Failure.
      std::move(callback).Run(std::move(result));
      return;
    }

    // Prepare data to be owned by async ExecuteFileBrowserHandlerFlow().
    std::vector<base::FilePath> entry_paths;
    std::vector<std::string> mime_types;
    const std::vector<crosapi::mojom::IntentFilePtr>& intent_files =
        launch_params->intent->files.value();
    for (const auto& intent_file : intent_files) {
      entry_paths.push_back(intent_file->file_path);
      DCHECK(intent_file->mime_type.has_value());
      mime_types.push_back(intent_file->mime_type.value());
    }

    const std::string& task_id = launch_params->intent->activity_name.value();
    extensions::ExecuteFileBrowserHandlerFlow(
        profile, extension, task_id, std::move(entry_paths),
        std::move(mime_types),
        base::BindOnce(
            &LacrosExtensionAppsController::OnExecuteFileBrowserHandlerComplete,
            weak_factory_.GetWeakPtr(), std::move(result),
            std::move(callback)));

    return;
  }

  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(std::move(result));
}

void LacrosExtensionAppsController::LaunchAppWithArgumentsCallback(
    Profile* profile,
    apps::AppLaunchParams params,
    LaunchCallback callback,
    crosapi::mojom::LaunchResultPtr result,
    bool should_open) {
  // Exit early, while notifying, in case `Don't open` was chosen.
  if (!should_open) {
    result->state = crosapi::mojom::LaunchResultState::kFailed;
    std::move(callback).Run(std::move(result));
    return;
  }

  // Reuse a recent foreground window for new tab creation.
  params.container = apps::LaunchContainer::kLaunchContainerTab;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.display_id = display::kInvalidDisplayId;

  // Launch Web File Handlers.
  OpenApplication(profile, std::move(params));
  result->instance_id = base::UnguessableToken::Create();
  result->state = crosapi::mojom::LaunchResultState::kSuccess;
  std::move(callback).Run(std::move(result));
}

void LacrosExtensionAppsController::OnExecuteFileBrowserHandlerComplete(
    crosapi::mojom::LaunchResultPtr result,
    LaunchCallback callback,
    bool success) {
  // TODO(crbug.com/40188614): Store the resulting instance token,
  // which will be used to close the instance at a later point in time.
  result->instance_id = base::UnguessableToken::Create();
  result->state = success ? crosapi::mojom::LaunchResultState::kSuccess
                          : crosapi::mojom::LaunchResultState::kFailed;
  std::move(callback).Run(std::move(result));
}
