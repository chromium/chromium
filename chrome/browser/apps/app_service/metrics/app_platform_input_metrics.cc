// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_input_metrics.h"

#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/app_constants/constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget.h"

namespace apps {

namespace {

constexpr char kInputEventMouseKey[] = "mouse";
constexpr char kInputEventStylusKey[] = "stylus";
constexpr char kInputEventTouchKey[] = "touch";
constexpr char kInputEventKeyboardKey[] = "keyboard";

base::flat_map<std::string, InputEventSource>& GetInputEventSourceMap() {
  static base::NoDestructor<base::flat_map<std::string, InputEventSource>>
      input_event_source_map;
  if (input_event_source_map->empty()) {
    *input_event_source_map = {
        {kInputEventMouseKey, InputEventSource::kMouse},
        {kInputEventStylusKey, InputEventSource::kStylus},
        {kInputEventTouchKey, InputEventSource::kTouch},
        {kInputEventKeyboardKey, InputEventSource::kKeyboard},
    };
  }
  return *input_event_source_map;
}

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

// Returns the input event source for the given `event_source` string.
InputEventSource GetInputEventSourceFromString(
    const std::string& event_source) {
  const auto& input_event_source_map = GetInputEventSourceMap();
  auto it = input_event_source_map.find(event_source);
  return (it != input_event_source_map.end()) ? it->second
                                              : InputEventSource::kUnknown;
}

// Returns the string key for `event_source` to save input events in the user
// pref.
std::string GetInputEventSourceKey(InputEventSource event_source) {
  switch (event_source) {
    case InputEventSource::kUnknown:
      return std::string();
    case InputEventSource::kMouse:
      return kInputEventMouseKey;
    case InputEventSource::kStylus:
      return kInputEventStylusKey;
    case InputEventSource::kTouch:
      return kInputEventTouchKey;
    case InputEventSource::kKeyboard:
      return kInputEventKeyboardKey;
  }
}

base::Value::Dict ConvertEventCountsToValue(
    const AppPlatformInputMetrics::EventSourceToCounts& event_counts) {
  base::Value::Dict event_counts_dict;
  for (const auto& counts : event_counts) {
    base::Value::Dict count_dict;
    for (const auto& it : counts.second) {
      count_dict.Set(GetAppTypeHistogramName(it.first), it.second);
    }
    event_counts_dict.Set(GetInputEventSourceKey(counts.first),
                          std::move(count_dict));
  }
  return event_counts_dict;
}

AppPlatformInputMetrics::EventSourceToCounts ConvertDictValueToEventCounts(
    const base::Value::Dict& event_counts) {
  AppPlatformInputMetrics::EventSourceToCounts ret;
  for (const auto [app_id, counts] : event_counts) {
    auto event_source = GetInputEventSourceFromString(app_id);
    if (event_source == InputEventSource::kUnknown) {
      continue;
    }

    const base::Value::Dict* counts_dict = counts.GetIfDict();
    if (!counts_dict) {
      continue;
    }
    for (const auto [app_type, count_value] : *counts_dict) {
      auto app_type_name = GetAppTypeNameFromString(app_type);
      if (app_type_name == AppTypeName::kUnknown) {
        continue;
      }

      auto count = count_value.GetIfInt();
      if (!count.has_value()) {
        continue;
      }
      ret[event_source][app_type_name] = count.value();
    }
  }
  return ret;
}

}  // namespace

constexpr char kAppInputEventsKey[] = "app_platform_metrics.app_input_events";

AppPlatformInputMetrics::AppPlatformInputMetrics(
    Profile* profile,
    const apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry)
    : profile_(profile), app_registry_cache_(app_registry_cache) {
  instance_registry_observation_.Observe(&instance_registry);
  if (chromeos::IsManagedGuestSession()) {
    CHECK(ukm::UkmRecorder::Get());
    ukm_recorder_observer_.Observe(ukm::UkmRecorder::Get());
  }
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
  if (event->type() == ui::EventType::kMouseReleased) {
    RecordEventCount(GetInputEventSource(event->pointer_details().pointer_type),
                     event->target());
  }
}

void AppPlatformInputMetrics::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::EventType::kKeyReleased) {
    RecordEventCount(InputEventSource::kKeyboard, event->target());
  }
}

void AppPlatformInputMetrics::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchReleased) {
    RecordEventCount(GetInputEventSource(event->pointer_details().pointer_type),
                     event->target());
  }
}

