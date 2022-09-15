// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_app_launch_handler.h"

#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/shelf/arc_shelf_spinner_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/system/scheduler_configuration_manager_base.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/wm_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/display/display.h"
#include "ui/wm/public/activation_client.h"

namespace {

// If the app launching condition doesn't match, e.g. the app is not ready,
// and after checking `kMaxCheckingNum` times, there is no improvement, move to
// the next window to launch.
constexpr int kMaxCheckingNum = 3;

// Time interval between each checking for the app launching condition, e.g. the
// memory pressure level, or whether the app is ready.
constexpr base::TimeDelta kAppLaunchCheckingDelay = base::Seconds(1);

// Delay between each app launching.
constexpr base::TimeDelta kAppLaunchDelay = base::Seconds(3);

constexpr int kCpuUsageRefreshIntervalInSeconds = 1;

// Count CPU usage by average on last 6 seconds.
constexpr int kCpuUsageCountWindowLength =
    6 * kCpuUsageRefreshIntervalInSeconds;

// Restrict ARC app launch if CPU usage over threshold.
constexpr int kCpuUsageThreshold = 90;

// Apply CPU usage restrict if and only if the CPU cores not over
// |kCpuRestrictCoresCondition|.
constexpr int kCpuRestrictCoresCondition = 2;

constexpr char kRestoredArcAppResultHistogram[] = "Apps.RestoreArcAppsResult";

constexpr char kArcGhostWindowLaunchHistogram[] = "Apps.ArcGhostWindowLaunch";

constexpr char kRestoreArcAppStatesHistogram[] = "Apps.RestoreArcAppStates";

constexpr char kGhostWindowPopToArcHistogram[] = "Arc.LaunchedWithGhostWindow";

constexpr char kNoGhostWindowReasonHistogram[] =
    "Apps.RestoreNoGhostWindowReason";

}  // namespace

namespace ash::app_restore {

ArcAppLaunchHandler::ArcAppLaunchHandler() {
  if (aura::Env::HasInstance())
    env_observer_.Observe(aura::Env::GetInstance());

  if (ash::Shell::HasInstance() && ash::Shell::Get()->GetPrimaryRootWindow()) {
    auto* activation_client =
        wm::GetActivationClient(ash::Shell::Get()->GetPrimaryRootWindow());
    if (activation_client)
      activation_client->AddObserver(this);
  }

  auto* manager = GetSchedulerConfigurationManager();
  if (manager) {
    absl::optional<std::pair<bool, size_t>> scheduler_configuration =
        manager->GetLastReply();
    if (scheduler_configuration) {
      // Logical CPU core number should consider system HyperThread status.
      should_apply_cpu_restirction_ =
          (base::SysInfo::NumberOfProcessors() -
           scheduler_configuration->second) <= kCpuRestrictCoresCondition;
    } else {
      // If the configuration not exist, add observer to receive configuration
      // update.
      manager->AddObserver(this);
    }
  }
}

ArcAppLaunchHandler::~ArcAppLaunchHandler() {
  if (ash::Shell::HasInstance() && ash::Shell::Get()->GetPrimaryRootWindow()) {
    auto* activation_client =
        wm::GetActivationClient(ash::Shell::Get()->GetPrimaryRootWindow());
    if (activation_client)
      activation_client->RemoveObserver(this);
  }

  auto* manager = GetSchedulerConfigurationManager();
  if (manager)
    manager->RemoveObserver(this);
}

void ArcAppLaunchHandler::RestoreArcApps(AppLaunchHandler* app_launch_handler) {
  DCHECK(app_launch_handler);
  handler_ = app_launch_handler;

  if (!arc::IsArcPlayStoreEnabledForProfile(handler_->profile()))
    return;

  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      handler_->profile()));

  LoadRestoreData();
  if (app_ids_.empty()) {
    base::UmaHistogramCounts100(kRestoredAppWindowCountHistogram, 0);
    return;
  }

  window_handler_ = AppRestoreArcTaskHandler::GetForProfile(handler_->profile())
                        ->window_handler();

  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile())
          ->AppRegistryCache();

  // Observe AppRegistryCache to get the notification when the app is ready.
  if (!app_registry_cache_observer_.IsObserving())
    app_registry_cache_observer_.Observe(&cache);

  if (is_shelf_ready_)
    PrepareLaunchApps();

  if (is_app_connection_ready_)
    OnAppConnectionReady();
}

