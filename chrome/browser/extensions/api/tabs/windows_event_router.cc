// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_event_router.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/api/tabs/app_window_controller.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process_platform_part.h"
#endif

using content::BrowserContext;

namespace extensions {

namespace windows = extensions::api::windows;

namespace {

constexpr char kWindowTypesKey[] = "windowTypes";

bool ControllerVisibleToListener(WindowController* window_controller,
                                 const Extension* extension,
                                 const base::Value::Dict* listener_filter) {
  if (!window_controller)
    return false;

  // If there is no filter the visibility is based on the extension.
  const base::Value::List* filter_value = nullptr;
  if (listener_filter) {
    filter_value = listener_filter->FindList(kWindowTypesKey);
  }

  // TODO(crbug.com/41367902): Remove this.
  bool allow_dev_tools_windows = !!filter_value;
  if (!window_controller->IsVisibleToTabsAPIForExtension(
          extension, allow_dev_tools_windows)) {
    return false;
  }

  return !filter_value ||
         window_controller->MatchesFilter(
             WindowController::GetFilterFromWindowTypesValues(filter_value));
}

bool WillDispatchWindowEvent(
    WindowController* window_controller,
    BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  bool has_filter =
      listener_filter && listener_filter->contains(kWindowTypesKey);
  // TODO(crbug.com/41367902): Remove this.
  bool allow_dev_tools_windows = has_filter;
  if (!window_controller->IsVisibleToTabsAPIForExtension(
          extension, allow_dev_tools_windows)) {
    return false;
  }

  event_filtering_info_out = mojom::EventFilteringInfo::New();
  // Only set the window type if the listener has set a filter.
  // Otherwise we set the window visibility relative to the extension.
  if (has_filter) {
    event_filtering_info_out->window_type =
        window_controller->GetWindowTypeText();
  } else {
    event_filtering_info_out->has_window_exposed_by_default = true;
    event_filtering_info_out->window_exposed_by_default = true;
  }
  return true;
}

bool WillDispatchWindowFocusedEvent(
    WindowController* window_controller,
    BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  int window_id = extension_misc::kUnknownWindowId;
  Profile* new_active_context = nullptr;
  bool has_filter =
      listener_filter && listener_filter->contains(kWindowTypesKey);

  // We might not have a window controller if the focus moves away
  // from chromium's windows.
  if (window_controller) {
    window_id = window_controller->GetWindowId();
    new_active_context = window_controller->profile();
  }

  event_filtering_info_out = mojom::EventFilteringInfo::New();
  // Only set the window type if the listener has set a filter,
  // otherwise set the visibility to true (if the window is not
  // supposed to be visible by the extension, we will clear out the
  // window id later).
  if (has_filter) {
    event_filtering_info_out->window_type =
        window_controller ? window_controller->GetWindowTypeText()
                          : api::tabs::ToString(api::tabs::WindowType::kNormal);
  } else {
    event_filtering_info_out->has_window_exposed_by_default = true;
    event_filtering_info_out->window_exposed_by_default = true;
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

  event_args_out.emplace();
  if (cant_cross_incognito || !visible_to_listener) {
    event_args_out->Append(extension_misc::kUnknownWindowId);
  } else {
    event_args_out->Append(window_id);
  }
  return true;
}

}  // namespace

WindowsEventRouter::WindowsEventRouter(Profile* profile)
    : profile_(profile),
      focused_profile_(nullptr),
      focused_window_id_(extension_misc::kUnknownWindowId) {
  DCHECK(!profile->IsOffTheRecord());

  observed_app_registry_.Observe(AppWindowRegistry::Get(profile_));
  observed_controller_list_.Observe(WindowControllerList::GetInstance());
  // Needed for when no suitable window can be passed to an extension as the
  // currently focused window. On Mac (even in a toolkit-views build) always
  // rely on the notification sent by AppControllerMac after AppKit sends
  // NSWindowDidBecomeKeyNotification and there is no [NSApp keyWindo7w]. This
  // allows windows not created by toolkit-views to be tracked.
#if BUILDFLAG(IS_MAC)
  observed_key_window_notifier_.Observe(
      &g_browser_process->platform_part()->key_window_notifier());
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
#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
#endif
}

void WindowsEventRouter::OnAppWindowAdded(AppWindow* app_window) {
  if (!profile_->IsSameOrParent(
          Profile::FromBrowserContext(app_window->browser_context())))
    return;
  AddAppWindow(app_window);
}

void WindowsEventRouter::OnAppWindowRemoved(AppWindow* app_window) {
  if (!profile_->IsSameOrParent(
          Profile::FromBrowserContext(app_window->browser_context())))
    return;

  app_windows_.erase(app_window->session_id().id());
}

void WindowsEventRouter::OnAppWindowActivated(AppWindow* app_window) {
  AppWindowMap::const_iterator iter =
      app_windows_.find(app_window->session_id().id());
  OnActiveWindowChanged(iter != app_windows_.end() ? iter->second.get()
                                                   : nullptr);
}

void WindowsEventRouter::OnWindowControllerAdded(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnCreated::kEventName))
    return;
  if (!profile_->IsSameOrParent(window_controller->profile()))
    return;
  // Ignore any windows without an associated browser (e.g., AppWindows).
  if (!window_controller->GetBrowser())
    return;

