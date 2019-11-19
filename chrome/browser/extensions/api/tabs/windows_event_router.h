// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_event_histogram_value.h"

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
#include "ui/views/focus/widget_focus_manager.h"  // nogncheck
#endif

class Profile;

namespace base {
class ListValue;
}

namespace extensions {

class AppWindow;
class AppWindowController;

// The WindowsEventRouter sends chrome.windows.* events to listeners
// inside extension process renderers. The router listens to *all* events,
// but will only route events within a profile to extension processes in the
// same profile.
class WindowsEventRouter : public AppWindowRegistry::Observer,
                           public WindowControllerListObserver,
#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
                           public views::WidgetFocusChangeListener,
#endif
                           public content::NotificationObserver {
 public:
  explicit WindowsEventRouter(Profile* profile);
  ~WindowsEventRouter() override;

  // |window_controller| is NULL to indicate a focused window has lost focus.
  void OnActiveWindowChanged(WindowController* window_controller);

 private:
  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;
  void OnAppWindowActivated(extensions::AppWindow* app_window) override;

  // WindowControllerListObserver methods:
  void OnWindowControllerAdded(WindowController* window_controller) override;
  void OnWindowControllerRemoved(WindowController* window) override;

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;
#endif

  // content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     WindowController* window_controller,
                     std::unique_ptr<base::ListValue> args);
  bool HasEventListener(const std::string& event_name);
  void AddAppWindow(extensions::AppWindow* app_window);

  content::NotificationRegistrar registrar_;

  // The main profile that owns this event router.
  Profile* profile_;

  // The profile the currently focused window belongs to; either the main or
  // incognito profile or NULL (none of the above). We remember this in order
  // to correctly handle focus changes between non-OTR and OTR windows.
  Profile* focused_profile_;

  // The currently focused window. We keep this so as to avoid sending multiple
  // windows.onFocusChanged events with the same windowId.
  int focused_window_id_;

  using AppWindowMap = std::map<int, std::unique_ptr<AppWindowController>>;
  // Map of application windows, the key to the session of the app window.
  AppWindowMap app_windows_;

  // Observed AppWindowRegistry.
  ScopedObserver<AppWindowRegistry, AppWindowRegistry::Observer>
      observed_app_registry_{this};

  // Observed WindowControllerList.
  ScopedObserver<WindowControllerList, WindowControllerListObserver>
      observed_controller_list_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowsEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_