void ArcAppLaunchHandler::OnAppConnectionReady() {
  is_app_connection_ready_ = true;

  if (!HasRestoreData())
    return;

  base::UmaHistogramCounts100(kRestoredAppWindowCountHistogram,
                              windows_.size() + no_stack_windows_.size());

  // Receive the memory pressure level.
  if (ResourcedClient::Get() && !resourced_client_observer_.IsObserving()) {
    resourced_client_observer_.Observe(ResourcedClient::Get());
  }

  // Receive the system CPU usage rate.
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(
        base::BindOnce(&ArcAppLaunchHandler::OnProbeServiceDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  StartCpuUsageCount();

  if (!app_launch_timer_) {
    app_launch_timer_ = std::make_unique<base::RepeatingTimer>();
    MaybeReStartTimer(kAppLaunchCheckingDelay);
  }

  if (!stop_restore_timer_) {
    stop_restore_timer_ = std::make_unique<base::OneShotTimer>();
    stop_restore_timer_->Start(FROM_HERE, kStopRestoreDelay,
                               base::BindOnce(&ArcAppLaunchHandler::StopRestore,
                                              weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcAppLaunchHandler::OnShelfReady() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ArcAppLaunchHandler::PrepareLaunchApps,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppLaunchHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (enabled)
    return;

  StopRestore();

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (window_handler_) {
    std::set<int32_t> session_ids;
    for (const auto& it : session_id_to_window_id_)
      session_ids.insert(it.first);
    for (auto session_id : session_ids)
      window_handler_->CloseWindow(session_id);
  }
#endif

  app_ids_.clear();
  windows_.clear();
  no_stack_windows_.clear();
}

void ArcAppLaunchHandler::LaunchApp(const std::string& app_id) {
  if (!IsAppReady(app_id))
    return;

  DCHECK(handler_);
  const auto it =
      handler_->restore_data()->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data()->app_id_to_launch_list().end())
    return;

  if (it->second.empty()) {
    handler_->restore_data()->RemoveApp(app_id);
    return;
  }

  for (const auto& data_it : it->second)
    LaunchApp(app_id, data_it.first);

  RemoveWindowsForApp(app_id);
}

bool ArcAppLaunchHandler::IsAppPendingRestore(const std::string& app_id) const {
  return base::Contains(app_ids_, app_id);
}

void ArcAppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() || update.AppType() != apps::AppType::kArc) {
    return;
  }

  if (!apps_util::IsInstalled(update.Readiness())) {
    RemoveWindowsForApp(update.AppId());
    return;
  }

  // If the app is not ready, don't launch the app for the restoration.
  if (update.Readiness() != apps::Readiness::kReady)
    return;

  if (is_shelf_ready_ && base::Contains(app_ids_, update.AppId())) {
    AddWindows(update.AppId());
    PrepareAppLaunching(update.AppId());
  }
}

void ArcAppLaunchHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void ArcAppLaunchHandler::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* new_active,
    aura::Window* old_active) {
  const auto session_id = arc::GetWindowSessionId(new_active);
  if (!session_id.has_value())
    return;

  auto it = session_id_to_window_id_.find(session_id.value());
  if (it == session_id_to_window_id_.end())
    return;

  const std::string* arc_app_id =
      new_active->GetProperty(::app_restore::kAppIdKey);
  if (!arc_app_id || arc_app_id->empty() || !IsAppReady(*arc_app_id))
    return;

  RemoveWindow(*arc_app_id, it->second);
  LaunchApp(*arc_app_id, it->second);
}

void ArcAppLaunchHandler::OnWindowInitialized(aura::Window* window) {
  // An app window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget. Tooltips, menus, and other kinds of transient
  // windows that can't activate are filtered out.
  if (window->GetType() != aura::client::WINDOW_TYPE_NORMAL ||
      !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level() ||
      !arc::GetWindowSessionId(window).has_value()) {
    return;
  }

  observed_windows_.AddObservation(window);
}

void ArcAppLaunchHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  const auto session_id = arc::GetWindowSessionId(window);
  if (!session_id.has_value())
    return;

  auto it = session_id_to_window_id_.find(session_id.value());
  if (it == session_id_to_window_id_.end())
    return;

  auto window_id = it->second;
  session_id_to_window_id_.erase(session_id.value());

  const std::string* arc_app_id = window->GetProperty(::app_restore::kAppIdKey);
  if (!arc_app_id || arc_app_id->empty())
    return;

  RemoveWindow(*arc_app_id, window_id);
}

