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

// Returns the app id for the active tab in the browser window `window`. If
// there is no app for the active tab, returns the Chrome browser app id.
std::string GetActiveAppIdForBrowserWindow(aura::Window* window) {
  auto* browser = chrome::FindBrowserWithWindow(window);
  if (!browser || !browser->tab_strip_model()) {
    return extension_misc::kChromeAppId;
  }

  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return extension_misc::kChromeAppId;
  }
  auto app_id = GetAppIdForWebContents(web_contents);
  return app_id.empty() ? extension_misc::kChromeAppId : app_id;
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

  if (update.State() & InstanceState::kDestroyed) {
    window_to_app_info_.erase(update.Window());
    return;
  }

  aura::Window* window = update.Window();
  auto app_id = update.AppId();
  AppTypeName app_type_name = GetAppTypeNameForWindow(
      profile_, GetAppType(profile_, app_id), app_id, window);
  if (app_type_name == AppTypeName::kUnknown) {
    return;
  }

  if (IsAppOpenedInTab(app_type_name, app_id)) {
    window = window->GetToplevelWindow();
    app_id = extension_misc::kChromeAppId;
  }

  if (app_id == extension_misc::kChromeAppId) {
    app_id = GetActiveAppIdForBrowserWindow(window);
  }

  window_to_app_info_[window].app_id = app_id;
  window_to_app_info_[window].app_type_name = app_type_name;
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
