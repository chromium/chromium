// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_desktop.h"

#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/ui_util.h"

namespace extensions {

namespace developer = api::developer_private;

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
    : DeveloperPrivateEventRouterShared(profile) {
  app_window_registry_observation_.Observe(AppWindowRegistry::Get(profile));
  toolbar_actions_model_observation_.Observe(ToolbarActionsModel::Get(profile));
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

void DeveloperPrivateEventRouter::OnToolbarPinnedActionsChanged() {
  // Currently, only enabled extensions are considered since they are the only
  // ones that have extension actions.
  // TODO(crbug.com/40280426): Since pinned info is stored as a pref, include
  // disabled extensions in this event as well.
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (const auto& extension : extensions) {
    if (ui_util::ShouldDisplayInExtensionSettings(*extension)) {
      BroadcastItemStateChanged(developer::EventType::kPinnedActionsChanged,
                                extension->id());
    }
  }
}

}  // namespace extensions