void ArcAppLaunchHandler::OnConfigurationSet(bool success,
                                             size_t num_cores_disabled) {
  // Logical CPU core number should consider system HyperThread status.
  should_apply_cpu_restirction_ =
      (base::SysInfo::NumberOfProcessors() - num_cores_disabled) <=
      kCpuRestrictCoresCondition;
  auto* manager = GetSchedulerConfigurationManager();
  if (manager)
    manager->RemoveObserver(this);
}

void ArcAppLaunchHandler::LoadRestoreData() {
  DCHECK(handler_);
  for (const auto& it : handler_->restore_data()->app_id_to_launch_list())
    app_ids_.insert(it.first);
}

void ArcAppLaunchHandler::AddWindows(const std::string& app_id) {
  DCHECK(handler_);
  auto it = handler_->restore_data()->app_id_to_launch_list().find(app_id);
  for (const auto& data_it : it->second) {
    if (data_it.second->activation_index.has_value()) {
      windows_[data_it.second->activation_index.value()] = {app_id,
                                                            data_it.first};
    } else {
      no_stack_windows_.push_back({app_id, data_it.first});
    }
  }
}

void ArcAppLaunchHandler::PrepareLaunchApps() {
  is_shelf_ready_ = true;

  if (app_ids_.empty())
    return;

  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile())
          ->AppRegistryCache();

  // Add the app to `app_ids` if there is a launch list from the restore data
  // for the app.
  std::set<std::string> app_ids;
  cache.ForEachApp([&app_ids, this](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::Readiness::kReady &&
        update.AppType() == apps::AppType::kArc &&
        base::Contains(app_ids_, update.AppId())) {
      app_ids.insert(update.AppId());
    }
  });

  for (const auto& app_id : app_ids) {
    AddWindows(app_id);
    PrepareAppLaunching(app_id);
  }
}

void ArcAppLaunchHandler::PrepareAppLaunching(const std::string& app_id) {
  DCHECK(handler_);
  app_ids_.erase(app_id);

  const auto it =
      handler_->restore_data()->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data()->app_id_to_launch_list().end())
    return;

  if (it->second.empty()) {
    handler_->restore_data()->RemoveApp(app_id);
    return;
  }

  for (const auto& data_it : it->second) {
    handler_->RecordRestoredAppLaunch(apps::AppTypeName::kArc);

    DCHECK(data_it.second->event_flag.has_value());

    // Set an ARC session id to find the restore window id based on the newly
    // created ARC task id. Note that the desk template launch ID must be set
    // first, if available.
    const int32_t arc_session_id = ::app_restore::CreateArcSessionId();
    if (desk_template_launch_id_ != 0) {
      ::app_restore::SetDeskTemplateLaunchIdForArcSessionId(
          arc_session_id, desk_template_launch_id_);
    }
    ::app_restore::SetArcSessionIdForWindowId(arc_session_id, data_it.first);
    window_id_to_session_id_[data_it.first] = arc_session_id;
    session_id_to_window_id_[arc_session_id] = data_it.first;

    bool launch_ghost_window = false;
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
    if (window_handler_ &&
        arc::CanLaunchGhostWindowByRestoreData(*data_it.second) &&
        window_handler_->LaunchArcGhostWindow(app_id, arc_session_id,
                                              data_it.second.get())) {
      launch_ghost_window = true;
    } else {
      // Only record bounds state when no ghost window launch.
      RecordLaunchBoundsState(data_it.second->bounds_in_root.has_value(),
                              data_it.second->current_bounds.has_value());
    }
#endif
    RecordArcGhostWindowLaunch(launch_ghost_window);

    const auto& file_path = handler_->profile()->GetPath();
    int32_t event_flags = data_it.second->event_flag.value();
    int64_t display_id = data_it.second->display_id.has_value()
                             ? data_it.second->display_id.value()
                             : display::kInvalidDisplayId;
    if (data_it.second->intent) {
      ::full_restore::SaveAppLaunchInfo(
          file_path, std::make_unique<::app_restore::AppLaunchInfo>(
                         app_id, event_flags, data_it.second->intent->Clone(),
                         arc_session_id, display_id));
    } else {
      ::full_restore::SaveAppLaunchInfo(
          file_path, std::make_unique<::app_restore::AppLaunchInfo>(
                         app_id, event_flags, arc_session_id, display_id));
    }

    if (launch_ghost_window)
      continue;

    ChromeShelfController* chrome_controller =
        ChromeShelfController::instance();
    // chrome_controller may be null in tests.
    if (chrome_controller) {
      apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();
      window_info->window_id = arc_session_id;
      chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
          app_id, std::make_unique<ArcShelfSpinnerItemController>(
                      app_id, data_it.second->event_flag.value(),
                      arc::UserInteractionType::APP_STARTED_FROM_FULL_RESTORE,
                      apps::MakeArcWindowInfo(std::move(window_info))));
    }
  }
}

