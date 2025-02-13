// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_shared.h"

namespace extensions {

namespace developer = api::developer_private;

DeveloperPrivateEventRouterShared::DeveloperPrivateEventRouterShared(
    Profile* profile)
    : profile_(profile), event_router_(EventRouter::Get(profile_)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  error_console_observation_.Observe(ErrorConsole::Get(profile));
}

DeveloperPrivateEventRouterShared::~DeveloperPrivateEventRouterShared() =
    default;

void DeveloperPrivateEventRouterShared::AddExtensionId(
    const ExtensionId& extension_id) {
  extension_ids_.insert(extension_id);
}

void DeveloperPrivateEventRouterShared::RemoveExtensionId(
    const ExtensionId& extension_id) {
  extension_ids_.erase(extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kLoaded, extension->id());
}

void DeveloperPrivateEventRouterShared::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kUnloaded, extension->id());
}

void DeveloperPrivateEventRouterShared::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kInstalled, extension->id());
}

void DeveloperPrivateEventRouterShared::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kUninstalled,
                            extension->id());
}

void DeveloperPrivateEventRouterShared::OnErrorAdded(
    const ExtensionError* error) {
  // We don't want to handle errors thrown by extensions subscribed to these
  // events (currently only the Apps Developer Tool), because doing so risks
  // entering a loop.
  if (extension_ids_.count(error->extension_id())) {
    return;
  }

  BroadcastItemStateChanged(developer::EventType::kErrorAdded,
                            error->extension_id());
}

void DeveloperPrivateEventRouterShared::OnExtensionConfigurationChanged(
    const ExtensionId& extension_id) {
  BroadcastItemStateChanged(developer::EventType::kConfigurationChanged,
                            extension_id);
}

void DeveloperPrivateEventRouterShared::OnErrorsRemoved(
    const std::set<ExtensionId>& removed_ids) {
  for (const ExtensionId& id : removed_ids) {
    if (!extension_ids_.count(id)) {
      BroadcastItemStateChanged(developer::EventType::kErrorsRemoved, id);
    }
  }
}

void DeveloperPrivateEventRouterShared::BroadcastItemStateChanged(
    developer::EventType event_type,
    const ExtensionId& extension_id) {}

}  // namespace extensions
