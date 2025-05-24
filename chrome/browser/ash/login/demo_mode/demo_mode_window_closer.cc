// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kAndroidGMSCorePackage[] = "com.google.android.gms";

std::string GetPackageNameFromAppId(const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());

  std::string publisher_id;
  proxy->AppRegistryCache().ForOneApp(app_id,
                                      [&](const apps::AppUpdate& update) {
                                        publisher_id = update.PublisherId();
                                      });
  return publisher_id;
}

void CloseWidgetWithInstanceId(const base::UnguessableToken& instance_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  proxy->InstanceRegistry().ForOneInstance(
      instance_id, [](const apps::InstanceUpdate& update) {
        auto* widget = views::Widget::GetWidgetForNativeWindow(update.Window());
        if (widget) {
          widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
        }
      });
}

void ClearAndCloseClipboard() {
  // Since this function is posted on another thread, we need to make sure all
  // the instances are not destroyed.
  if (!ash::Shell::HasInstance()) {
    return;
  }
  ash::ClipboardHistoryControllerImpl* clipboard_history_controller =
      ash::Shell::Get()->clipboard_history_controller();
  if (!clipboard_history_controller ||
      !clipboard_history_controller->history()) {
    return;
  }
  ash::ClipboardHistory* clipboard_history = const_cast<ash::ClipboardHistory*>(
      clipboard_history_controller->history());
  // Clear the clipboard. Clearing the clipboard will automatically close the
  // clipboard because it has no contents.
  clipboard_history->Clear();
}

}  // namespace

DemoModeWindowCloser::DemoModeWindowCloser(
    LaunchDemoAppCallback launch_demo_app_callback)
    : launch_demo_app_callback_(launch_demo_app_callback) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  browser_observation_.Observe(BrowserList::GetInstance());

  // Some test profiles will not have AppServiceProxy.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  CHECK(proxy);
  scoped_observation_.Observe(&proxy->InstanceRegistry());
}

DemoModeWindowCloser::~DemoModeWindowCloser() = default;

void DemoModeWindowCloser::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  // We should always close GMS core window to avoid the block for attract loop.
  if (CloseGMSCoreWindowIfPresent(update)) {
    return;
  }
  if (!update.StateChanged()) {
    return;
  }

  const bool is_arc_app = ash::IsArcWindow(update.Window());
  const std::string app_id_or_package =
      is_arc_app ? GetPackageNameFromAppId(update.AppId()) : update.AppId();
  const auto instance_id = update.InstanceId();
  auto* metric_recorder = ash::DemoSessionMetricsRecorder::Get();
  CHECK(metric_recorder);

  if (update.IsCreation()) {
    const bool is_widget_app =
        views::Widget::GetWidgetForNativeWindow(update.Window());
    if (is_widget_app) {
      // Some Chrome app has no widget, it will be closed by
      // `chrome::CloseAllBrowsers`.
      opened_apps_with_widget_.insert(instance_id);
    }
    metric_recorder->OnAppCreation(app_id_or_package, is_arc_app);
  } else if (update.IsDestruction()) {
    metric_recorder->OnAppDestruction(app_id_or_package, is_arc_app);
    opened_apps_with_widget_.erase(instance_id);
  }
}

void DemoModeWindowCloser::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  if (scoped_observation_.GetSource() == cache) {
    scoped_observation_.Reset();
  }
}

void DemoModeWindowCloser::OnBrowserRemoved(Browser* browser) {
  // `OnBrowserRemoved` is called after `browser` is removed from
  // `browser_list`, triggered by `DemoModeWindowCloser::StartClosingApps()` or
  // other process. If `is_reseting_browser_` is false, the browser removed is
  // triggered by user or other process. Return directly for this case. If
  // `is_reseting_browser_` is true, we need to wait until the last browser is
  // removed to avoid the race condition of browser removal and closing widgets.
  if (!is_reseting_browser_ || !BrowserList::GetInstance()->empty()) {
    return;
  }

  is_reseting_browser_ = false;
  StartClosingWidgets();
}

void DemoModeWindowCloser::StartClosingApps() {
  if (BrowserList::GetInstance()->empty()) {
    StartClosingWidgets();
  } else {
    is_reseting_browser_ = true;

    // If browsers are open, close them right now and delay
    // `StartClosingWidgets` to all browser removed to avoid race condition.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&chrome::CloseAllBrowsers));
  }

  // TODO(crbug.com/379946574): Close chromevox/screen capture or any other
  // non-window/widget if open.

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClearAndCloseClipboard));
}

// TODO(crbug.com/302583338): Remove this function once
// `DemoMode.GMSCoreDialogShown` never present.
bool DemoModeWindowCloser::CloseGMSCoreWindowIfPresent(
    const apps::InstanceUpdate& update) {
  if (!gms_core_app_id_.empty()) {
    if (update.AppId() != gms_core_app_id_) {
      return false;
    }
  } else if (GetPackageNameFromAppId(update.AppId()) !=
             kAndroidGMSCorePackage) {
    return false;
  }

  gms_core_app_id_ = update.AppId();

  // Post the task to close only when the window is being created.
  if (update.IsCreation()) {
    base::UmaHistogramBoolean("DemoMode.GMSCoreDialogShown", true);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseWidgetWithInstanceId, update.InstanceId()));
  }
  return true;
}

void DemoModeWindowCloser::StartClosingWidgets() {
  // Make a copy for `opened_apps_with_widget_` in case it is modified before
  // looping finishes.
  auto opened_apps_copies = opened_apps_with_widget_;

  // Start closing non-browser apps with widget. It is safe even for
  // `launch_demo_app_callback_` finish earlier since it has no effect for demo
  // app. Demo app has no widget for its window.
  for (const auto& app : opened_apps_copies) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CloseWidgetWithInstanceId, app));
  }

  // Since all browsers are closed, it's safe to re-luanch demo app.
  launch_demo_app_callback_.Run();
}
