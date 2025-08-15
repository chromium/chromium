// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/app_window_helper.h"

#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/api/tabs/app_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/app_window/app_window.h"

namespace extensions {

AppWindowHelper::AppWindowHelper(Profile* profile,
                                 ActiveWindowChangedCallback callback)
    : profile_(profile), active_window_changed_callback_(std::move(callback)) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile);

  app_registry_observation_.Observe(registry);

  for (AppWindow* app_window : registry->app_windows()) {
    AddAppWindow(app_window);
  }
}

AppWindowHelper::~AppWindowHelper() = default;

void AppWindowHelper::OnAppWindowAdded(AppWindow* app_window) {
  // TODO(devlin): Why this check? This only observes the AppWindowRegistry
  // associated with the profile, so I'd expect this to always be true?
  // Ditto for the checks below.
  if (!profile_->IsSameOrParent(
          Profile::FromBrowserContext(app_window->browser_context()))) {
    return;
  }

  AddAppWindow(app_window);
}

void AppWindowHelper::OnAppWindowRemoved(AppWindow* app_window) {
  if (!profile_->IsSameOrParent(
          Profile::FromBrowserContext(app_window->browser_context()))) {
    return;
  }

  app_windows_.erase(app_window->session_id().id());
}

void AppWindowHelper::OnAppWindowActivated(AppWindow* app_window) {
  // TODO(devlin): Can this ever not be found? We create a new `app_windows_`
  // entry for each AppWindow, so there should always be one....
  AppWindowMap::const_iterator iter =
      app_windows_.find(app_window->session_id().id());
  AppWindowController* controller =
      iter != app_windows_.end() ? iter->second.get() : nullptr;
  active_window_changed_callback_.Run(controller);
}

void AppWindowHelper::AddAppWindow(AppWindow* app_window) {
  auto controller = std::make_unique<AppWindowController>(
      app_window, std::make_unique<AppBaseWindow>(app_window), profile_);
  app_windows_[app_window->session_id().id()] = std::move(controller);
}

}  // namespace extensions
