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
  // We only observe the AppWindowRegistry for our associated Profile, so this
  // should always match.
  CHECK(profile_->IsSameOrParent(
      Profile::FromBrowserContext(app_window->browser_context())));

  AddAppWindow(app_window);
}

void AppWindowHelper::OnAppWindowRemoved(AppWindow* app_window) {
  // We only observe the AppWindowRegistry for our associated Profile, so this
  // should always match.
  CHECK(profile_->IsSameOrParent(
      Profile::FromBrowserContext(app_window->browser_context())));

  app_windows_.erase(app_window->session_id().id());
}

void AppWindowHelper::OnAppWindowActivated(AppWindow* app_window) {
  AppWindowMap::const_iterator iter =
      app_windows_.find(app_window->session_id().id());
  // We create a new entry in `app_windows_` for every AppWindow when it's
  // created, so there should always be an entry.
  CHECK(iter != app_windows_.end());

  active_window_changed_callback_.Run(iter->second.get());
}

void AppWindowHelper::AddAppWindow(AppWindow* app_window) {
  auto controller = std::make_unique<AppWindowController>(
      app_window, std::make_unique<AppBaseWindow>(app_window), profile_);
  app_windows_[app_window->session_id().id()] = std::move(controller);
}

}  // namespace extensions
