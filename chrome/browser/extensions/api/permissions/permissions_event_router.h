// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/permissions_manager.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class EventRouter;

// The PermissionsEventRouter listens to permission change events and routes
// them to extensions. PermissionsEventRouter will only route events to
// extension processes in the same profile as this object.
class PermissionsEventRouter : public KeyedService,
                               public PermissionsManager::Observer {
 public:
  explicit PermissionsEventRouter(content::BrowserContext* context);
  PermissionsEventRouter(const PermissionsEventRouter&) = delete;
  PermissionsEventRouter& operator=(const PermissionsEventRouter&) = delete;
  ~PermissionsEventRouter() override;

  // KeyedService:
  void Shutdown() override;

  // PermissionsManager::Observer:
  void OnExtensionPermissionsUpdated(
      const Extension& extension,
      const PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<PermissionsManager> permissions_manager_;
  raw_ptr<EventRouter> event_router_;

  base::ScopedObservation<PermissionsManager, PermissionsManager::Observer>
      permissions_manager_observer_{this};

  base::WeakPtrFactory<PermissionsEventRouter> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_H_
