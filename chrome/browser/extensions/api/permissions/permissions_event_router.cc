// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_event_router.h"

#include <memory>

#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/common/extensions/api/permissions.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

PermissionsEventRouter::PermissionsEventRouter(content::BrowserContext* context)
    : browser_context_(context),
      permissions_manager_(PermissionsManager::Get(browser_context_)),
      event_router_(EventRouter::Get(browser_context_)) {
  permissions_manager_observer_.Observe(permissions_manager_);
}

PermissionsEventRouter::~PermissionsEventRouter() = default;

void PermissionsEventRouter::Shutdown() {
  permissions_manager_observer_.Reset();
  permissions_manager_ = nullptr;
  event_router_ = nullptr;
}

void PermissionsEventRouter::OnExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsManager::UpdateReason reason) {
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = nullptr;

  switch (reason) {
    case PermissionsManager::UpdateReason::kAdded:
      histogram_value = events::PERMISSIONS_ON_ADDED;
      event_name = api::permissions::OnAdded::kEventName;
      break;
    case PermissionsManager::UpdateReason::kRemoved:
      histogram_value = events::PERMISSIONS_ON_REMOVED;
      event_name = api::permissions::OnRemoved::kEventName;
      break;
    case PermissionsManager::UpdateReason::kPolicy:
      // Explicitly don't trigger onAdded and onRemoved for policy-related
      // events.
      return;
  }

  // Trigger the onAdded and onRemoved events in the extension.
  base::Value::List event_args;
  std::unique_ptr<api::permissions::Permissions> packed_permissions =
      permissions_api_helpers::PackPermissionSet(permissions);
  event_args.Append(packed_permissions->ToValue());
  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), browser_context_);
  // `event_router_` may be null in tests.
  if (event_router_) {
    event_router_->DispatchEventToExtension(extension.id(), std::move(event));
  }
}

}  // namespace extensions
