// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_event_router.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/api/tabs/app_window_controller.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"

using content::BrowserContext;

namespace extensions {

namespace windows = extensions::api::windows;

namespace {

bool ControllerVisibleToListener(WindowController* window_controller,
                                 const Extension* extension,
                                 const base::DictionaryValue* listener_filter) {
  if (!window_controller)
    return false;

  // If there is no filter the visibility is based on the extension.
  const base::ListValue* filter_value = nullptr;
  if (listener_filter)
    listener_filter->GetList(extensions::tabs_constants::kWindowTypesKey,
                             &filter_value);

  // TODO(https://crbug.com/807313): Remove this.
  bool allow_dev_tools_windows = !!filter_value;
  if (!window_controller->IsVisibleToTabsAPIForExtension(
          extension, allow_dev_tools_windows)) {
    return false;
  }

  return !filter_value ||
         window_controller->MatchesFilter(
             WindowController::GetFilterFromWindowTypesValues(filter_value));
}

bool WillDispatchWindowEvent(WindowController* window_controller,
                             BrowserContext* browser_context,
                             Feature::Context target_context,
                             const Extension* extension,
                             Event* event,
                             const base::DictionaryValue* listener_filter) {
  bool has_filter =
      listener_filter &&
      listener_filter->HasKey(extensions::tabs_constants::kWindowTypesKey);
  // TODO(https://crbug.com/807313): Remove this.
  bool allow_dev_tools_windows = has_filter;
  if (!window_controller->IsVisibleToTabsAPIForExtension(
          extension, allow_dev_tools_windows)) {
    return false;
  }

  // Cleanup previous values.
  event->filter_info = EventFilteringInfo();
  // Only set the window type if the listener has set a filter.
  // Otherwise we set the window visibility relative to the extension.
  if (has_filter) {
    event->filter_info.window_type = window_controller->GetWindowTypeText();
  } else {
    event->filter_info.window_exposed_by_default = true;
  }
  return true;
}

bool WillDispatchWindowFocusedEvent(
    WindowController* window_controller,
    BrowserContext* browser_context,
    Feature::Context target_context,
    const Extension* extension,
    Event* event,
    const base::DictionaryValue* listener_filter) {
  int window_id = extension_misc::kUnknownWindowId;
  Profile* new_active_context = nullptr;
  bool has_filter =
      listener_filter &&
      listener_filter->HasKey(extensions::tabs_constants::kWindowTypesKey);

  // We might not have a window controller if the focus moves away
  // from chromium's windows.
  if (window_controller) {
    window_id = window_controller->GetWindowId();
    new_active_context = window_controller->profile();
  }

  // Cleanup previous values.
  event->filter_info = EventFilteringInfo();
  // Only set the window type if the listener has set a filter,
  // otherwise set the visibility to true (if the window is not
  // supposed to be visible by the extension, we will clear out the
  // window id later).
  if (has_filter) {
    event->filter_info.window_type =
        window_controller ? window_controller->GetWindowTypeText()
                          : extensions::tabs_constants::kWindowTypeValueNormal;
  } else {
    event->filter_info.window_exposed_by_default = true;
  }

  // When switching between windows in the default and incognito profiles,
  // dispatch WINDOW_ID_NONE to extensions whose profile lost focus that
  // can't see the new focused window across the incognito boundary.
  // See crbug.com/46610.
  bool cant_cross_incognito =
      new_active_context && new_active_context != browser_context &&
      !util::CanCrossIncognito(extension, browser_context);
  // If the window is not visible by the listener, we also need to
  // clear out the window id from the event.
  bool visible_to_listener = ControllerVisibleToListener(
      window_controller, extension, listener_filter);

  if (cant_cross_incognito || !visible_to_listener) {
    event->event_args->Clear();
    event->event_args->AppendInteger(extension_misc::kUnknownWindowId);
  } else {
    event->event_args->Clear();
    event->event_args->AppendInteger(window_id);
  }
  return true;
}

}  // namespace

WindowsEventRouter::WindowsEventRouter(Profile* profile)
    : profile_(profile),
      focused_profile_(nullptr),
      focused_window_id_(extension_misc::kUnknownWindowId) {
  DCHECK(!profile->IsOffTheRecord());

  observed_app_registry_.Add(AppWindowRegistry::Get(profile_));
  observed_controller_list_.Add(WindowControllerList::GetInstance());
  // Needed for when no suitable window can be passed to an extension as the
  // currently focused window. On Mac (even in a toolkit-views build) always
  // rely on the notification sent by AppControllerMac after AppKit sends
  // NSWindowDidBecomeKeyNotification and there is no [NSApp keyWindo7w]. This
  // allows windows not created by toolkit-views to be tracked.
  // TODO(tapted): Remove the ifdefs (and NOTIFICATION_NO_KEY_WINDOW) when
  // Chrome on Mac only makes windows with toolkit-views.
#if defined(OS_MACOSX)
  registrar_.Add(this, chrome::NOTIFICATION_NO_KEY_WINDOW,
                 content::NotificationService::AllSources());
#elif defined(TOOLKIT_VIEWS)
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
#else
#error Unsupported
#endif

  AppWindowRegistry* registry = AppWindowRegistry::Get(profile_);
  for (AppWindow* app_window : registry->app_windows())
    AddAppWindow(app_window);
}

WindowsEventRouter::~WindowsEventRouter() {
#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
#endif
}

void WindowsEventRouter::OnAppWindowAdded(extensions::AppWindow* app_window) {
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(app_window->browser_context())))
    return;
  AddAppWindow(app_window);
}

