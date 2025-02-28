// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_DESKTOP_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_DESKTOP_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_shared.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/command.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class DeveloperPrivateEventRouter : public DeveloperPrivateEventRouterShared,
                                    public AppWindowRegistry::Observer,
                                    public CommandService::Observer,
                                    public ExtensionAllowlist::Observer,
                                    public ExtensionManagement::Observer,
                                    public ToolbarActionsModel::Observer,
                                    public AccountExtensionTracker::Observer {
 public:
  explicit DeveloperPrivateEventRouter(Profile* profile);

  DeveloperPrivateEventRouter(const DeveloperPrivateEventRouter&) = delete;
  DeveloperPrivateEventRouter& operator=(const DeveloperPrivateEventRouter&) =
      delete;

  ~DeveloperPrivateEventRouter() override;

 private:
  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* window) override;
  void OnAppWindowRemoved(AppWindow* window) override;

  // CommandService::Observer:
  void OnExtensionCommandAdded(const ExtensionId& extension_id,
                               const Command& added_command) override;
  void OnExtensionCommandRemoved(const ExtensionId& extension_id,
                                 const Command& removed_command) override;

  // ExtensionAllowlist::Observer
  void OnExtensionAllowlistWarningStateChanged(const ExtensionId& extension_id,
                                               bool show_warning) override;

  // ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarModelInitialized() override {}
  void OnToolbarPinnedActionsChanged() override;

  // AccountExtensionTracker::Observer:
  void OnExtensionUploadabilityChanged(const ExtensionId& id) override;
  void OnExtensionsUploadabilityChanged() override;

  // Handles a profile preference change.
  void OnProfilePrefChanged();

  // Broadcasts an event to all listeners.
  void BroadcastItemStateChanged(api::developer_private::EventType event_type,
                                 const ExtensionId& id) override;
  void BroadcastItemStateChangedHelper(
      api::developer_private::EventType event_type,
      const ExtensionId& extension_id,
      std::unique_ptr<ExtensionInfoGenerator> info_generator,
      std::vector<api::developer_private::ExtensionInfo> infos);

  base::ScopedObservation<AppWindowRegistry, AppWindowRegistry::Observer>
      app_window_registry_observation_{this};
  base::ScopedObservation<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observation_{this};
  base::ScopedObservation<CommandService, CommandService::Observer>
      command_service_observation_{this};
  base::ScopedObservation<ExtensionAllowlist, ExtensionAllowlist::Observer>
      extension_allowlist_observer_{this};
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_actions_model_observation_{this};
  base::ScopedObservation<AccountExtensionTracker,
                          AccountExtensionTracker::Observer>
      account_extension_tracker_observation_{this};

  base::WeakPtrFactory<DeveloperPrivateEventRouter> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_DESKTOP_H_