void AppPlatformInputMetrics::OnFiveMinutes() {
  // For the first five minutes, since the saved input events in pref haven't
  // been recorded yet, read the input events saved in the user pref, and record
  // the input events UKM, then save the new input events to the user pref.
  if (should_record_ukm_from_pref_) {
    RecordInputEventsAppKMFromPref();
    should_record_ukm_from_pref_ = false;
  }
  SaveInputEvents();
}

void AppPlatformInputMetrics::OnTwoHours() {
  RecordInputEventsAppKM();
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
  instance_registry_observation_.Reset();
}

void AppPlatformInputMetrics::OnStartingShutdown() {
  CHECK(chromeos::IsManagedGuestSession());
  RecordInputEventsAppKM();
}

void AppPlatformInputMetrics::SetAppInfoForActivatedWindow(
    AppType app_type,
    const std::string& app_id,
    aura::Window* window,
    const base::UnguessableToken& instance_id) {
  // For the browser window, if a tab of the browser is activated, we don't
  // need to update, because we can reuse the active tab's app id.
  if (app_id == app_constants::kChromeAppId &&
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
    window = IsLacrosWindow(window) ? window : window->GetToplevelWindow();
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
    app_id = IsLacrosWindow(browser_window) ? app_constants::kLacrosAppId
                                            : app_constants::kChromeAppId;
  }

  window_to_app_info_[browser_window].app_id = app_id;
  window_to_app_info_[browser_window].app_type_name =
      IsLacrosWindow(browser_window) ? apps::AppTypeName::kStandaloneBrowser
                                     : apps::AppTypeName::kChromeBrowser;
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

  if (!ShouldRecordAppKMForApp(it->second.app_id)) {
    return;
  }

  ++app_id_to_event_count_per_two_hours_[it->second.app_id][event_source]
                                        [it->second.app_type_name];
}

void AppPlatformInputMetrics::RecordInputEventsAppKM() {
  if (!ShouldRecordAppKM(profile_)) {
    return;
  }

  for (const auto& event_counts : app_id_to_event_count_per_two_hours_) {
    if (!ShouldRecordAppKMForApp(event_counts.first)) {
      continue;
    }
    // `event_counts.second` is the map from InputEventSource to the event
    // counts.
    RecordInputEventsAppKMForApp(event_counts.first, event_counts.second);
  }

  app_id_to_event_count_per_two_hours_.clear();
}

void AppPlatformInputMetrics::RecordInputEventsAppKMForApp(
    const std::string& app_id,
    const EventSourceToCounts& event_counts) {
  for (const auto& counts : event_counts) {
    InputEventSource event_source = counts.first;

    // `counts.second` is the map from AppTypeName to the event count.
    for (const auto& count : counts.second) {
      auto source_id = AppPlatformMetrics::GetSourceId(profile_, app_id);
      if (source_id == ukm::kInvalidSourceId) {
        continue;
      }
      ukm::builders::ChromeOSApp_InputEvent builder(source_id);
      builder.SetAppType((int)count.first)
          .SetAppInputEventSource((int)event_source)
          .SetAppInputEventCount(count.second)
          .SetUserDeviceMatrix(GetUserTypeByDeviceTypeMetrics())
          .Record(ukm::UkmRecorder::Get());
      AppPlatformMetrics::RemoveSourceId(source_id);
    }
  }
}

void AppPlatformInputMetrics::SaveInputEvents() {
  ScopedDictPrefUpdate input_events_update(profile_->GetPrefs(),
                                           kAppInputEventsKey);
  input_events_update->clear();
  for (const auto& event_counts : app_id_to_event_count_per_two_hours_) {
    input_events_update->SetByDottedPath(
        event_counts.first, ConvertEventCountsToValue(event_counts.second));
  }
}

void AppPlatformInputMetrics::RecordInputEventsAppKMFromPref() {
  if (!ShouldRecordAppKM(profile_)) {
    return;
  }

  ScopedDictPrefUpdate input_events_update(profile_->GetPrefs(),
                                           kAppInputEventsKey);

  for (const auto [app_id, events] : *input_events_update) {
    if (!ShouldRecordAppKMForApp(app_id)) {
      continue;
    }

    const base::Value::Dict* events_dict = events.GetIfDict();
    if (!events_dict) {
      continue;
    }

    EventSourceToCounts event_counts =
        ConvertDictValueToEventCounts(*events_dict);
    RecordInputEventsAppKMForApp(app_id, event_counts);
  }
}

bool AppPlatformInputMetrics::ShouldRecordAppKMForApp(
    const std::string& app_id) {
  return ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(),
                                   app_id) &&
         ShouldRecordAppKMForAppTypeName(GetAppType(profile_, app_id));
}

}  // namespace apps
