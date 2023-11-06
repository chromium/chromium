// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_CONTROLLER_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/file_handlers/web_file_handlers_permission_handler.h"
#include "chrome/browser/lacros/for_which_extension_type.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class LacrosExtensionAppsPublisher;
class Profile;

namespace apps {
class ExtensionAppsEnableFlow;
struct AppLaunchParams;
}  // namespace apps

// This class is responsible for receiving AppController events from Ash, and
// implementing their effects. Distinct instances should be used to handle
// Chrome Apps and Extensions separately.
//
// See LacrosExtensionAppsPublisher for the class responsible for sending events
// to Ash.
class LacrosExtensionAppsController : public crosapi::mojom::AppController {
 public:
  static std::unique_ptr<LacrosExtensionAppsController> MakeForChromeApps();
  static std::unique_ptr<LacrosExtensionAppsController> MakeForExtensions();

  // Should not be directly called. Normally this should be private, but then
  // this would require friending std::make_unique.
  explicit LacrosExtensionAppsController(
      const ForWhichExtensionType& which_type);

  ~LacrosExtensionAppsController() override;

  LacrosExtensionAppsController(const LacrosExtensionAppsController&) = delete;
  LacrosExtensionAppsController& operator=(
      const LacrosExtensionAppsController&) = delete;

  // This class does not receive events from ash until Initialize is called.
  // Tests may construct this class without using Initialize if the tests
  // directly call the AppController methods.
  void Initialize(mojo::Remote<crosapi::mojom::AppPublisher>& publisher);

  void SetPublisher(LacrosExtensionAppsPublisher* publisher);

  // crosapi::mojom::AppController
  // Public for testing.
  void Uninstall(const std::string& app_id,
                 apps::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    GetMenuModelCallback callback) override;
  void DEPRECATED_LoadIcon(const std::string& app_id,
                           apps::IconKeyPtr icon_key,
                           apps::IconType icon_type,
                           int32_t size_hint_in_dip,
                           apps::LoadIconCallback callback) override;
  void GetCompressedIcon(const std::string& app_id,
                         int32_t size_in_dip,
                         ui::ResourceScaleFactor scale_factor,
                         apps::LoadIconCallback callback) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void UpdateAppSize(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::WindowMode window_mode) override;
  void Launch(crosapi::mojom::LaunchParamsPtr launch_params,
              LaunchCallback callback) override;
  void ExecuteContextMenuCommand(
      const std::string& app_id,
      const std::string& id,
      ExecuteContextMenuCommandCallback callback) override;
  void StopApp(const std::string& app_id) override;
  void SetPermission(const std::string& app_id,
                     apps::PermissionPtr permission) override;

 private:
  // Called when the enable flow has finished. |success| indicates whether the
  // extension was enabled.
  void FinishedEnableFlow(crosapi::mojom::LaunchParamsPtr launch_params,
                          LaunchCallback callback,
                          crosapi::mojom::LaunchResultPtr result,
                          void* key,
                          bool success);

  // Continues Launch() using the same {|launch_param|, |callback|} and passed
  // |result| after prerequisites have been met.
  void FinallyLaunch(crosapi::mojom::LaunchParamsPtr launch_params,
                     LaunchCallback callback,
                     crosapi::mojom::LaunchResultPtr result);

  // Callback from extensions::ExecuteFileBrowserHandlerFlow().
  void OnExecuteFileBrowserHandlerComplete(
      crosapi::mojom::LaunchResultPtr result,
      LaunchCallback callback,
      bool success);

  // This is called after maybe presenting a file dialog permission UI to ensure
  // that the extension is confirmed to be able to open the relevant file type.
  //
  // Arguments is a single word that represents either intents or params.
  // LacrosExtensionAppsController::LaunchAppWithIntentCallback() uses pararms.
  // ExtensionAppsChromeOs::LaunchAppWithIntentCallback() uses intents.
  void LaunchAppWithArgumentsCallback(Profile* profile,
                                      apps::AppLaunchParams params,
                                      LaunchCallback callback,
                                      crosapi::mojom::LaunchResultPtr result,
                                      bool should_open);

  // State to decide which extension type (e.g., Chrome Apps vs. Extensions)
  // to support.
  const ForWhichExtensionType which_type_;

  // Tracks instances of ExtensionAppsEnableFlow. This class constructs one
  // instance of ExtensionAppsEnableFlow for each attempt to launch a disabled
  // application. There is no deduplication by app_id -- that means that if ash
  // attempts to launch the same disabled app multiple times, there will be
  // multiple instances of ExtensionAppsEnableFlow, and eventually each one will
  // result in a callback to FinishedEnableFlow.
  // The key is the raw pointer to the ExtensionAppsEnableFlow.
  std::map<void*, std::unique_ptr<apps::ExtensionAppsEnableFlow>> enable_flows_;

  raw_ptr<LacrosExtensionAppsPublisher> publisher_ = nullptr;  // Not owned.

  // Mojo endpoint that's responsible for receiving messages from Ash.
  mojo::Receiver<crosapi::mojom::AppController> controller_;

  std::unique_ptr<extensions::WebFileHandlersPermissionHandler>
      web_file_handlers_permission_handler_;

  base::WeakPtrFactory<LacrosExtensionAppsController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_CONTROLLER_H_
