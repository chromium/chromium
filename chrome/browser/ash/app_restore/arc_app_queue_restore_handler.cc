// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_app_queue_restore_handler.h"

#include <list>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/ash/shelf/arc_shelf_spinner_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/wm_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "ui/display/display.h"
#include "ui/wm/public/activation_client.h"

namespace ash::app_restore {

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

}  // namespace

ArcAppQueueRestoreHandler::ArcAppQueueRestoreHandler() {
  if (aura::Env::HasInstance())
    env_observer_.Observe(aura::Env::GetInstance());

  if (Shell::HasInstance() && Shell::Get()->GetPrimaryRootWindow()) {
    auto* activation_client =
        wm::GetActivationClient(Shell::Get()->GetPrimaryRootWindow());
    if (activation_client)
      activation_client->AddObserver(this);
  }

  auto* manager = GetSchedulerConfigurationManager();
  if (manager) {
    std::optional<std::pair<bool, size_t>> scheduler_configuration =
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

ArcAppQueueRestoreHandler::~ArcAppQueueRestoreHandler() {
  if (Shell::HasInstance() && Shell::Get()->GetPrimaryRootWindow()) {
    auto* activation_client =
        wm::GetActivationClient(Shell::Get()->GetPrimaryRootWindow());
    if (activation_client)
      activation_client->RemoveObserver(this);
  }

  auto* manager = GetSchedulerConfigurationManager();
  if (manager)
    manager->RemoveObserver(this);
}

void ArcAppQueueRestoreHandler::RestoreArcApps(
    AppLaunchHandler* app_launch_handler) {
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

  window_handler_ =
      AppRestoreArcTaskHandlerFactory::GetForProfile(handler_->profile())
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

void ArcAppQueueRestoreHandler::OnAppConnectionReady() {
  is_app_connection_ready_ = true;

  if (!HasRestoreData())
    return;

  base::UmaHistogramCounts100(kRestoredAppWindowCountHistogram,
                              windows_.size() + no_stack_windows_.size());

  // Receive the memory pressure level.
  if (ResourcedClient::Get() && !resourced_client_observer_.IsObserving())
    resourced_client_observer_.Observe(ResourcedClient::Get());

  // Receive the system CPU usage rate.
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(
        base::BindOnce(&ArcAppQueueRestoreHandler::OnProbeServiceDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  StartCpuUsageCount();

  if (!app_launch_timer_) {
    app_launch_timer_ = std::make_unique<base::RepeatingTimer>();
    MaybeReStartTimer(kAppLaunchCheckingDelay);
  }

  if (!stop_restore_timer_) {
    stop_restore_timer_ = std::make_unique<base::OneShotTimer>();
    stop_restore_timer_->Start(FROM_HERE, kStopRestoreDelay, this,
                               &ArcAppQueueRestoreHandler::StopRestore);
  }
}

void ArcAppQueueRestoreHandler::OnShelfReady() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcAppQueueRestoreHandler::PrepareLaunchApps,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppQueueRestoreHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (enabled)
    return;

  StopRestore();

  if (window_handler_) {
    std::set<int32_t> session_ids;
    for (const auto& it : session_id_to_window_id_)
      session_ids.insert(it.first);
    for (auto session_id : session_ids)
      window_handler_->CloseWindow(session_id);
  }

  app_ids_.clear();
  windows_.clear();
  no_stack_windows_.clear();
}

void ArcAppQueueRestoreHandler::LaunchApp(const std::string& app_id) {
  if (!IsAppReady(app_id))
    return;

  DCHECK(handler_);
  const auto it =
      handler_->restore_data()->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data()->app_id_to_launch_list().end())
    return;

  const auto& launch_list = it->second;
  if (launch_list.empty()) {
    handler_->restore_data()->RemoveApp(app_id);
    return;
  }

  for (const auto& [window_id, _] : launch_list)
    LaunchAppWindow(app_id, window_id);

  RemoveWindowsForApp(app_id);
}

bool ArcAppQueueRestoreHandler::IsAppPendingRestore(
    const std::string& app_id) const {
  return base::Contains(app_ids_, app_id);
}

void ArcAppQueueRestoreHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() || update.AppType() != apps::AppType::kArc)
    return;

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

void ArcAppQueueRestoreHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void ArcAppQueueRestoreHandler::OnWindowActivated(
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

  auto window_id = it->second;
  RemoveWindow(*arc_app_id, window_id);
  LaunchAppWindow(*arc_app_id, window_id);
}

void ArcAppQueueRestoreHandler::OnWindowInitialized(aura::Window* window) {
  // An app window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget. Tooltips, menus, and other kinds of transient
  // windows that can't activate are filtered out.
  if (window->GetType() != aura::client::WINDOW_TYPE_NORMAL ||
      !window->delegate()) {
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level() ||
      !arc::GetWindowSessionId(window).has_value()) {
    return;
  }

  observed_windows_.AddObservation(window);
}

void ArcAppQueueRestoreHandler::OnWindowDestroying(aura::Window* window) {
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

void ArcAppQueueRestoreHandler::OnConfigurationSet(bool success,
                                                   size_t num_cores_disabled) {
  // Logical CPU core number should consider system HyperThread status.
  should_apply_cpu_restirction_ =
      (base::SysInfo::NumberOfProcessors() - num_cores_disabled) <=
      kCpuRestrictCoresCondition;
  auto* manager = GetSchedulerConfigurationManager();
  if (manager)
    manager->RemoveObserver(this);
}

void ArcAppQueueRestoreHandler::LoadRestoreData() {
  DCHECK(handler_);
  for (const auto& it : handler_->restore_data()->app_id_to_launch_list())
    app_ids_.insert(it.first);
}

void ArcAppQueueRestoreHandler::AddWindows(const std::string& app_id) {
  DCHECK(handler_);
  auto it = handler_->restore_data()->app_id_to_launch_list().find(app_id);
  DCHECK(it != handler_->restore_data()->app_id_to_launch_list().end());
  const auto& launch_list = it->second;
  for (const auto& [window_id, app_restore_data] : launch_list) {
    if (app_restore_data->window_info.activation_index.has_value()) {
      windows_[app_restore_data->window_info.activation_index.value()] = {
          app_id, window_id};
    } else {
      no_stack_windows_.push_back({app_id, window_id});
    }
  }
}

void ArcAppQueueRestoreHandler::PrepareLaunchApps() {
  is_shelf_ready_ = true;

  // Explicit check if the root window controller initialized. b/321719023
  if (RootWindowController::root_window_controllers().empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ArcAppQueueRestoreHandler::PrepareLaunchApps,
                       weak_ptr_factory_.GetWeakPtr()),
        kAppLaunchCheckingDelay);
    return;
  }

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

void ArcAppQueueRestoreHandler::PrepareAppLaunching(const std::string& app_id) {
  DCHECK(handler_);
  app_ids_.erase(app_id);

  const auto it =
      handler_->restore_data()->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data()->app_id_to_launch_list().end())
    return;

  const auto& launch_list = it->second;
  if (launch_list.empty()) {
    handler_->restore_data()->RemoveApp(app_id);
    return;
  }

  // Activate ARC in case still not active.
  arc::ArcSessionManager::Get()->AllowActivation(
      arc::ArcSessionManager::AllowActivationReason::kRestoreApps);

  for (const auto& [window_id, app_restore_data] : launch_list) {
    handler_->RecordRestoredAppLaunch(apps::AppTypeName::kArc);

    DCHECK(app_restore_data->event_flag.has_value());

    // Set an ARC session id to find the restore window id based on the newly
    // created ARC task id. Note that the desk template launch ID must be set
    // first, if available.
    const int32_t arc_session_id = ::app_restore::CreateArcSessionId();
    if (desk_template_launch_id_ != 0) {
      ::app_restore::SetDeskTemplateLaunchIdForArcSessionId(
          arc_session_id, desk_template_launch_id_);
    }
    ::app_restore::SetArcSessionIdForWindowId(arc_session_id, window_id);
    window_id_to_session_id_[window_id] = arc_session_id;
    session_id_to_window_id_[arc_session_id] = window_id;

    bool launch_ghost_window = false;
    if (window_handler_ &&
        arc::CanLaunchGhostWindowByRestoreData(*app_restore_data) &&
        window_handler_->LaunchArcGhostWindow(app_id, arc_session_id,
                                              app_restore_data.get())) {
      launch_ghost_window = true;

      // Update ARC app states immediately, since the app states may already
      // changed from original state.
      ArcAppListPrefs* prefs = ArcAppListPrefs::Get(handler_->profile());
      if (prefs) {
        auto app_info = prefs->GetApp(app_id);
        if (app_info)
          window_handler_->OnAppStatesUpdate(app_id, app_info->ready,
                                             app_info->need_fixup);
      }
    }
    RecordArcGhostWindowLaunch(launch_ghost_window);

    const auto& file_path = handler_->profile()->GetPath();
    int32_t event_flags = app_restore_data->event_flag.value();
    int64_t display_id = app_restore_data->display_id.has_value()
                             ? app_restore_data->display_id.value()
                             : display::kInvalidDisplayId;
    if (app_restore_data->intent) {
      ::full_restore::SaveAppLaunchInfo(
          file_path, std::make_unique<::app_restore::AppLaunchInfo>(
                         app_id, event_flags, app_restore_data->intent->Clone(),
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
      apps::WindowInfoPtr window_info = std::make_unique<apps::WindowInfo>();
      window_info->window_id = arc_session_id;
      chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
          app_id, std::make_unique<ArcShelfSpinnerItemController>(
                      app_id, nullptr, app_restore_data->event_flag.value(),
                      arc::UserInteractionType::APP_STARTED_FROM_FULL_RESTORE,
                      apps::MakeArcWindowInfo(std::move(window_info))));
    }
  }
}

void ArcAppQueueRestoreHandler::OnMemoryPressure(
    ResourcedClient::PressureLevel level,
    memory_pressure::ReclaimTarget) {
  pressure_level_ = level;
}

bool ArcAppQueueRestoreHandler::HasRestoreData() {
  return !(windows_.empty() && no_stack_windows_.empty() &&
           pending_windows_.empty());
}

bool ArcAppQueueRestoreHandler::CanLaunchApp() {
  // Checks CPU usage limiting and memory pressure, make sure it can
  // be recorded for UMA statistic data.
  bool is_under_cpu_usage_limiting = IsUnderCPUUsageLimiting();
  if (is_under_cpu_usage_limiting)
    was_cpu_usage_limited_ = true;
  bool is_under_memory_pressure = IsUnderMemoryPressure();
  if (is_under_memory_pressure)
    was_memory_pressured_ = true;
  bool is_root_window_controller_initialized =
      !RootWindowController::root_window_controllers().empty();
  return !is_under_cpu_usage_limiting && !is_under_memory_pressure &&
         is_root_window_controller_initialized;
}

bool ArcAppQueueRestoreHandler::IsUnderMemoryPressure() {
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

bool ArcAppQueueRestoreHandler::IsUnderCPUUsageLimiting() {
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

bool ArcAppQueueRestoreHandler::IsAppReady(const std::string& app_id) {
  DCHECK(handler_);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(handler_->profile());
  if (!prefs)
    return false;

  return prefs->IsAbleToBeLaunched(app_id);
}

void ArcAppQueueRestoreHandler::MaybeLaunchApp() {
  // Check CanLaunchApp() first for record the system states.
  if (!CanLaunchApp() && !first_run_) {
    MaybeReStartTimer(kAppLaunchCheckingDelay);
    return;
  }

  const auto find_ready_window = [this](const std::list<WindowInfo>& l) {
    return std::find_if(l.begin(), l.end(), [this](const WindowInfo& info) {
      return IsAppReady(info.app_id);
    });
  };

  if (const auto it = find_ready_window(pending_windows_);
      it != pending_windows_.end()) {
    const WindowInfo info = *it;
    LaunchAppWindow(info.app_id, info.window_id);
    MaybeReStartTimer(kAppLaunchDelay);
    std::erase(pending_windows_, info);
    return;
  }

  if (!windows_.empty()) {
    auto it = windows_.begin();
    if (IsAppReady(it->second.app_id)) {
      launch_count_ = 0;
      LaunchAppWindow(it->second.app_id, it->second.window_id);
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

  if (auto it = find_ready_window(no_stack_windows_);
      it != no_stack_windows_.end()) {
    const WindowInfo info = *it;
    LaunchAppWindow(info.app_id, info.window_id);
    MaybeReStartTimer(kAppLaunchDelay);
    std::erase(no_stack_windows_, info);
  }
}

void ArcAppQueueRestoreHandler::LaunchAppWindow(const std::string& app_id,
                                                int32_t window_id) {
  DCHECK(handler_);

  const auto it =
      handler_->restore_data()->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data()->app_id_to_launch_list().end())
    return;

  const auto& launch_list = it->second;
  if (launch_list.empty()) {
    handler_->restore_data()->RemoveApp(app_id);
    return;
  }

  const auto data_it = launch_list.find(window_id);
  if (data_it == launch_list.end())
    return;
  const auto& app_restore_data = data_it->second;

  first_run_ = false;

  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile());
  DCHECK(proxy);

  DCHECK(app_restore_data->event_flag.has_value());

  apps::WindowInfoPtr window_info =
      full_restore::HandleArcWindowInfo(app_restore_data->GetAppWindowInfo());
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

  if (app_restore_data->intent) {
    proxy->LaunchAppWithIntent(app_id, app_restore_data->event_flag.value(),
                               app_restore_data->intent->Clone(),
                               apps::LaunchSource::kFromFullRestore,
                               std::move(window_info), base::DoNothing());
  } else {
    proxy->Launch(app_id, app_restore_data->event_flag.value(),
                  apps::LaunchSource::kFromFullRestore, std::move(window_info));
  }

  if (!HasRestoreData())
    StopRestore();
}

void ArcAppQueueRestoreHandler::RemoveWindowsForApp(const std::string& app_id) {
  app_ids_.erase(app_id);
  std::vector<int32_t> window_stacks;
  for (const auto& [window_stack, window_info] : windows_) {
    if (window_info.app_id == app_id)
      window_stacks.push_back(window_stack);
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

void ArcAppQueueRestoreHandler::RemoveWindow(const std::string& app_id,
                                             int32_t window_id) {
  for (auto& [window_stack, window_info] : windows_) {
    if (window_info.app_id == app_id && window_info.window_id == window_id) {
      windows_.erase(window_stack);
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

void ArcAppQueueRestoreHandler::MaybeReStartTimer(
    const base::TimeDelta& delay) {
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

  app_launch_timer_->Start(FROM_HERE, current_delay_, this,
                           &ArcAppQueueRestoreHandler::MaybeLaunchApp);
}

void ArcAppQueueRestoreHandler::StopRestore() {
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

int ArcAppQueueRestoreHandler::GetCpuUsageRate() {
  uint64_t idle = 0, sum = 0;
  for (const auto& tick : cpu_tick_window_) {
    idle += tick.idle_time;
    sum += tick.idle_time + tick.used_time;
  }

  // Convert to xx% percentage.
  return sum ? int(100 * (sum - idle) / sum) : 0;
}

void ArcAppQueueRestoreHandler::StartCpuUsageCount() {
  cpu_tick_count_timer_.Start(FROM_HERE,
                              base::Seconds(kCpuUsageRefreshIntervalInSeconds),
                              this, &ArcAppQueueRestoreHandler::UpdateCpuUsage);
}

void ArcAppQueueRestoreHandler::StopCpuUsageCount() {
  probe_service_.reset();
  cpu_tick_count_timer_.Stop();
}

void ArcAppQueueRestoreHandler::UpdateCpuUsage() {
  if (!probe_service_ || !probe_service_.is_connected())
    return;
  probe_service_->ProbeTelemetryInfo(
      {cros_healthd::mojom::ProbeCategoryEnum::kCpu},
      base::BindOnce(&ArcAppQueueRestoreHandler::OnCpuUsageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppQueueRestoreHandler::OnCpuUsageUpdated(
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

void ArcAppQueueRestoreHandler::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

void ArcAppQueueRestoreHandler::RecordArcGhostWindowLaunch(
    bool is_arc_ghost_window) {
  base::UmaHistogramBoolean(kArcGhostWindowLaunchHistogram,
                            is_arc_ghost_window);
}

void ArcAppQueueRestoreHandler::RecordRestoreResult() {
  bool isFinished = !HasRestoreData();

  base::UmaHistogramEnumeration(
      kRestoredArcAppResultHistogram,
      isFinished ? RestoreResult::kFinish : RestoreResult::kNotFinish);

  ArcRestoreState restore_state = ArcRestoreState::kFailedWithUnknown;
  if (isFinished) {
    if (was_cpu_usage_limited_ && was_memory_pressured_) {
      restore_state =
          ArcRestoreState::kSuccessWithMemoryPressureAndCPUUsageRateLimiting;
    } else if (was_cpu_usage_limited_) {
      restore_state = ArcRestoreState::kSuccessWithCPUUsageRateLimiting;
    } else if (was_memory_pressured_) {
      restore_state = ArcRestoreState::kSuccessWithMemoryPressure;
    } else {
      restore_state = ArcRestoreState::kSuccess;
    }
  } else {
    if (was_cpu_usage_limited_ && was_memory_pressured_) {
      restore_state =
          ArcRestoreState::kFailedWithMemoryPressureAndCPUUsageRateLimiting;
    } else if (was_cpu_usage_limited_) {
      restore_state = ArcRestoreState::kFailedWithCPUUsageRateLimiting;
    } else if (was_memory_pressured_) {
      restore_state = ArcRestoreState::kFailedWithMemoryPressure;
    }
    // For other cases, mark the failed state as "unknown".
  }

  base::UmaHistogramEnumeration(kRestoreArcAppStatesHistogram, restore_state);

  if (window_handler_) {
    base::UmaHistogramCounts100(kGhostWindowPopToArcHistogram,
                                window_handler_->ghost_window_pop_count());
  }
}

SchedulerConfigurationManager*
ArcAppQueueRestoreHandler::GetSchedulerConfigurationManager() {
  if (!g_browser_process || !g_browser_process->platform_part())
    return nullptr;
  return g_browser_process->platform_part()->scheduler_configuration_manager();
}

}  // namespace ash::app_restore