void WindowsEventRouter::OnAppWindowRemoved(extensions::AppWindow* app_window) {
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(app_window->browser_context())))
    return;

  app_windows_.erase(app_window->session_id().id());
}

void WindowsEventRouter::OnAppWindowActivated(
    extensions::AppWindow* app_window) {
  AppWindowMap::const_iterator iter =
      app_windows_.find(app_window->session_id().id());
  OnActiveWindowChanged(iter != app_windows_.end() ? iter->second.get()
                                                   : nullptr);
}

void WindowsEventRouter::OnWindowControllerAdded(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnCreated::kEventName))
    return;
  if (!profile_->IsSameProfile(window_controller->profile()))
    return;
  // Ignore any windows without an associated browser (e.g., AppWindows).
  if (!window_controller->GetBrowser())
    return;

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  // Since we don't populate tab info here, the context type doesn't matter.
  constexpr ExtensionTabUtil::PopulateTabBehavior populate_behavior =
      ExtensionTabUtil::kDontPopulateTabs;
  constexpr Feature::Context context_type = Feature::UNSPECIFIED_CONTEXT;
  args->Append(ExtensionTabUtil::CreateWindowValueForExtension(
      *window_controller->GetBrowser(), nullptr, populate_behavior,
      context_type));
  DispatchEvent(events::WINDOWS_ON_CREATED, windows::OnCreated::kEventName,
                window_controller, std::move(args));
}

void WindowsEventRouter::OnWindowControllerRemoved(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnRemoved::kEventName))
    return;
  if (!profile_->IsSameProfile(window_controller->profile()))
    return;
  // Ignore any windows without an associated browser (e.g., AppWindows).
  if (!window_controller->GetBrowser())
    return;

  int window_id = window_controller->GetWindowId();
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendInteger(window_id);
  DispatchEvent(events::WINDOWS_ON_REMOVED, windows::OnRemoved::kEventName,
                window_controller, std::move(args));
}

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
void WindowsEventRouter::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (!focused_now)
    OnActiveWindowChanged(nullptr);
}
#endif

void WindowsEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  DCHECK_EQ(chrome::NOTIFICATION_NO_KEY_WINDOW, type);
  OnActiveWindowChanged(nullptr);
#endif
}

void WindowsEventRouter::OnActiveWindowChanged(
    WindowController* window_controller) {
  Profile* window_profile = nullptr;
  int window_id = extension_misc::kUnknownWindowId;
  if (window_controller &&
      profile_->IsSameProfile(window_controller->profile())) {
    window_profile = window_controller->profile();
    window_id = window_controller->GetWindowId();
  }

  if (focused_window_id_ == window_id)
    return;

  // window_profile is either the default profile for the active window, its
  // incognito profile, or nullptr if the previous profile is losing focus.
  focused_profile_ = window_profile;
  focused_window_id_ = window_id;

  if (!HasEventListener(windows::OnFocusChanged::kEventName))
    return;

  std::unique_ptr<Event> event = std::make_unique<Event>(
      events::WINDOWS_ON_FOCUS_CHANGED, windows::OnFocusChanged::kEventName,
      std::make_unique<base::ListValue>());
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchWindowFocusedEvent, window_controller);
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

void WindowsEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                       const std::string& event_name,
                                       WindowController* window_controller,
                                       std::unique_ptr<base::ListValue> args) {
  auto event =
      std::make_unique<Event>(histogram_value, event_name, std::move(args),
                              window_controller->profile());
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchWindowEvent, window_controller);
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

bool WindowsEventRouter::HasEventListener(const std::string& event_name) {
  return EventRouter::Get(profile_)->HasEventListener(event_name);
}

void WindowsEventRouter::AddAppWindow(extensions::AppWindow* app_window) {
  std::unique_ptr<AppWindowController> controller(new AppWindowController(
      app_window, std::make_unique<AppBaseWindow>(app_window), profile_));
  app_windows_[app_window->session_id().id()] = std::move(controller);
}

}  // namespace extensions