void ArcAppLaunchHandler::OnMemoryPressure(ResourcedClient::PressureLevel level,
                                           uint64_t reclaim_target_kb) {
  pressure_level_ = level;
}

bool ArcAppLaunchHandler::HasRestoreData() {
  return !(windows_.empty() && no_stack_windows_.empty() &&
           pending_windows_.empty());
}

bool ArcAppLaunchHandler::CanLaunchApp() {
  // Checks CPU usage limiting and memory pressure, make sure it can
  // be recorded for UMA statistic data.
  bool is_under_cpu_usage_limiting = IsUnderCPUUsageLimiting();
  if (is_under_cpu_usage_limiting)
    was_cpu_usage_limited_ = true;
  bool is_under_memory_pressure = IsUnderMemoryPressure();
  if (is_under_memory_pressure)
    was_memory_pressured_ = true;

  return !is_under_cpu_usage_limiting && !is_under_memory_pressure;
}

bool ArcAppLaunchHandler::IsUnderMemoryPressure() {
  switch (pressure_level_) {
    case ResourcedClient::PressureLevel::NONE:
      return false;
    case ResourcedClient::PressureLevel::MODERATE:
    case ResourcedClient::PressureLevel::CRITICAL: {
      LOG(WARNING) << "Stop restoring Arc apps due to memory pressure: "
                   << (pressure_level_ ==
                               ResourcedClient::PressureLevel::MODERATE
                           ? "MODERATE"
                           : "CRITICAL");
      return true;
    }
  }
  return false;
}

bool ArcAppLaunchHandler::IsUnderCPUUsageLimiting() {
  // If the CPU HyperThread info has not updated from CrOS, enable cpu usage
  // limiting as default behavior.
  if (should_apply_cpu_restirction_.value_or(true)) {
    int cpu_usage_rate = GetCpuUsageRate();
    if (cpu_usage_rate >= kCpuUsageThreshold) {
      LOG(WARNING) << "CPU usage rate is too high to restore Arc apps: "
                   << cpu_usage_rate;
      return true;
    }
  }
  return false;
}

bool ArcAppLaunchHandler::IsAppReady(const std::string& app_id) {
  DCHECK(handler_);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(handler_->profile());
  if (!prefs)
    return false;

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info || app_info->suspended || !app_info->ready)
    return false;

  return true;
}

