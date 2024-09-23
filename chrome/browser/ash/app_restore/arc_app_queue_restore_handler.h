// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_APP_QUEUE_RESTORE_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_APP_QUEUE_RESTORE_HANDLER_H_

#include <list>
#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/scheduler_config/scheduler_configuration_manager.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace apps {
class AppUpdate;
}  // namespace apps

namespace ash {

class AppLaunchHandler;

namespace full_restore {
class ArcAppQueueRestoreHandlerArcAppBrowserTest;
class ArcGhostWindowHandler;
class FullRestoreAppLaunchHandlerArcAppBrowserTest;
}  // namespace full_restore

namespace app_restore {

struct CpuTick {
  uint64_t idle_time = 0;
  uint64_t used_time = 0;
  CpuTick operator-(const CpuTick& rhs) const {
    return {idle_time - rhs.idle_time, used_time - rhs.used_time};
  }
};

// This is used for logging, so do not remove or reorder existing entries.
enum class RestoreResult {
  kFinish = 0,
  kNotFinish = 1,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kNotFinish,
};

// This is used for logging, so do not remove or reorder existing entries.
enum class ArcRestoreState {
  kSuccess = 0,
  kSuccessWithMemoryPressure = 1,
  kSuccessWithCPUUsageRateLimiting = 2,
  kSuccessWithMemoryPressureAndCPUUsageRateLimiting = 3,
  kFailedWithMemoryPressure = 4,
  kFailedWithCPUUsageRateLimiting = 5,
  kFailedWithMemoryPressureAndCPUUsageRateLimiting = 6,
  kFailedWithUnknown = 7,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kFailedWithUnknown,
};

constexpr char kRestoredAppWindowCountHistogram[] =
    "Apps.RestoreArcWindowCount";

// The restoration process might be blocked by some issues, e.g. the memory
// pressure, CPU rate, etc. However we don't want to have the restoration
// process taking too long to interact the normal usage. So if the restoration
// has finished in `kAppLaunchDelay` timeframe, we stop the restoration process.
constexpr base::TimeDelta kStopRestoreDelay = base::Minutes(1);

// The ArcAppQueueRestoreHandler class restores ARC apps during the system
// startup phase.
class ArcAppQueueRestoreHandler
    : public apps::AppRegistryCache::Observer,
      public ResourcedClient::Observer,
      public wm::ActivationChangeObserver,
      public aura::EnvObserver,
      public aura::WindowObserver,
      public SchedulerConfigurationManagerBase::Observer {
 public:
  struct WindowInfo {
    std::string app_id;
    int32_t window_id;

    bool operator==(const WindowInfo& rhs) const {
      return app_id == rhs.app_id && window_id == rhs.window_id;
    }
  };

  ArcAppQueueRestoreHandler();
  ArcAppQueueRestoreHandler(const ArcAppQueueRestoreHandler&) = delete;
  ArcAppQueueRestoreHandler& operator=(const ArcAppQueueRestoreHandler&) =
      delete;
  ~ArcAppQueueRestoreHandler() override;

  // Invoked when the restoration process can start. Reads the restore data, and
  // add the ARC apps windows to `windows_` and `no_stack_windows_`. For each
  // AppLaunchHandler, it is only expected be called once.
  void RestoreArcApps(AppLaunchHandler* app_launch_handler);

  void OnAppConnectionReady();

  // Invoked when ChromeShelfController is created.
  void OnShelfReady();

  void OnArcPlayStoreEnabledChanged(bool enabled);

  // Launch all windows for the given `app_id`.
  void LaunchApp(const std::string& app_id);

  bool IsAppPendingRestore(const std::string& app_id) const;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* new_active,
                         aura::Window* old_active) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // SchedulerConfigurationManagerBase::Observer:
  void OnConfigurationSet(bool success, size_t num_cores_disabled) override;

  void set_desk_template_launch_id(int32_t desk_template_launch_id) {
    desk_template_launch_id_ = desk_template_launch_id;
  }

 private:
  friend class full_restore::ArcAppQueueRestoreHandlerArcAppBrowserTest;
  friend class full_restore::FullRestoreAppLaunchHandlerArcAppBrowserTest;

  // Reads the restore data, and add the ARC app windows to `windows_`,
  // `no_stack_windows_` and `app_ids_`.
  void LoadRestoreData();

  // Adds the restore windows to `windows_` or `no_stack_windows_`.
  void AddWindows(const std::string& app_id);

  // Creates ghost windows or displays spin icons for all ARC apps to be
  // restored.
  void PrepareLaunchApps();

  // Creates the ghost windows or displays the icon with an overlaid spinner to
  // provide visual feedback that the app cannot be launched immediately (due to
  // ARC not being ready, or the system perforamcne concern) on Chrome OS.
  void PrepareAppLaunching(const std::string& app_id);

