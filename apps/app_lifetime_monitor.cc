// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_lifetime_monitor.h"

#include "base/observer_list.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/extension.h"

namespace apps {

using extensions::AppWindow;
using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ExtensionHost;

AppLifetimeMonitor::AppLifetimeMonitor(content::BrowserContext* context)
    : context_(context) {
  extension_host_registry_observation_.Observe(
      extensions::ExtensionHostRegistry::Get(context_));

  AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Factory::GetForBrowserContext(context_,
                                                       false /* create */);
  DCHECK(app_window_registry);
  app_window_registry->AddObserver(this);
}

AppLifetimeMonitor::~AppLifetimeMonitor() = default;

void AppLifetimeMonitor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppLifetimeMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppLifetimeMonitor::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  const Extension* extension = host->extension();
  if (!extension || !extension->is_platform_app())
    return;

  NotifyAppStart(extension->id());
}

void AppLifetimeMonitor::OnExtensionHostDestroyed(
    content::BrowserContext* browser_context,
    extensions::ExtensionHost* host) {
  const Extension* extension = host->extension();
  if (!extension || !extension->is_platform_app())
    return;

  NotifyAppStop(extension->id());
}

void AppLifetimeMonitor::OnAppWindowRemoved(AppWindow* app_window) {
  if (!HasOtherVisibleAppWindows(app_window))
    NotifyAppDeactivated(app_window->extension_id());
}

void AppLifetimeMonitor::OnAppWindowHidden(AppWindow* app_window) {
  if (!HasOtherVisibleAppWindows(app_window))
    NotifyAppDeactivated(app_window->extension_id());
}

void AppLifetimeMonitor::OnAppWindowShown(AppWindow* app_window,
                                          bool was_hidden) {
  if (app_window->window_type() != AppWindow::WINDOW_TYPE_DEFAULT)
    return;

  // The app is being activated if this is the first window to become visible.
  if (was_hidden && !HasOtherVisibleAppWindows(app_window)) {
    NotifyAppActivated(app_window->extension_id());
  }
}

void AppLifetimeMonitor::Shutdown() {
  AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Factory::GetForBrowserContext(context_,
                                                       false /* create */);
  if (app_window_registry)
    app_window_registry->RemoveObserver(this);
}

bool AppLifetimeMonitor::HasOtherVisibleAppWindows(
    AppWindow* app_window) const {
  AppWindowRegistry::AppWindowList windows =
      AppWindowRegistry::Get(app_window->browser_context())
          ->GetAppWindowsForApp(app_window->extension_id());

  for (AppWindowRegistry::AppWindowList::const_iterator i = windows.begin();
       i != windows.end();
       ++i) {
    if (*i != app_window && !(*i)->is_hidden())
      return true;
  }
  return false;
}

void AppLifetimeMonitor::NotifyAppStart(const std::string& app_id) {
  for (auto& observer : observers_)
    observer.OnAppStart(context_, app_id);
}

void AppLifetimeMonitor::NotifyAppActivated(const std::string& app_id) {
  for (auto& observer : observers_)
    observer.OnAppActivated(context_, app_id);
}

void AppLifetimeMonitor::NotifyAppDeactivated(const std::string& app_id) {
  for (auto& observer : observers_)
    observer.OnAppDeactivated(context_, app_id);
}

void AppLifetimeMonitor::NotifyAppStop(const std::string& app_id) {
  for (auto& observer : observers_)
    observer.OnAppStop(context_, app_id);
}

}  // namespace apps