void ArcAppLaunchHandler::MaybeLaunchApp() {
  // Check CanLaunchApp() first for record the system states.
  if (!CanLaunchApp() && !first_run_) {
    MaybeReStartTimer(kAppLaunchCheckingDelay);
    return;
  }

  for (auto it = pending_windows_.begin(); it != pending_windows_.end(); ++it) {
    if (IsAppReady(it->app_id)) {
      LaunchApp(it->app_id, it->window_id);
      pending_windows_.erase(it);
      MaybeReStartTimer(kAppLaunchDelay);
      return;
    }
  }

  if (!windows_.empty()) {
    auto it = windows_.begin();
    if (IsAppReady(it->second.app_id)) {
      launch_count_ = 0;
      LaunchApp(it->second.app_id, it->second.window_id);
      windows_.erase(it);
      MaybeReStartTimer(kAppLaunchDelay);
    } else {
      ++launch_count_;
      if (launch_count_ >= kMaxCheckingNum) {
        pending_windows_.push_back({it->second.app_id, it->second.window_id});
        windows_.erase(it);
        launch_count_ = 0;
      } else if (launch_count_ == 1) {
        MaybeReStartTimer(kAppLaunchCheckingDelay);
      }
    }
    return;
  }

  for (auto it = no_stack_windows_.begin(); it != no_stack_windows_.end();
       ++it) {
    if (IsAppReady(it->app_id)) {
      LaunchApp(it->app_id, it->window_id);
      no_stack_windows_.erase(it);
      MaybeReStartTimer(kAppLaunchDelay);
      return;
    }
  }
}

void ArcAppLaunchHandler::LaunchApp(const std::string& app_id,
                                    int32_t window_id) {
  DCHECK(handler_);

  const auto it =
      handler_->restore_data()->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data()->app_id_to_launch_list().end())
    return;

  if (it->second.empty()) {
    handler_->restore_data()->RemoveApp(app_id);
    return;
  }

  const auto data_it = it->second.find(window_id);
  if (data_it == it->second.end())
    return;

  first_run_ = false;

  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile());
  DCHECK(proxy);

  DCHECK(data_it->second->event_flag.has_value());

  apps::WindowInfoPtr window_info =
      full_restore::HandleArcWindowInfo(data_it->second->GetAppWindowInfo());
  const auto window_it = window_id_to_session_id_.find(window_id);
  if (window_it != window_id_to_session_id_.end()) {
    window_info->window_id = window_it->second;
    window_id_to_session_id_.erase(window_it);
  } else {
    // Set an ARC session id to find the restore window id based on the newly
    // created ARC task id.
    const int32_t arc_session_id = ::app_restore::CreateArcSessionId();
    window_info->window_id = arc_session_id;
    ::app_restore::SetArcSessionIdForWindowId(arc_session_id, window_id);
    window_id_to_session_id_[window_id] = arc_session_id;
  }

  if (data_it->second->intent) {
    if (base::FeatureList::IsEnabled(apps::kAppServiceLaunchWithoutMojom)) {
      proxy->LaunchAppWithIntent(app_id, data_it->second->event_flag.value(),
                                 data_it->second->intent->Clone(),
                                 apps::LaunchSource::kFromFullRestore,
                                 std::move(window_info), base::DoNothing());
    } else {
      proxy->LaunchAppWithIntent(
          app_id, data_it->second->event_flag.value(),
          apps::ConvertIntentToMojomIntent(data_it->second->intent),
          apps::mojom::LaunchSource::kFromFullRestore,
          ConvertWindowInfoToMojomWindowInfo(window_info), {});
    }
  } else {
    if (base::FeatureList::IsEnabled(apps::kAppServiceLaunchWithoutMojom)) {
      proxy->Launch(app_id, data_it->second->event_flag.value(),
                    apps::LaunchSource::kFromFullRestore,
                    std::move(window_info));
    } else {
      proxy->Launch(app_id, data_it->second->event_flag.value(),
                    apps::mojom::LaunchSource::kFromFullRestore,
                    ConvertWindowInfoToMojomWindowInfo(window_info));
    }
  }

  if (!HasRestoreData())
    StopRestore();
}

