// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_

#include <memory>
#include <string>

#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
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
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace apps {

class PublisherHost;

struct AppLaunchParams;

// An app publisher (in the App Service sense) of Plugin VM apps.
//
// See components/services/app_service/README.md.
//
// TODO(crbug.com/1253250):
// 1. Remove the parent class apps::PublisherBase.
// 2. Remove all apps::mojom related code.
class PluginVmApps : public apps::PublisherBase,
                     public AppPublisher,
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
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override;
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

  // apps::PublisherBase overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;

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

  apps::mojom::AppPtr Convert(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool new_icon_key);
  void OnPluginVmAllowedChanged(bool is_allowed);
  void OnPluginVmConfiguredChanged();
  void OnPermissionChanged();

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;
  guest_os::GuestOsRegistryService* registry_ = nullptr;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  // Whether the Plugin VM app is allowed by policy.
  bool is_allowed_ = false;

  std::unique_ptr<plugin_vm::PluginVmPolicySubscription> policy_subscription_;
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_