  base::Value::List args;
  // Since we don't populate tab info here, the context type doesn't matter.
  constexpr WindowController::PopulateTabBehavior populate_behavior =
      WindowController::kDontPopulateTabs;
  constexpr mojom::ContextType context_type = mojom::ContextType::kUnspecified;
  args.Append(window_controller->CreateWindowValueForExtension(
      nullptr, populate_behavior, context_type));
  DispatchEvent(events::WINDOWS_ON_CREATED, windows::OnCreated::kEventName,
                window_controller, std::move(args));
}

void WindowsEventRouter::OnWindowControllerRemoved(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnRemoved::kEventName))
    return;
  if (!profile_->IsSameOrParent(window_controller->profile()))
    return;
  // Ignore any windows without an associated browser (e.g., AppWindows).
  if (!window_controller->GetBrowser())
    return;

  int window_id = window_controller->GetWindowId();
  base::Value::List args;
  args.Append(window_id);
  DispatchEvent(events::WINDOWS_ON_REMOVED, windows::OnRemoved::kEventName,
                window_controller, std::move(args));
}

void WindowsEventRouter::OnWindowBoundsChanged(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnBoundsChanged::kEventName))
    return;
  if (!profile_->IsSameOrParent(window_controller->profile()))
    return;
  // Ignore any windows without an associated browser (e.g., AppWindows).
  if (!window_controller->GetBrowser())
    return;

  base::Value::List args;
  // Since we don't populate tab info here, the context type doesn't matter.
  constexpr WindowController::PopulateTabBehavior populate_behavior =
      WindowController::kDontPopulateTabs;
  constexpr mojom::ContextType context_type = mojom::ContextType::kUnspecified;
  args.Append(ExtensionTabUtil::CreateWindowValueForExtension(
      *window_controller->GetBrowser(), nullptr, populate_behavior,
      context_type));
  DispatchEvent(events::WINDOWS_ON_BOUNDS_CHANGED,
                windows::OnBoundsChanged::kEventName, window_controller,
                std::move(args));
}

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
void WindowsEventRouter::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (!focused_now)
    OnActiveWindowChanged(nullptr);
}
#endif

#if BUILDFLAG(IS_MAC)
void WindowsEventRouter::OnNoKeyWindow() {
  OnActiveWindowChanged(nullptr);
}
#endif

void WindowsEventRouter::OnActiveWindowChanged(
    WindowController* window_controller) {
  Profile* window_profile = nullptr;
  int window_id = extension_misc::kUnknownWindowId;
  if (window_controller &&
      profile_->IsSameOrParent(window_controller->profile())) {
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
      base::Value::List());
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchWindowFocusedEvent, window_controller);
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

void WindowsEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                       const std::string& event_name,
                                       WindowController* window_controller,
                                       base::Value::List args) {
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

void WindowsEventRouter::AddAppWindow(AppWindow* app_window) {
  std::unique_ptr<AppWindowController> controller(new AppWindowController(
      app_window, std::make_unique<AppBaseWindow>(app_window), profile_));
  app_windows_[app_window->session_id().id()] = std::move(controller);
}

}  // namespace extensions
