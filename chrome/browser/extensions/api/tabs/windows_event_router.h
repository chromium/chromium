// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_event_histogram_value.h"

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
#include "ui/views/focus/widget_focus_manager.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/mac/key_window_notifier.h"
#endif

class Profile;

namespace extensions {

class AppWindow;
class AppWindowController;

// The WindowsEventRouter sends chrome.windows.* events to listeners
// inside extension process renderers. The router listens to *all* events,
// but will only route events within a profile to extension processes in the
// same profile.
class WindowsEventRouter : public AppWindowRegistry::Observer,
#if BUILDFLAG(IS_MAC)
                           public KeyWindowNotifier::Observer,
#elif defined(TOOLKIT_VIEWS)
                           public views::WidgetFocusChangeListener,
#endif
                           public WindowControllerListObserver {
 public:
  explicit WindowsEventRouter(Profile* profile);

  WindowsEventRouter(const WindowsEventRouter&) = delete;
  WindowsEventRouter& operator=(const WindowsEventRouter&) = delete;

  ~WindowsEventRouter() override;

  // |window_controller| is NULL to indicate a focused window has lost focus.
  void OnActiveWindowChanged(WindowController* window_controller);

 private:
  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* app_window) override;
  void OnAppWindowRemoved(AppWindow* app_window) override;
  void OnAppWindowActivated(AppWindow* app_window) override;

  // WindowControllerListObserver methods:
  void OnWindowControllerAdded(WindowController* window_controller) override;
  void OnWindowControllerRemoved(WindowController* window) override;
  void OnWindowBoundsChanged(WindowController* window_controller) override;

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;
#endif

#if BUILDFLAG(IS_MAC)
  // KeyWindowNotifier::Observer:
  void OnNoKeyWindow() override;
#endif

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     WindowController* window_controller,
                     base::Value::List args);
  bool HasEventListener(const std::string& event_name);
  void AddAppWindow(AppWindow* app_window);

  // The main profile that owns this event router.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // The profile the currently focused window belongs to; either the main or
  // incognito profile or NULL (none of the above). We remember this in order
  // to correctly handle focus changes between non-OTR and OTR windows.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> focused_profile_;

  // The currently focused window. We keep this so as to avoid sending multiple
  // windows.onFocusChanged events with the same windowId.
  int focused_window_id_;

  using AppWindowMap = std::map<int, std::unique_ptr<AppWindowController>>;
  // Map of application windows, the key to the session of the app window.
  AppWindowMap app_windows_;

  // Observed AppWindowRegistry.
  base::ScopedObservation<AppWindowRegistry, AppWindowRegistry::Observer>
      observed_app_registry_{this};

  // Observed WindowControllerList.
  base::ScopedObservation<WindowControllerList, WindowControllerListObserver>
      observed_controller_list_{this};

#if BUILDFLAG(IS_MAC)
  base::ScopedObservation<KeyWindowNotifier, KeyWindowNotifier::Observer>
      observed_key_window_notifier_{this};
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_