void ArcAppLaunchHandler::RemoveWindowsForApp(const std::string& app_id) {
  app_ids_.erase(app_id);
  std::vector<int32_t> window_stacks;
  for (auto& it : windows_) {
    if (it.second.app_id == app_id)
      window_stacks.push_back(it.first);
  }

  for (auto window_stack : window_stacks)
    windows_.erase(window_stack);

  std::vector<std::list<WindowInfo>::iterator> windows;
  for (auto it = no_stack_windows_.begin(); it != no_stack_windows_.end();
       ++it) {
    if (it->app_id == app_id)
      windows.push_back(it);
  }

  for (auto it : windows)
    no_stack_windows_.erase(it);
  windows.clear();

  for (auto it = pending_windows_.begin(); it != pending_windows_.end(); ++it) {
    if (it->app_id == app_id)
      windows.push_back(it);
  }

  for (auto it : windows)
    pending_windows_.erase(it);
}

void ArcAppLaunchHandler::RemoveWindow(const std::string& app_id,
                                       int32_t window_id) {
  for (auto& it : windows_) {
    if (it.second.app_id == app_id && it.second.window_id == window_id) {
      windows_.erase(it.first);
      return;
    }
  }

  for (auto it = no_stack_windows_.begin(); it != no_stack_windows_.end();
       ++it) {
    if (it->app_id == app_id && it->window_id == window_id) {
      no_stack_windows_.erase(it);
      return;
    }
  }

  for (auto it = pending_windows_.begin(); it != pending_windows_.end(); ++it) {
    if (it->app_id == app_id && it->window_id == window_id) {
      pending_windows_.erase(it);
      return;
    }
  }
}

