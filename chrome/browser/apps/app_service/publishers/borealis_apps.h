// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BOREALIS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BOREALIS_APPS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"

namespace apps {

class PublisherHost;

// An app publisher (in the App Service sense) of Borealis apps.
// See components/services/app_service/README.md.
class BorealisApps
    : public GuestOSApps,
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

  // Called by the pref registry when one of borealis' global permissions (mic,
  // camera, etc) change.
  void OnPermissionChanged();

  // Re-create borealis' "special apps", called when one of the preferences
  // which control these changes (i.e for install and uninstall).
  void RefreshSpecialApps();

  // GuestOsApps overrides.
  bool CouldBeAllowed() const override;
  apps::AppType AppType() const override;
  guest_os::VmType VmType() const override;
  void Initialize() override;
  void CreateAppOverrides(
      const guest_os::GuestOsRegistryService::Registration& registration,
      App* app) override;

  // apps::AppPublisher overrides.
  int DefaultIconResourceId() const override;
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

  // borealis::BorealisWindowManager::AnonymousAppObserver overrides.
  void OnAnonymousAppAdded(const std::string& shelf_app_id,
                           const std::string& shelf_app_name) override;
  void OnAnonymousAppRemoved(const std::string& shelf_app_id) override;
  void OnWindowManagerDeleted(
      borealis::BorealisWindowManager* window_manager) override;

  base::ScopedObservation<borealis::BorealisWindowManager,
                          borealis::BorealisWindowManager::AnonymousAppObserver>
      anonymous_app_observation_{this};

  PrefChangeRegistrar pref_registrar_;

  base::WeakPtrFactory<BorealisApps> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BOREALIS_APPS_H_
