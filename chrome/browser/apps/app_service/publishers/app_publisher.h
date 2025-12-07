// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publisher.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace apps {

class PackageId;

#if BUILDFLAG(IS_CHROMEOS)
struct PromiseApp;
using PromiseAppPtr = std::unique_ptr<PromiseApp>;
#endif

// AppPublisher parent class (in the App Service sense) for all app publishers.
// See components/services/app_service/README.md.
// TODO(crbug.com/441649482): The name looks confusing. Rename to
// PublisherBase.
class AppPublisher : public Publisher {
 public:
  explicit AppPublisher(AppServiceProxy* proxy);
  AppPublisher(const AppPublisher&) = delete;
  AppPublisher& operator=(const AppPublisher&) = delete;
  ~AppPublisher() override;

  // Returns an app object from the provided parameters
  static AppPtr MakeApp(AppType app_type,
                        const std::string& app_id,
                        Readiness readiness,
                        const std::string& name,
                        InstallReason install_reason,
                        InstallSource install_source);

  // Registers this AppPublisher to AppServiceProxy, allowing it to receive App
  // Service API calls. This function must be called after the object's
  // creation, and can't be called in the constructor function to avoid
  // receiving API calls before being fully constructed and ready. This should
  // be called immediately before the first call to AppPublisher::Publish that
  // sends the initial list of apps to the App Service.
  void RegisterPublisher(AppType app_type);

  // Publisher overrides
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void StopApp(const std::string& app_id) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override {}
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void SetPermission(const std::string& app_id,
                     PermissionPtr permission) override;
  void UpdateAppSize(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     WindowMode window_mode) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void BlockApp(const std::string& app_id) override;
  void UnblockApp(const std::string& app_id) override;
  void SetResizeLocked(const std::string& app_id, bool locked) override;

#if BUILDFLAG(IS_CHROMEOS)
  int DefaultIconResourceId() const override;
  void SetAppLocale(const std::string& app_id,
                    const std::string& locale_tag) override;

  // CompressedIconGetter override.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;

  // Creates and returns a promise app object.
  static PromiseAppPtr MakePromiseApp(const PackageId& package_id);

  // Publishes a single promise app delta to the Promise App Registry Cache.
  void PublishPromiseApp(PromiseAppPtr delta);
#endif

 protected:
  // Publish one `app` to AppServiceProxy. Should be called whenever the app
  // represented by `app` undergoes some state change to inform AppServiceProxy
  // of the change. Ensure that RegisterPublisher() has been called before the
  // first call to this method.
  void Publish(AppPtr app);

  // Publish multiple `apps` to AppServiceProxy. Should be called whenever the
  // apps represented by `apps` undergoes some state change to inform
  // AppServiceProxy of the change. Ensure that RegisterPublisher() has been
  // called before the first call to this method.
  //
  // `should_notify_initialized` is true, when the publisher for `app_type` has
  // finished initiating apps - typically this is the very first time Publish()
  // is called with the initial set of apps present at the time the publisher is
  // first created. Otherwise `should_notify_initialized` is false. When
  // `should_notify_initialized` is true, `app_type` should not be `kUnknown`.
  void Publish(std::vector<AppPtr> apps,
               AppType app_type,
               bool should_notify_initialized);

  // Modifies CapabilityAccess for `app_id`.
  void ModifyCapabilityAccess(const std::string& app_id,
                              std::optional<bool> accessing_camera,
                              std::optional<bool> accessing_microphone);

  // Resets all tracked capabilities for apps of type `app_type`. Should be
  // called when the publisher stops running apps (e.g. when a VM shuts down).
  void ResetCapabilityAccess(AppType app_type);

  AppServiceProxy* proxy() { return proxy_; }

 private:
  const raw_ptr<AppServiceProxy, DanglingUntriaged> proxy_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_