void ArcAppLaunchHandler::MaybeReStartTimer(const base::TimeDelta& delay) {
  DCHECK(app_launch_timer_);

  // If there is no window to be launched, stop the timer.
  if (!HasRestoreData()) {
    StopRestore();
    return;
  }

  if (current_delay_ == delay)
    return;

  // If the delay is changed, restart the timer.
  if (app_launch_timer_->IsRunning())
    app_launch_timer_->Stop();

  current_delay_ = delay;

  app_launch_timer_->Start(
      FROM_HERE, current_delay_,
      base::BindRepeating(&ArcAppLaunchHandler::MaybeLaunchApp,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppLaunchHandler::StopRestore() {
  if (app_launch_timer_ && app_launch_timer_->IsRunning())
    app_launch_timer_->Stop();
  app_launch_timer_.reset();

  if (stop_restore_timer_ && stop_restore_timer_->IsRunning())
    stop_restore_timer_->Stop();
  stop_restore_timer_.reset();

  auto* manager = GetSchedulerConfigurationManager();
  if (manager)
    manager->RemoveObserver(this);

  StopCpuUsageCount();

  RecordRestoreResult();
}

int ArcAppLaunchHandler::GetCpuUsageRate() {
  uint64_t idle = 0, sum = 0;
  for (const auto& tick : cpu_tick_window_) {
    idle += tick.idle_time;
    sum += tick.idle_time + tick.used_time;
  }

  // Convert to xx% percentage.
  return sum ? int(100 * (sum - idle) / sum) : 0;
}

void ArcAppLaunchHandler::StartCpuUsageCount() {
  cpu_tick_count_timer_.Start(
      FROM_HERE, base::Seconds(kCpuUsageRefreshIntervalInSeconds),
      base::BindRepeating(&ArcAppLaunchHandler::UpdateCpuUsage,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppLaunchHandler::StopCpuUsageCount() {
  probe_service_.reset();
  cpu_tick_count_timer_.Stop();
}

void ArcAppLaunchHandler::UpdateCpuUsage() {
  if (!probe_service_ || !probe_service_.is_connected())
    return;
  probe_service_->ProbeTelemetryInfo(
      {cros_healthd::mojom::ProbeCategoryEnum::kCpu},
      base::BindOnce(&ArcAppLaunchHandler::OnCpuUsageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppLaunchHandler::OnCpuUsageUpdated(
    cros_healthd::mojom::TelemetryInfoPtr info_ptr) {
  // May be null in tests.
  if (info_ptr.is_null() || info_ptr->cpu_result.is_null() ||
      info_ptr->cpu_result->get_cpu_info().is_null()) {
    return;
  }

  CpuTick tick;
  // For simplicity, assume that device has only one physical CPU.
  for (const auto& logical_cpu :
       info_ptr->cpu_result->get_cpu_info()->physical_cpus[0]->logical_cpus) {
    tick.idle_time += logical_cpu->idle_time_user_hz;
    tick.used_time +=
        logical_cpu->user_time_user_hz + logical_cpu->system_time_user_hz;
  }

  if (last_cpu_tick_.has_value())
    cpu_tick_window_.push_back(tick - last_cpu_tick_.value());
  last_cpu_tick_ = tick;

  // Sliding window for CPU usage count.
  while (cpu_tick_window_.size() > kCpuUsageCountWindowLength)
    cpu_tick_window_.pop_front();
}

void ArcAppLaunchHandler::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

void ArcAppLaunchHandler::RecordArcGhostWindowLaunch(bool is_arc_ghost_window) {
  base::UmaHistogramBoolean(kArcGhostWindowLaunchHistogram,
                            is_arc_ghost_window);

  if (!is_arc_ghost_window && !exo::WMHelper::HasInstance()) {
    base::UmaHistogramEnumeration(kNoGhostWindowReasonHistogram,
                                  NoGhostWindowReason::kNoExoHelper);
  }
}

void ArcAppLaunchHandler::RecordLaunchBoundsState(bool has_root_bounds,
                                                  bool has_screen_bounds) {
  bool is_from_crash = ExitTypeService::GetLastSessionExitType(
                           handler_->profile()) == ExitType::kCrashed;
  if (!has_root_bounds) {
    base::UmaHistogramEnumeration(
        kNoGhostWindowReasonHistogram,
        is_from_crash ? NoGhostWindowReason::kNoRootBoundsFromCrash
                      : NoGhostWindowReason::kNoRootBounds);
  }
  if (!has_screen_bounds) {
    base::UmaHistogramEnumeration(
        kNoGhostWindowReasonHistogram,
        is_from_crash ? NoGhostWindowReason::kNoScreenBoundsFromCrash
                      : NoGhostWindowReason::kNoScreenBounds);
  }
  if (!window_handler_) {
    base::UmaHistogramEnumeration(kNoGhostWindowReasonHistogram,
                                  is_from_crash
                                      ? NoGhostWindowReason::kNoHandlerFromCrash
                                      : NoGhostWindowReason::kNoHandler);
  }
}

void ArcAppLaunchHandler::RecordRestoreResult() {
  bool isFinished = !HasRestoreData();

  base::UmaHistogramEnumeration(
      kRestoredArcAppResultHistogram,
      isFinished ? RestoreResult::kFinish : RestoreResult::kNotFinish);

  ArcRestoreState restore_state = ArcRestoreState::kFailedWithUnknown;
  if (isFinished) {
    if (was_cpu_usage_limited_ && was_memory_pressured_)
      restore_state =
          ArcRestoreState::kSuccessWithMemoryPressureAndCPUUsageRateLimiting;
    else if (was_cpu_usage_limited_)
      restore_state = ArcRestoreState::kSuccessWithCPUUsageRateLimiting;
    else if (was_memory_pressured_)
      restore_state = ArcRestoreState::kSuccessWithMemoryPressure;
    else
      restore_state = ArcRestoreState::kSuccess;
  } else {
    if (was_cpu_usage_limited_ && was_memory_pressured_)
      restore_state =
          ArcRestoreState::kFailedWithMemoryPressureAndCPUUsageRateLimiting;
    else if (was_cpu_usage_limited_)
      restore_state = ArcRestoreState::kFailedWithCPUUsageRateLimiting;
    else if (was_memory_pressured_)
      restore_state = ArcRestoreState::kFailedWithMemoryPressure;
    // For other cases, mark the failed state as "unknown".
  }

  base::UmaHistogramEnumeration(kRestoreArcAppStatesHistogram, restore_state);

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (window_handler_) {
    base::UmaHistogramCounts100(kGhostWindowPopToArcHistogram,
                                window_handler_->ghost_window_pop_count());
  }
#endif
}

ash::SchedulerConfigurationManager*
ArcAppLaunchHandler::GetSchedulerConfigurationManager() {
  if (!g_browser_process || !g_browser_process->platform_part())
    return nullptr;
  return g_browser_process->platform_part()->scheduler_configuration_manager();
}

}  // namespace ash::app_restore
