// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_

#include <memory>
#include <string>

#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace apps {

// An app publisher (in the App Service sense) of Plugin VM apps.
//
// See components/services/app_service/README.md.
class PluginVmApps : public apps::PublisherBase,
                     public guest_os::GuestOsRegistryService::Observer {
 public:
  PluginVmApps(const mojo::Remote<apps::mojom::AppService>& app_service,
               Profile* profile);
  ~PluginVmApps() override;

  PluginVmApps(const PluginVmApps&) = delete;
  PluginVmApps& operator=(const PluginVmApps&) = delete;

 private:
  // apps::PublisherBase overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;

  // GuestOsRegistryService::Observer overrides.
  void OnRegistryUpdated(
      guest_os::GuestOsRegistryService* registry_service,
      guest_os::GuestOsRegistryService::VmType vm_type,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override;

  apps::mojom::AppPtr Convert(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool new_icon_key);
  void OnPluginVmAllowedChanged(bool is_allowed);
  void OnPluginVmConfiguredChanged();

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;
  guest_os::GuestOsRegistryService* registry_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  // Whether the Plugin VM app is allowed by policy.
  bool is_allowed_;

  std::unique_ptr<plugin_vm::PluginVmPolicySubscription> policy_subscription_;
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PLUGIN_VM_APPS_H_
