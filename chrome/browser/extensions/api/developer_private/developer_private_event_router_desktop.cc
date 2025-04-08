// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_desktop.h"

#include "chrome/browser/extensions/api/developer_private/profile_info_generator.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_util.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/ui_util.h"

namespace extensions {

namespace developer = api::developer_private;

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
    : DeveloperPrivateEventRouterShared(profile) {
  app_window_registry_observation_.Observe(AppWindowRegistry::Get(profile));
  extension_management_observation_.Observe(
      ExtensionManagementFactory::GetForBrowserContext(profile));
  command_service_observation_.Observe(CommandService::Get(profile));
  extension_allowlist_observer_.Observe(
      ExtensionSystem::Get(profile)->extension_service()->allowlist());
  toolbar_actions_model_observation_.Observe(ToolbarActionsModel::Get(profile));

  if (switches::IsExtensionsExplicitBrowserSigninEnabled()) {
    account_extension_tracker_observation_.Observe(
        AccountExtensionTracker::Get(profile));
  }
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

void DeveloperPrivateEventRouter::OnExtensionCommandAdded(
    const ExtensionId& extension_id,
    const Command& added_command) {
  BroadcastItemStateChanged(developer::EventType::kCommandAdded, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionCommandRemoved(
    const ExtensionId& extension_id,
    const Command& removed_command) {
  BroadcastItemStateChanged(developer::EventType::kCommandRemoved,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionAllowlistWarningStateChanged(
    const ExtensionId& extension_id,
    bool show_warning) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionManagementSettingsChanged() {
  base::Value::List args;
  args.Append(CreateProfileInfo(profile_).ToValue());

  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
      developer::OnProfileStateChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));
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

void DeveloperPrivateEventRouter::OnExtensionUploadabilityChanged(
    const ExtensionId& id) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, id);
}

void DeveloperPrivateEventRouter::OnExtensionsUploadabilityChanged() {
  const ExtensionSet extensions =
      ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet();
  for (const auto& extension : extensions) {
    if (sync_util::ShouldSync(profile_, extension.get())) {
      BroadcastItemStateChanged(developer::EventType::kPrefsChanged,
                                extension->id());
    }
  }
}

}  // namespace extensions
