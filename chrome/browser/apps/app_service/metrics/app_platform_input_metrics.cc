// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_input_metrics.h"

#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget.h"

namespace apps {

namespace {

InputEventSource GetInputEventSource(ui::EventPointerType type) {
  switch (type) {
    case ui::EventPointerType::kUnknown:
      return InputEventSource::kUnknown;
    case ui::EventPointerType::kMouse:
      return InputEventSource::kMouse;
    case ui::EventPointerType::kPen:
      return InputEventSource::kStylus;
    case ui::EventPointerType::kTouch:
      return InputEventSource::kTouch;
    case ui::EventPointerType::kEraser:
      return InputEventSource::kStylus;
  }
}

}  // namespace

AppPlatformInputMetrics::AppPlatformInputMetrics(
    Profile* profile,
    apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry)
    : profile_(profile) {
  InstanceRegistry::Observer::Observe(&instance_registry);
  AppRegistryCache::Observer::Observe(&app_registry_cache);
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->AddPreTargetHandler(this);
  }
}

AppPlatformInputMetrics::~AppPlatformInputMetrics() {
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->RemovePreTargetHandler(this);
  }
}

void AppPlatformInputMetrics::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_RELEASED) {
    RecordEventCount(GetInputEventSource(event->pointer_details().pointer_type),
                     event->target());
  }
}

void AppPlatformInputMetrics::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_RELEASED) {
    RecordEventCount(InputEventSource::kKeyboard, event->target());
  }
}

void AppPlatformInputMetrics::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_RELEASED) {
    RecordEventCount(GetInputEventSource(event->pointer_details().pointer_type),
                     event->target());
  }
}

void AppPlatformInputMetrics::OnFiveMinutes() {
  for (const auto& event_counts : app_id_to_event_count_per_five_minutes_) {
    ukm::SourceId source_id = GetSourceId(event_counts.first);
    if (source_id == ukm::kInvalidSourceId) {
      continue;
    }

    // `event_counts.second` is the map from InputEventSource to the event
    // counts.
    RecordInputEventsUkm(source_id, event_counts.second);
  }
  app_id_to_event_count_per_five_minutes_.clear();
}

void AppPlatformInputMetrics::OnInstanceUpdate(const InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }

  aura::Window* window = update.Window();
  if (update.State() & InstanceState::kDestroyed) {
    window_to_app_info_.erase(window);
    browser_to_tab_list_.RemoveActivatedTab(update.InstanceId());
    return;
  }

  auto app_id = update.AppId();
  auto app_type = GetAppType(profile_, app_id);

  // For apps, not opened with browser windows, the app id and app type should
  // not change. So if we have the app info for the window, we don't need to
  // update it.
  if (base::Contains(window_to_app_info_, window) &&
      !IsAppOpenedWithBrowserWindow(profile_, app_type, app_id)) {
    return;
  }

  if (update.State() & apps::InstanceState::kActive) {
    SetAppInfoForActivatedWindow(app_type, app_id, window, update.InstanceId());
  } else {
    SetAppInfoForInactivatedWindow(update.InstanceId());
  }
}

void AppPlatformInputMetrics::OnInstanceRegistryWillBeDestroyed(
    InstanceRegistry* cache) {
  InstanceRegistry::Observer::Observe(nullptr);
}

void AppPlatformInputMetrics::OnAppRegistryCacheWillBeDestroyed(
    AppRegistryCache* cache) {
  AppRegistryCache::Observer::Observe(nullptr);
}

void AppPlatformInputMetrics::OnAppUpdate(const AppUpdate& update) {
  if (!update.ReadinessChanged() ||
      apps_util::IsInstalled(update.Readiness())) {
    return;
  }

  auto it = app_id_to_source_id_.find(update.AppId());
  if (it == app_id_to_source_id_.end()) {
    return;
  }

  // Remove the source id when the app is removed. The source id will be added
  // when record the UKM, so we don't need to add the source id here when the
  // app is installed.
  AppPlatformMetrics::RemoveSourceId(it->second);
  app_id_to_source_id_.erase(it);
}

