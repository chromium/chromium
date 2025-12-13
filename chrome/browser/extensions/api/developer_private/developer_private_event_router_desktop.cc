// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_desktop.h"

#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/app_window/app_window.h"

namespace extensions {

namespace developer = api::developer_private;

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
    : DeveloperPrivateEventRouterShared(profile) {
  app_window_registry_observation_.Observe(AppWindowRegistry::Get(profile));
}

DeveloperPrivateEventRouter::~DeveloperPrivateEventRouter() = default;

void DeveloperPrivateEventRouter::OnAppWindowAdded(AppWindow* window) {
  BroadcastItemStateChanged(developer::EventType::kViewRegistered,
                            window->extension_id());
}

void DeveloperPrivateEventRouter::OnAppWindowRemoved(AppWindow* window) {
  BroadcastItemStateChanged(developer::EventType::kViewUnregistered,
                            window->extension_id());
}

}  // namespace extensions
