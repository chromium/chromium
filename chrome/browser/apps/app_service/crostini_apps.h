// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefChangeRegistrar;
class Profile;

namespace apps {

// An app publisher (in the App Service sense) of Crostini apps,
//
// See components/services/app_service/README.md.
class CrostiniApps : public KeyedService,
                     public apps::PublisherBase,
                     public guest_os::GuestOsRegistryService::Observer {
 public:
  CrostiniApps(const mojo::Remote<apps::mojom::AppService>& app_service,
               Profile* profile);
  ~CrostiniApps() override;

  void ReInitializeForTesting(
      const mojo::Remote<apps::mojom::AppService>& app_service,
      Profile* profile);

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // apps::mojom::Publisher overrides.
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
              int64_t display_id) override;
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

  // Registers and unregisters terminal with AppService.
  // TODO(crbug.com/1028898): Move this code into System Apps
  // once it can support hiding apps.
  void OnCrostiniEnabledChanged();

  apps::mojom::AppPtr Convert(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool new_icon_key);
  apps::mojom::IconKeyPtr NewIconKey(const std::string& app_id);

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  guest_os::GuestOsRegistryService* registry_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  bool crostini_enabled_;

  base::WeakPtrFactory<CrostiniApps> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_
