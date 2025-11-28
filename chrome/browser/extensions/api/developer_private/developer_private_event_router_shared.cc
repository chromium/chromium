// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_shared.h"

#include <memory>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/developer_private/profile_info_generator.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/warning_service.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/command.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace developer = api::developer_private;

// static
developer::UserSiteSettings
DeveloperPrivateEventRouterShared::ConvertToUserSiteSettings(
    const PermissionsManager::UserPermissionsSettings& settings) {
  api::developer_private::UserSiteSettings user_site_settings;
  user_site_settings.permitted_sites.reserve(settings.permitted_sites.size());
  for (const auto& origin : settings.permitted_sites) {
    user_site_settings.permitted_sites.push_back(origin.Serialize());
  }

  user_site_settings.restricted_sites.reserve(settings.restricted_sites.size());
  for (const auto& origin : settings.restricted_sites) {
    user_site_settings.restricted_sites.push_back(origin.Serialize());
  }

  return user_site_settings;
}

DeveloperPrivateEventRouterShared::DeveloperPrivateEventRouterShared(
    Profile* profile)
    : profile_(profile), event_router_(EventRouter::Get(profile_)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  error_console_observation_.Observe(ErrorConsole::Get(profile));
  process_manager_observation_.Observe(ProcessManager::Get(profile));
  extension_prefs_observation_.Observe(ExtensionPrefs::Get(profile));
  warning_service_observation_.Observe(WarningService::Get(profile));
  permissions_manager_observation_.Observe(PermissionsManager::Get(profile));
  extension_management_observation_.Observe(
      ExtensionManagementFactory::GetForBrowserContext(profile));
  extension_allowlist_observer_.Observe(ExtensionAllowlist::Get(profile));
  command_service_observation_.Observe(CommandService::Get(profile));

  pref_change_registrar_.Init(profile->GetPrefs());
  // The unretained is safe, since the PrefChangeRegistrar unregisters the
  // callback on destruction.
  pref_change_registrar_.Add(
      prefs::kExtensionsUIDeveloperMode,
      base::BindRepeating(
          &DeveloperPrivateEventRouterShared::OnProfilePrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      kMV2DeprecationWarningAcknowledgedGloballyPref.name,
      base::BindRepeating(
          &DeveloperPrivateEventRouterShared::OnProfilePrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      kMV2DeprecationDisabledAcknowledgedGloballyPref.name,
      base::BindRepeating(
          &DeveloperPrivateEventRouterShared::OnProfilePrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      kMV2DeprecationUnsupportedAcknowledgedGloballyPref.name,
      base::BindRepeating(
          &DeveloperPrivateEventRouterShared::OnProfilePrefChanged,
          base::Unretained(this)));

  if (switches::IsExtensionsExplicitBrowserSigninEnabled()) {
    account_extension_tracker_observation_.Observe(
        AccountExtensionTracker::Get(profile));
  }
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

void DeveloperPrivateEventRouterShared::OnExtensionFrameRegistered(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host) {
  BroadcastItemStateChanged(developer::EventType::kViewRegistered,
                            extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionFrameUnregistered(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host) {
  BroadcastItemStateChanged(developer::EventType::kViewUnregistered,
                            extension_id);
}

void DeveloperPrivateEventRouterShared::OnStartedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  BroadcastItemStateChanged(developer::EventType::kServiceWorkerStarted,
                            worker_id.extension_id);
}

void DeveloperPrivateEventRouterShared::OnStoppedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  BroadcastItemStateChanged(developer::EventType::kServiceWorkerStopped,
                            worker_id.extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionDisableReasonsChanged(
    const ExtensionId& extension_id,
    DisableReasonSet disable_reasons) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionRuntimePermissionsChanged(
    const ExtensionId& extension_id) {
  BroadcastItemStateChanged(developer::EventType::kPermissionsChanged,
                            extension_id);
}

void DeveloperPrivateEventRouterShared::ExtensionWarningsChanged(
    const ExtensionIdSet& affected_extensions) {
  for (const ExtensionId& id : affected_extensions) {
    BroadcastItemStateChanged(developer::EventType::kWarningsChanged, id);
  }
}

void DeveloperPrivateEventRouterShared::OnUserPermissionsSettingsChanged(
    const PermissionsManager::UserPermissionsSettings& settings) {
  developer::UserSiteSettings user_site_settings =
      ConvertToUserSiteSettings(settings);
  base::Value::List args;
  args.Append(user_site_settings.ToValue());

  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_USER_SITE_SETTINGS_CHANGED,
      developer::OnUserSiteSettingsChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateEventRouterShared::OnExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsManager::UpdateReason reason) {
  BroadcastItemStateChanged(developer::EventType::kPermissionsChanged,
                            extension.id());
}

void DeveloperPrivateEventRouterShared::OnExtensionManagementSettingsChanged() {
  base::Value::List args;
  args.Append(CreateProfileInfo(profile_).ToValue());

  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
      developer::OnProfileStateChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateEventRouterShared::OnExtensionAllowlistWarningStateChanged(
    const ExtensionId& extension_id,
    bool show_warning) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionCommandAdded(
    const ExtensionId& extension_id,
    const std::string& command_name) {
  BroadcastItemStateChanged(developer::EventType::kCommandAdded, extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionCommandRemoved(
    const ExtensionId& extension_id,
    const std::string& command_name) {
  BroadcastItemStateChanged(developer::EventType::kCommandRemoved,
                            extension_id);
}

void DeveloperPrivateEventRouterShared::OnExtensionUploadabilityChanged(
    const ExtensionId& id) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, id);
}

void DeveloperPrivateEventRouterShared::OnExtensionsUploadabilityChanged() {
  const ExtensionSet extensions =
      ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet();
  for (const auto& extension : extensions) {
    if (sync_util::ShouldSync(profile_, extension.get())) {
      BroadcastItemStateChanged(developer::EventType::kPrefsChanged,
                                extension->id());
    }
  }
}

void DeveloperPrivateEventRouterShared::OnProfilePrefChanged() {
  base::Value::List args;
  args.Append(CreateProfileInfo(profile_).ToValue());
  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
      developer::OnProfileStateChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));

  // The following properties are updated when dev mode is toggled.
  //   - error_collection.is_enabled
  //   - error_collection.is_active
  //   - runtime_errors
  //   - manifest_errors
  //   - install_warnings
  // An alternative approach would be to factor out the dev mode state from the
  // above properties and allow the UI control what happens when dev mode
  // changes. If the UI rendering performance is an issue, instead of replacing
  // the entire extension info, a diff of the old and new extension info can be
  // made by the UI and only perform a partial update of the extension info.
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (const auto& extension : extensions) {
    BroadcastItemStateChanged(developer::EventType::kPrefsChanged,
                              extension->id());
  }
}

void DeveloperPrivateEventRouterShared::BroadcastItemStateChanged(
    developer::EventType event_type,
    const ExtensionId& extension_id) {
  auto info_generator = std::make_unique<ExtensionInfoGenerator>(profile_);
  ExtensionInfoGenerator* info_generator_weak = info_generator.get();
  info_generator_weak->CreateExtensionInfo(
      extension_id,
      base::BindOnce(
          &DeveloperPrivateEventRouterShared::BroadcastItemStateChangedHelper,
          weak_factory_.GetWeakPtr(), event_type, extension_id,
          std::move(info_generator)));
}

void DeveloperPrivateEventRouterShared::BroadcastItemStateChangedHelper(
    developer::EventType event_type,
    const ExtensionId& extension_id,
    std::unique_ptr<ExtensionInfoGenerator> info_generator,
    ExtensionInfoGenerator::ExtensionInfoList infos) {
  DCHECK_LE(infos.size(), 1u);

  developer::EventData event_data;
  event_data.event_type = event_type;
  event_data.item_id = extension_id;
  if (!infos.empty()) {
    event_data.extension_info = std::move(infos[0]);
  }

  base::Value::List args;
  args.Append(event_data.ToValue());
  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_ITEM_STATE_CHANGED,
      developer::OnItemStateChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));
}

}  // namespace extensions