void AppPlatformInputMetrics::SetAppInfoForActivatedWindow(
    mojom::AppType app_type,
    const std::string& app_id,
    aura::Window* window,
    const base::UnguessableToken& instance_id) {
  // For the browser window, if a tab of the browser is activated, we don't
  // need to update, because we can reuse the active tab's app id.
  if (app_id == extension_misc::kChromeAppId &&
      browser_to_tab_list_.HasActivatedTab(window)) {
    return;
  }

  AppTypeName app_type_name =
      GetAppTypeNameForWindow(profile_, app_type, app_id, window);
  if (app_type_name == AppTypeName::kUnknown) {
    return;
  }

  // For apps opened in browser windows, get the top level window, and modify
  // `browser_to_tab_list_` to save the activated tab app id.
  if (IsAppOpenedWithBrowserWindow(profile_, app_type, app_id)) {
    window = window->GetToplevelWindow();
    if (IsAppOpenedInTab(app_type_name, app_id)) {
      // When the tab is pulled to a separate browser window, the instance id is
      // not changed, but the parent browser window is changed. So remove the
      // tab window instance from previous browser window, and add it to the new
      // browser window.
      browser_to_tab_list_.RemoveActivatedTab(instance_id);
      browser_to_tab_list_.AddActivatedTab(window, instance_id, app_id);
    }
  }

  window_to_app_info_[window].app_id = app_id;
  window_to_app_info_[window].app_type_name = app_type_name;
}

void AppPlatformInputMetrics::SetAppInfoForInactivatedWindow(
    const base::UnguessableToken& instance_id) {
  // When the window is inactived, only modify the app info for browser windows,
  // because the activated tab changing might affect the app id for browser
  // windows.
  //
  // For apps, not opened with browser windows,  the app id and app type should
  // not be changed for non-browser windows, and they can be modified when the
  // window is activated.
  auto* browser_window = browser_to_tab_list_.GetBrowserWindow(instance_id);
  if (!browser_window) {
    return;
  }

  browser_to_tab_list_.RemoveActivatedTab(instance_id);

  auto app_id = browser_to_tab_list_.GetActivatedTabAppId(browser_window);
  if (app_id.empty()) {
    app_id = extension_misc::kChromeAppId;
  }

  window_to_app_info_[browser_window].app_id = app_id;
  window_to_app_info_[browser_window].app_type_name =
      apps::AppTypeName::kChromeBrowser;
}

void AppPlatformInputMetrics::RecordEventCount(InputEventSource event_source,
                                               ui::EventTarget* event_target) {
  views::Widget* target = views::Widget::GetTopLevelWidgetForNativeView(
      static_cast<aura::Window*>(event_target));
  if (!target) {
    return;
  }

  aura::Window* top_window = target->GetNativeWindow();
  if (!top_window) {
    return;
  }

  auto it = window_to_app_info_.find(top_window);
  if (it == window_to_app_info_.end()) {
    return;
  }

  ++app_id_to_event_count_per_five_minutes_[it->second.app_id][event_source]
                                           [it->second.app_type_name];
}

ukm::SourceId AppPlatformInputMetrics::GetSourceId(const std::string& app_id) {
  auto it = app_id_to_source_id_.find(app_id);
  if (it != app_id_to_source_id_.end()) {
    return it->second;
  }

  auto source_id = AppPlatformMetrics::GetSourceId(profile_, app_id);
  app_id_to_source_id_[app_id] = source_id;
  return source_id;
}

void AppPlatformInputMetrics::RecordInputEventsUkm(
    ukm::SourceId source_id,
    const EventSourceToCounts& event_counts) {
  for (const auto& counts : event_counts) {
    InputEventSource event_source = counts.first;

    // `counts.second` is the map from AppTypeName to the event count.
    for (const auto& count : counts.second) {
      ukm::builders::ChromeOSApp_InputEvent builder(source_id);
      builder.SetAppType((int)count.first)
          .SetAppInputEventSource((int)event_source)
          .SetAppInputEventCount(count.second)
          .SetUserDeviceMatrix(GetUserTypeByDeviceTypeMetrics())
          .Record(ukm::UkmRecorder::Get());
    }
  }
}

}  // namespace apps
