// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BOREALIS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BOREALIS_APPS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace apps {

class PublisherHost;

struct AppLaunchParams;

// An app publisher (in the App Service sense) of Borealis apps.
// See components/services/app_service/README.md.
//
// TODO(crbug.com/1253250):
// 1. Remove the parent class apps::PublisherBase.
// 2. Remove all apps::mojom related code.
class BorealisApps
    : public apps::PublisherBase,
      public AppPublisher,
      public guest_os::GuestOsRegistryService::Observer,
      public borealis::BorealisWindowManager::AnonymousAppObserver {
 public:
  explicit BorealisApps(AppServiceProxy* proxy);
  ~BorealisApps() override;

  // Disallow copy and assign.
  BorealisApps(const BorealisApps&) = delete;
  BorealisApps& operator=(const BorealisApps&) = delete;

 private:
  friend class PublisherHost;

  // Helper method for dispatching to the provided |callback| once we
  // have queried whether borealis is allowed not installed.
  void CallWithBorealisAllowed(base::OnceCallback<void(bool)> callback);

  // Called after determining whether borealis is |allowed| and |enabled|, this
  // method sets up the "special" (i.e. non-vm, non-anonymous) apps used by
  // borealis, such as its installer.
  void SetUpSpecialApps(bool allowed);
  void SetUpSpecialAppsMojom(bool allowed);

  // Helper method to get the registry used by this profile
  guest_os::GuestOsRegistryService* Registry();

  // Turns GuestOsRegistry's "app" into one the AppService can use.
  AppPtr CreateApp(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool generate_new_icon_key);

  // Turns GuestOsRegistry's "app" into one the AppService can use.
  apps::mojom::AppPtr Convert(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool new_icon_key);

  void Initialize();

  // Called by the pref registry when one of borealis' global permissions (mic,
  // camera, etc) change.
  void OnPermissionChanged();

  // Re-create borealis' "special apps", called when one of the preferences
  // which control thse changes (i.e for insert_coin, install and uninstall).
  void RefreshSpecialApps();

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

  // borealis::BorealisWindowManager::AnonymousAppObserver overrides.
  void OnAnonymousAppAdded(const std::string& shelf_app_id,
                           const std::string& shelf_app_name) override;
  void OnAnonymousAppRemoved(const std::string& shelf_app_id) override;
  void OnWindowManagerDeleted(
      borealis::BorealisWindowManager* window_manager) override;

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  Profile* const profile_;

  base::ScopedObservation<borealis::BorealisWindowManager,
                          borealis::BorealisWindowManager::AnonymousAppObserver>
      anonymous_app_observation_{this};

  PrefChangeRegistrar pref_registrar_;

  base::WeakPtrFactory<BorealisApps> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BOREALIS_APPS_H_
