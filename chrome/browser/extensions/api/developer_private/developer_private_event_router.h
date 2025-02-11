// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_H_

#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/warning_service.h"

namespace extensions {

class DeveloperPrivateEventRouter : public ExtensionRegistryObserver,
                                    public ErrorConsole::Observer,
                                    public ProcessManagerObserver,
                                    public AppWindowRegistry::Observer,
                                    public CommandService::Observer,
                                    public ExtensionPrefsObserver,
                                    public ExtensionAllowlist::Observer,
                                    public ExtensionManagement::Observer,
                                    public WarningService::Observer,
                                    public PermissionsManager::Observer,
                                    public ToolbarActionsModel::Observer,
                                    public AccountExtensionTracker::Observer {
 public:
  static std::unique_ptr<api::developer_private::ProfileInfo> CreateProfileInfo(
      Profile* profile);

  static api::developer_private::UserSiteSettings ConvertToUserSiteSettings(
      const PermissionsManager::UserPermissionsSettings& settings);

  explicit DeveloperPrivateEventRouter(Profile* profile);

  DeveloperPrivateEventRouter(const DeveloperPrivateEventRouter&) = delete;
  DeveloperPrivateEventRouter& operator=(const DeveloperPrivateEventRouter&) =
      delete;

  ~DeveloperPrivateEventRouter() override;

  // Add or remove an ID to the list of extensions subscribed to events.
  void AddExtensionId(const ExtensionId& extension_id);
  void RemoveExtensionId(const ExtensionId& extension_id);

  // Called when the configuration (such as user preferences) for an extension
  // has changed in a way that may affect the chrome://extensions UI.
  void OnExtensionConfigurationChanged(const ExtensionId& extension_id);

 private:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // ErrorConsole::Observer:
  void OnErrorAdded(const ExtensionError* error) override;
  void OnErrorsRemoved(const std::set<ExtensionId>& extension_ids) override;

  // ProcessManagerObserver:
  void OnExtensionFrameRegistered(
      const ExtensionId& extension_id,
      content::RenderFrameHost* render_frame_host) override;
  void OnExtensionFrameUnregistered(
      const ExtensionId& extension_id,
      content::RenderFrameHost* render_frame_host) override;
  void OnStartedTrackingServiceWorkerInstance(
      const WorkerId& worker_id) override;
  void OnStoppedTrackingServiceWorkerInstance(
      const WorkerId& worker_id) override;

  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* window) override;
  void OnAppWindowRemoved(AppWindow* window) override;

  // CommandService::Observer:
  void OnExtensionCommandAdded(const ExtensionId& extension_id,
                               const Command& added_command) override;
  void OnExtensionCommandRemoved(const ExtensionId& extension_id,
                                 const Command& removed_command) override;

  // ExtensionPrefsObserver:
  void OnExtensionDisableReasonsChanged(
      const ExtensionId& extension_id,
      DisableReasonSet disable_reasons) override;
  void OnExtensionRuntimePermissionsChanged(
      const ExtensionId& extension_id) override;

  // ExtensionAllowlist::Observer
  void OnExtensionAllowlistWarningStateChanged(const ExtensionId& extension_id,
                                               bool show_warning) override;

  // ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // WarningService::Observer:
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override;

  // PermissionsManager::Observer:
  void OnUserPermissionsSettingsChanged(
      const PermissionsManager::UserPermissionsSettings& settings) override;
  void OnExtensionPermissionsUpdated(
      const Extension& extension,
      const PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override;

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
                                 const ExtensionId& id);
  void BroadcastItemStateChangedHelper(
      api::developer_private::EventType event_type,
      const ExtensionId& extension_id,
      std::unique_ptr<ExtensionInfoGenerator> info_generator,
      std::vector<api::developer_private::ExtensionInfo> infos);

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<ErrorConsole, ErrorConsole::Observer>
      error_console_observation_{this};
  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};
  base::ScopedObservation<AppWindowRegistry, AppWindowRegistry::Observer>
      app_window_registry_observation_{this};
  base::ScopedObservation<WarningService, WarningService::Observer>
      warning_service_observation_{this};
  base::ScopedObservation<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observation_{this};
  base::ScopedObservation<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observation_{this};
  base::ScopedObservation<CommandService, CommandService::Observer>
      command_service_observation_{this};
  base::ScopedObservation<ExtensionAllowlist, ExtensionAllowlist::Observer>
      extension_allowlist_observer_{this};
  base::ScopedObservation<PermissionsManager, PermissionsManager::Observer>
      permissions_manager_observation_{this};
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_actions_model_observation_{this};
  base::ScopedObservation<AccountExtensionTracker,
                          AccountExtensionTracker::Observer>
      account_extension_tracker_observation_{this};

  raw_ptr<Profile> profile_;

  raw_ptr<EventRouter> event_router_;

  // The set of IDs of the Extensions that have subscribed to DeveloperPrivate
  // events. Since the only consumer of the DeveloperPrivate API is currently
  // the Apps Developer Tool (which replaces the chrome://extensions page), we
  // don't want to send information about the subscribing extension in an
  // update. In particular, we want to avoid entering a loop, which could happen
  // when, e.g., the Apps Developer Tool throws an error.
  std::set<ExtensionId> extension_ids_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<DeveloperPrivateEventRouter> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_H_
