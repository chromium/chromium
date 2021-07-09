// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_APP_LAUNCH_HANDLER_H_

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/resourced/resourced_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {
class AppUpdate;
}  // namespace apps

namespace chromeos {
namespace full_restore {

class ArcAppLaunchHandlerArcAppBrowserTest;
class ArcWindowHandler;
class FullRestoreAppLaunchHandler;

struct CpuTick {
  uint64_t idle_time = 0;
  uint64_t used_time = 0;
  CpuTick operator-(const CpuTick& rhs) const {
    return {idle_time - rhs.idle_time, used_time - rhs.used_time};
  }
};

// The ArcAppLaunchHandler class restores ARC apps during the system startup
// phase.
//
// TODO(crbug.com/1146900):
// 1. Add memory pressure checking before launch ARC apps.
// 2. Add app launch policy.
// 3. Check whether the ARC app is ready before launch the ARC apps.
class ArcAppLaunchHandler : public apps::AppRegistryCache::Observer,
                            public chromeos::ResourcedClient::Observer {
 public:
  struct WindowInfo {
    std::string app_id;
    int32_t window_id;
  };

  ArcAppLaunchHandler();
  ArcAppLaunchHandler(const ArcAppLaunchHandler&) = delete;
  ArcAppLaunchHandler& operator=(const ArcAppLaunchHandler&) = delete;
  ~ArcAppLaunchHandler() override;

  // Invoked when the restoration process can start. Reads the restore data, and
  // add the ARC apps windows to `windows_` and `no_stack_windows_`.
  void RestoreArcApps(FullRestoreAppLaunchHandler* app_launch_handler);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void OnAppConnectionReady();

  void LaunchApp(const std::string& app_id);

 private:
  friend ArcAppLaunchHandlerArcAppBrowserTest;

  // Reads the restore data, and add the ARC app windows to `windows_`,
  // `no_stack_windows_` and `app_ids_`.
  void LoadRestoreData();

  // Creates the ghost windows or displays the icon with an overlaid spinner to
  // provide visual feedback that the app cannot be launched immediately (due to
  // ARC not being ready, or the system perforamcne concern) on Chrome OS.
  void PrepareAppLaunching(const std::string& app_id);

  // Override chromeos::ResourcedClient::Observer
  void OnMemoryPressure(chromeos::ResourcedClient::PressureLevel level,
                        uint64_t reclaim_target_kb) override;

  // Returns true if there are windows to be restored. Otherwise, returns false.
  bool HasRestoreData();

  // Returns true if the app can be launched. Otherwise, returns false.
  bool CanLaunchApp();

  // Returns true if the app is ready to be launched. Otherwise, returns false.
  bool IsAppReady(const std::string& app_id);

  void LaunchApp(const std::string& app_id, int32_t window_id);

  // Removes windows related with `app_id`.
  void RemoveWindowsForApp(const std::string& app_id);

  // Returns [0, 100] as percentage of device CPU usage rate.
  int GetCpuUsageRate();

  void StartCpuUsageCount();
  void StopCpuUsageCount();
  void UpdateCpuUsage();
  void OnCpuUsageUpdated(
      chromeos::cros_healthd::mojom::TelemetryInfoPtr info_ptr);
  void OnProbeServiceDisconnect();

  FullRestoreAppLaunchHandler* handler_ = nullptr;

  // The app id list to be restored. When the ARC app is ready in
  // AppRegistryCache, launch the ghost window or spin the icon and remove it
  // from `app_ids`.
  std::set<std::string> app_ids_;

  // The map from the window stack to the app id and the window id. This map is
  // used to save the windows to be restored.
  std::map<int32_t, WindowInfo> windows_;

  // ARC app windows without the window stack info. This list is used to save
  // the windows to be restored.
  std::list<WindowInfo> no_stack_windows_;

  std::map<int32_t, int32_t> window_id_to_session_id_;
  std::map<int32_t, int32_t> session_id_to_window_id_;

  ArcWindowHandler* window_handler_ = nullptr;

  chromeos::ResourcedClient::PressureLevel pressure_level_ =
      chromeos::ResourcedClient::PressureLevel::MODERATE;

  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> probe_service_;

  // Cpu usage rate count window. It save the cpu usage in a time interval.
  std::list<CpuTick> cpu_tick_window_;
  absl::optional<CpuTick> last_cpu_tick_;
  base::RepeatingTimer cpu_tick_count_timer_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<chromeos::ResourcedClient,
                          chromeos::ResourcedClient::Observer>
      resourced_client_observer_{this};

  base::WeakPtrFactory<ArcAppLaunchHandler> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_APP_LAUNCH_HANDLER_H_
