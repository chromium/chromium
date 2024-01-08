// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_SERVICE_WRAPPER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_SERVICE_WRAPPER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

class Profile;

namespace base {
class UnguessableToken;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {
namespace app_time {

class AppId;
struct PauseAppInfo;

// Wrapper around AppService.
// Provides abstraction layer for Per-App Time Limits (PATL). Takes care of
// types conversions and data filetering, so those operations are not spread
// around the PATL code.
class AppServiceWrapper : public apps::AppRegistryCache::Observer,
                          public apps::InstanceRegistry::Observer {
 public:
  // Notifies listeners about app state changes.
  // Listener only get updates about apps that are relevant for PATL feature.
  class EventListener : public base::CheckedObserver {
   public:
    // Called when app with |app_id| is installed and at the beginning of each
    // user session (because AppService does not store apps information between
    // sessions).
    virtual void OnAppInstalled(const AppId& app_id) {}

    // Called when app with |app_id| is uninstalled.
    virtual void OnAppUninstalled(const AppId& app_id) {}

    // Called when app with |app_id| become available for usage. Usually when
    // app is unblocked.
    virtual void OnAppAvailable(const AppId& app_id) {}

    // Called when app with |app_id| become disabled and cannot be used.
    virtual void OnAppBlocked(const AppId& app_id) {}

    // Called when app with |app_id| becomes active.
    // Active means that the app is in usage (visible in foreground).
    // If the app is launched multiple times, |instance_id| indicates which
    // of the instances is active.
    // |timestamp| indicates the time when the app became active.
    virtual void OnAppActive(const AppId& app_id,
                             const base::UnguessableToken& instance_id,
                             base::Time timestamp) {}

    // Called when app with |app_id| becomes inactive.
    // Inactive means that the app is not in the foreground. It still can run
    // and be partially visible. |timestamp| indicates the time when the app
    // became inactive. |instance_id| to specify which of the application's
    // potentially multiple instances became inactive. Note: This can be called
    // for the app that is already inactive.
    virtual void OnAppInactive(const AppId& app_id,
                               const base::UnguessableToken& instance_id,
                               base::Time timestamp) {}

    // Called when app with |app_id| is destroyed.
    // |timestamp| indicates the time when the app is destroyed.
    // |instance_id| to specify which of the application's potentially multiple
    // instances was destroyed.
    virtual void OnAppDestroyed(const AppId& app_id,
                                const base::UnguessableToken& instance_id,
                                base::Time timestamp) {}
  };

  explicit AppServiceWrapper(Profile* profile);
  AppServiceWrapper(const AppServiceWrapper&) = delete;
  AppServiceWrapper& operator=(const AppServiceWrapper&) = delete;
  ~AppServiceWrapper() override;

  // Pauses the app identified by |PauseAppInfo::app_id|.
  // Uses |PauseAppInfo::daily_limit| to communicate applied time restriction to
  // the user by showing the dialog. After this is called user will not be able
  // to launch the app and the visual effect will be applied to the icon.
  // |PauseAppInfo::show_pause_dialog| indicates whether the user should be
  // notified with a dialog.
  void PauseApp(const PauseAppInfo& pause_app);
  void PauseApps(const std::vector<PauseAppInfo>& paused_apps);

  // Resets time restriction from the app identified with |app_id|. After this
  // is called user will be able to use the app again and the visual effect
  // will be removed from the icon.
  void ResumeApp(const AppId& app_id);

  // Launches app identified by |app_service_id| with no event flags explicitly
  // and default display id.
  void LaunchApp(const std::string& app_service_id);

  // Returns installed apps that are relevant for Per-App Time Limits feature.
  // Installed apps of unsupported types will not be included.
  std::vector<AppId> GetInstalledApps() const;

  // Returns true if the application is a hidden arc++ app.
  bool IsHiddenArcApp(const AppId& app_id) const;

  // Returns the list of arc++ apps hidden from user.
  std::vector<AppId> GetHiddenArcApps() const;

  // Returns short name of the app identified by |app_id|.
  // Might return empty string.
  std::string GetAppName(const AppId& app_id) const;

  // Returns the uncompressed image icon for app identified by |app_id| with
  // size |size_hint_in_dp|.
  void GetAppIcon(const AppId& app_id,
                  int size_hint_in_dp,
                  base::OnceCallback<void(std::optional<gfx::ImageSkia>)>
                      on_icon_ready) const;

  // Returns app service id for the app identified by |app_id|.
  // App service id will be only different from app_id.app_id() for ARC++ apps.
  // It does not make sense to call it for other apps.
  std::string GetAppServiceId(const AppId& app_id) const;

  // Return true if the App with |app_service_id| is installed.
  bool IsAppInstalled(const std::string& app_service_id);

  // Returns AppId from |app_service_id| and |app_type|.
  AppId AppIdFromAppServiceId(const std::string& app_service_id,
                              apps::AppType app_type) const;

  void AddObserver(EventListener* observer);
  void RemoveObserver(EventListener* observer);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  apps::InstanceRegistry& GetInstanceRegistry() const;

 private:
  apps::AppServiceProxy* GetAppProxy() const;
  apps::AppRegistryCache& GetAppCache() const;

  // Return whether app with |app_id| should be included for per-app time
  // limits.
  bool ShouldIncludeApp(const AppId& app_id) const;

  base::ObserverList<EventListener> listeners_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  const raw_ptr<Profile> profile_;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_SERVICE_WRAPPER_H_
