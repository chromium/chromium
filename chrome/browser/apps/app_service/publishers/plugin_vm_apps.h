// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"

class Profile;

namespace apps {

class PublisherHost;

struct AppLaunchParams;

// An app publisher (in the App Service sense) of Plugin VM apps.
//
// See components/services/app_service/README.md.
class PluginVmApps : public AppPublisher,
                     public guest_os::GuestOsRegistryService::Observer {
 public:
  explicit PluginVmApps(AppServiceProxy* proxy);
  ~PluginVmApps() override;

  PluginVmApps(const PluginVmApps&) = delete;
  PluginVmApps& operator=(const PluginVmApps&) = delete;

 private:
  friend class PublisherHost;

  void Initialize();

  // apps::AppPublisher overrides.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void SetPermission(const std::string& app_id,
                     PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;

  // GuestOsRegistryService::Observer overrides.
  void OnRegistryUpdated(
      guest_os::GuestOsRegistryService* registry_service,
      guest_os::VmType vm_type,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override;

  AppPtr CreateApp(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool generate_new_icon_key);

  void OnPluginVmAvailabilityChanged(bool is_allowed, bool is_configured);
  void OnPermissionChanged();

  const raw_ptr<Profile> profile_;
  raw_ptr<guest_os::GuestOsRegistryService> registry_ = nullptr;

  // Whether the Plugin VM app is allowed by policy.
  bool is_allowed_ = false;

  std::unique_ptr<plugin_vm::PluginVmAvailabilitySubscription>
      availability_subscription_;
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_