  // Override ResourcedClient::Observer
  void OnMemoryPressure(ResourcedClient::PressureLevel level,
                        memory_pressure::ReclaimTarget target) override;

  // Returns true if there are windows to be restored. Otherwise, returns false.
  bool HasRestoreData();

  // Returns true if the app can be launched. Otherwise, returns false.
  bool CanLaunchApp();
  // Returns true if the memory under the pressure. Otherwise, returns false.
  bool IsUnderMemoryPressure();
  // Returns true if the CPU usage over the resourece limiting. Otherwise,
  // returns false.
  bool IsUnderCPUUsageLimiting();

  // Returns true if the app is ready to be launched. Otherwise, returns false.
  bool IsAppReady(const std::string& app_id);

  // Checks the app launching condition. If we can launch an app, launch the app
  // following the window stack priority.
  void MaybeLaunchApp();

  void LaunchAppWindow(const std::string& app_id, int32_t window_id);

  // Removes all windows records related with `app_id` from `windows_`,
  // `no_stack_windows_`, and `pending_windows_`.
  void RemoveWindowsForApp(const std::string& app_id);

  // Removes the window record related with `app_id` and `window_id` from
  // `windows_`, `no_stack_windows_`, or `pending_windows_`.
  void RemoveWindow(const std::string& app_id, int32_t window_id);

  void MaybeReStartTimer(const base::TimeDelta& delay);

  void StopRestore();

  // Returns [0, 100] as percentage of device CPU usage rate.
  int GetCpuUsageRate();

  void StartCpuUsageCount();
  void StopCpuUsageCount();
  void UpdateCpuUsage();
  void OnCpuUsageUpdated(cros_healthd::mojom::TelemetryInfoPtr info_ptr);
  void OnProbeServiceDisconnect();

  void RecordArcGhostWindowLaunch(bool is_arc_ghost_window);
  void RecordRestoreResult();

  SchedulerConfigurationManager* GetSchedulerConfigurationManager();

  raw_ptr<AppLaunchHandler, DanglingUntriaged> handler_ = nullptr;

  // The app id list from the restore data. If the app has been added the
  // AppRegistryCache, the app will be removed from `app_ids_` to
  // prevent restoring the app multiple times.
  std::set<std::string> app_ids_;

  // The map from the window stack to the app id and the window id. This map is
  // used to save the windows to be restored.
  std::map<int32_t, WindowInfo> windows_;

  // ARC app windows without the window stack info. This list is used to save
  // the windows to be restored.
  std::list<WindowInfo> no_stack_windows_;

  // If the app launching condition doesn't match, e.g. the app is not ready,
  // and after checking `kMaxCheckingNum` times, there is no improvement, the
  // window is moved to `pending_windows_` to be launched later.
  std::list<WindowInfo> pending_windows_;

  std::map<int32_t, int32_t> window_id_to_session_id_;
  std::map<int32_t, int32_t> session_id_to_window_id_;

  raw_ptr<full_restore::ArcGhostWindowHandler> window_handler_ = nullptr;

  // If the system is under memory pressuure or high CPU usage rate, only launch
  // 1 window following the window stack priority. `first_run_` is used to check
  // whether this is the first window to be restored, which can skip the system
  // memory and CPU usage rate checking.
  bool first_run_ = true;

  // The number to record how many times the current top window has been
  // launched.
  int launch_count_ = 0;

  // If ChromeShelfController has not been created, we can't create the spin
  // icon on shelf. `is_shelf_ready_` is used to mark whether
  // ChromeShelfController is created to prepare launching apps.
  bool is_shelf_ready_ = false;

  bool is_app_connection_ready_ = false;

  // If nonzero, identifies the desk template launch that this handler is used
  // for.
  int32_t desk_template_launch_id_ = 0;

  // A repeating timer to check whether we can restore the ARC apps.
  std::unique_ptr<base::RepeatingTimer> app_launch_timer_;

  // A one shot timer to stop the restoration process.
  std::unique_ptr<base::OneShotTimer> stop_restore_timer_;

  // The timer delay.
  base::TimeDelta current_delay_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  ResourcedClient::PressureLevel pressure_level_ =
      ResourcedClient::PressureLevel::MODERATE;

  std::optional<bool> should_apply_cpu_restirction_;

  // Record if the restore process faced memory pressure or CPU usage limiting.
  bool was_memory_pressured_ = false;
  bool was_cpu_usage_limited_ = false;

  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> probe_service_;

  // Cpu usage rate count window. It save the cpu usage in a time interval.
  std::list<CpuTick> cpu_tick_window_;
  std::optional<CpuTick> last_cpu_tick_;
  base::RepeatingTimer cpu_tick_count_timer_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<ResourcedClient, ResourcedClient::Observer>
      resourced_client_observer_{this};

  base::WeakPtrFactory<ArcAppQueueRestoreHandler> weak_ptr_factory_{this};
};

}  // namespace app_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_APP_QUEUE_RESTORE_HANDLER_H_
