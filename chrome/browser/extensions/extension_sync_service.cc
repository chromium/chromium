// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/one_shot_event.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/sync/model/sync_change.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"

using extensions::AccountExtensionTracker;
using extensions::AppSorting;
using extensions::Extension;
using extensions::ExtensionManagement;
using extensions::ExtensionManagementFactory;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::ExtensionSyncData;
using extensions::ExtensionSystem;
using extensions::SyncBundle;

namespace {
// Returns true if the sync type of |extension| matches |type|.
bool IsCorrectSyncType(const Extension& extension, syncer::DataType type) {
  return (type == syncer::EXTENSIONS && extension.is_extension()) ||
         (type == syncer::APPS && extension.is_app());
}

// Predicate for PendingExtensionManager.
// TODO(crbug.com/41401013): The !is_theme check should be unnecessary after all
// the bad data from crbug.com/558299 has been cleaned up.
bool ShouldAllowInstall(const Extension* extension,
                        content::BrowserContext* context) {
  return !extension->is_theme() &&
         extensions::util::ShouldSync(extension, context);
}

std::map<std::string, syncer::SyncData> ToSyncerSyncDataMap(
    const std::vector<ExtensionSyncData>& data) {
  std::map<std::string, syncer::SyncData> result;
  for (const ExtensionSyncData& item : data)
    result[item.id()] = item.GetSyncData();
  return result;
}

syncer::SyncDataList ToSyncerSyncDataList(
    const std::vector<ExtensionSyncData>& data) {
  syncer::SyncDataList result;
  result.reserve(data.size());
  for (const ExtensionSyncData& item : data)
    result.push_back(item.GetSyncData());
  return result;
}

static_assert(extensions::disable_reason::DISABLE_REASON_LAST == (1LL << 25),
              "Please consider whether your new disable reason should be"
              " syncable, and if so update this bitmask accordingly!");
const int kKnownSyncableDisableReasons =
    extensions::disable_reason::DISABLE_USER_ACTION |
    extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE |
    extensions::disable_reason::DISABLE_SIDELOAD_WIPEOUT |
    extensions::disable_reason::DISABLE_GREYLIST |
    extensions::disable_reason::DISABLE_REMOTE_INSTALL;
const int kAllKnownDisableReasons =
    extensions::disable_reason::DISABLE_REASON_LAST - 1;
// We also include any future bits for newer clients that added another disable
// reason.
const int kSyncableDisableReasons =
    kKnownSyncableDisableReasons | ~kAllKnownDisableReasons;

}  // namespace

struct ExtensionSyncService::PendingUpdate {
  PendingUpdate() : grant_permissions_and_reenable(false) {}
  PendingUpdate(const base::Version& version,
                bool grant_permissions_and_reenable)
    : version(version),
      grant_permissions_and_reenable(grant_permissions_and_reenable) {}

  base::Version version;
  bool grant_permissions_and_reenable;
};

ExtensionSyncService::ExtensionSyncService(Profile* profile)
    : profile_(profile),
      system_(ExtensionSystem::Get(profile_)),
      ignore_updates_(false),
      flare_(sync_start_util::GetFlareForSyncableService(profile->GetPath())) {
  registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  prefs_observation_.Observe(ExtensionPrefs::Get(profile_));
}

ExtensionSyncService::~ExtensionSyncService() {
}

// static
ExtensionSyncService* ExtensionSyncService::Get(
    content::BrowserContext* context) {
  return ExtensionSyncServiceFactory::GetForBrowserContext(context);
}

// static
bool ExtensionSyncService::ShouldSync(content::BrowserContext* browser_context,
                                      const Extension& extension) {
  // Themes are handled by the ThemeSyncableService.
  return extensions::util::ShouldSync(&extension, browser_context) &&
         !extension.is_theme() &&
         !extensions::blocklist_prefs::IsExtensionBlocklisted(
             extension.id(), ExtensionPrefs::Get(browser_context));
}

void ExtensionSyncService::SyncExtensionChangeIfNeeded(
    const Extension& extension) {
  if (ignore_updates_ || !ShouldSync(profile_, extension)) {
    return;
  }

  syncer::DataType type =
      extension.is_app() ? syncer::APPS : syncer::EXTENSIONS;
  SyncBundle* bundle = GetSyncBundle(type);
  if (bundle->IsSyncing()) {
    bundle->PushSyncAddOrUpdate(extension.id(),
                                CreateSyncData(extension).GetSyncData());
    DCHECK(!ExtensionPrefs::Get(profile_)->NeedsSync(extension.id()));
  } else {
    ExtensionPrefs::Get(profile_)->SetNeedsSync(extension.id(), true);
    if (system_->is_ready() && !flare_.is_null())
      flare_.Run(type);  // Tell sync to start ASAP.
  }
}

void ExtensionSyncService::WaitUntilReadyToSync(base::OnceClosure done) {
  // Wait for the extension system to be ready.
  system_->ready().Post(FROM_HERE, std::move(done));
}

std::optional<syncer::ModelError>
ExtensionSyncService::MergeDataAndStartSyncing(
    syncer::DataType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  CHECK(sync_processor.get());
  LOG_IF(FATAL, type != syncer::EXTENSIONS && type != syncer::APPS)
      << "Got " << type << " DataType";

  SyncBundle* bundle = GetSyncBundle(type);
  bundle->StartSyncing(std::move(sync_processor));

  // Apply the initial sync data, filtering out any items where we have more
  // recent local changes. Also tell the SyncBundle the extension IDs.
  for (const syncer::SyncData& sync_data : initial_sync_data) {
    std::unique_ptr<ExtensionSyncData> extension_sync_data(
        ExtensionSyncData::CreateFromSyncData(sync_data));
    // If the extension has local state that needs to be synced, ignore this
    // change (we assume the local state is more recent).
    if (extension_sync_data &&
        !ExtensionPrefs::Get(profile_)->NeedsSync(extension_sync_data->id())) {
      ApplySyncData(*extension_sync_data);
    }
  }

  // Now push the local state to sync.
  // Note: We'd like to only send out changes for extensions which have
  // NeedsSync set. However, we can't tell if our changes ever made it to the
  // sync server (they might not e.g. when there's a temporary auth error), so
  // we couldn't safely clear the flag. So just send out everything and let the
  // sync client handle no-op changes.
  std::vector<ExtensionSyncData> data_list = GetLocalSyncDataList(type);
  bundle->PushSyncDataMap(ToSyncerSyncDataMap(data_list));

  for (const ExtensionSyncData& data : data_list)
    ExtensionPrefs::Get(profile_)->SetNeedsSync(data.id(), false);

  if (type == syncer::APPS)
    system_->app_sorting()->FixNTPOrdinalCollisions();

  return std::nullopt;
}

void ExtensionSyncService::StopSyncing(syncer::DataType type) {
  GetSyncBundle(type)->Reset();
}

syncer::SyncDataList ExtensionSyncService::GetAllSyncDataForTesting(
    syncer::DataType type) const {
  const SyncBundle* bundle = GetSyncBundle(type);
  if (!bundle->IsSyncing())
    return syncer::SyncDataList();

  std::vector<ExtensionSyncData> sync_data_list = GetLocalSyncDataList(type);

  // Add pending data (where the local extension is not installed yet).
  std::vector<ExtensionSyncData> pending_extensions =
      bundle->GetPendingExtensionData();
  sync_data_list.insert(sync_data_list.begin(),
                        pending_extensions.begin(),
                        pending_extensions.end());

  return ToSyncerSyncDataList(sync_data_list);
}

std::optional<syncer::ModelError> ExtensionSyncService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (const syncer::SyncChange& sync_change : change_list) {
    std::unique_ptr<ExtensionSyncData> extension_sync_data(
        ExtensionSyncData::CreateFromSyncChange(sync_change));
    if (extension_sync_data)
      ApplySyncData(*extension_sync_data);
  }

  system_->app_sorting()->FixNTPOrdinalCollisions();

  return std::nullopt;
}

base::WeakPtr<syncer::SyncableService> ExtensionSyncService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ExtensionSyncData ExtensionSyncService::CreateSyncData(
    const Extension& extension) const {
  const std::string& id = extension.id();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  int disable_reasons =
      extension_prefs->GetDisableReasons(id) & kSyncableDisableReasons;
  // Note that we're ignoring the enabled state during ApplySyncData (we check
  // for the existence of disable reasons instead), we're just setting it here
  // for older Chrome versions (<M48).
  bool enabled = (disable_reasons == extensions::disable_reason::DISABLE_NONE);
  if (extensions::blocklist_prefs::IsExtensionBlocklisted(id,
                                                          extension_prefs)) {
    enabled = false;
    NOTREACHED_IN_MIGRATION()
        << "Blocklisted extensions should not be getting synced.";
  }

  bool incognito_enabled = extensions::util::IsIncognitoEnabled(id, profile_);
  bool remote_install = extension_prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_REMOTE_INSTALL);
  AppSorting* app_sorting = system_->app_sorting();

  ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);

  const GURL update_url =
      extension_management->GetEffectiveUpdateURL(extension);
  ExtensionSyncData result =
      extension.is_app()
          ? ExtensionSyncData(
                extension, enabled, disable_reasons, incognito_enabled,
                remote_install, update_url,
                app_sorting->GetAppLaunchOrdinal(id),
                app_sorting->GetPageOrdinal(id),
                extensions::GetLaunchTypePrefValue(extension_prefs, id))
          : ExtensionSyncData(extension, enabled, disable_reasons,
                              incognito_enabled, remote_install, update_url);

  // If there's a pending update, send the new version to sync instead of the
  // installed one.
  auto it = pending_updates_.find(id);
  if (it != pending_updates_.end()) {
    const base::Version& version = it->second.version;
    // If we have a pending version, it should be newer than the installed one.
    DCHECK_EQ(-1, extension.version().CompareTo(version));
    result.set_version(version);
    // If we'll re-enable the extension once it's updated, also send that back
    // to sync.
    if (it->second.grant_permissions_and_reenable)
      result.set_enabled(true);
  }
  return result;
}

void ExtensionSyncService::ApplySyncData(
    const ExtensionSyncData& extension_sync_data) {
  const std::string& id = extension_sync_data.id();

  // Remove all deprecated bookmark apps immediately, as they aren't loaded into
  // the extensions system at all (and thus cannot be looked up).
  if (extension_sync_data.is_deprecated_bookmark_app()) {
    GetSyncBundle(syncer::APPS)->ApplySyncData(extension_sync_data);
    GetSyncBundle(syncer::APPS)
        ->PushSyncDeletion(id, extension_sync_data.GetSyncData());
    return;
  }

  // Note: |extension| may be null if it hasn't been installed yet.
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetInstalledExtension(id);
  // If there is an existing extension that shouldn't be sync'd, don't
  // apply this sync data. This can happen if the local version of an
  // extension is default-installed, but the sync server has data from another
  // (non-default-installed) installation. We can't apply the sync data because
  // it would always override the local state (which would never get sync'd).
  // See crbug.com/731824.
  if (extension && !ShouldSync(profile_, *extension)) {
    return;
  }

  // Ignore any pref change notifications etc. while we're applying incoming
  // sync data, so that we don't end up notifying ourselves.
  base::AutoReset<bool> ignore_updates(&ignore_updates_, true);

  syncer::DataType type =
      extension_sync_data.is_app() ? syncer::APPS : syncer::EXTENSIONS;
  SyncBundle* bundle = GetSyncBundle(type);
  DCHECK(bundle->IsSyncing());
  if (extension && !IsCorrectSyncType(*extension, type)) {
    // The installed item isn't the same type as the sync data item, so we need
    // to remove the sync data item; otherwise it will be a zombie that will
    // keep coming back even if the installed item with this id is uninstalled.
    // First tell the bundle about the extension, so that it won't just ignore
    // the deletion, then push the deletion.
    bundle->ApplySyncData(extension_sync_data);
    bundle->PushSyncDeletion(id, extension_sync_data.GetSyncData());
    return;
  }

  // Forward to the bundle. This will just update the list of synced extensions.
  bundle->ApplySyncData(extension_sync_data);

  // Handle uninstalls first.
  if (extension_sync_data.uninstalled()) {
    std::u16string error;
    bool uninstalled = true;
    if (!extension) {
      error = u"Unknown extension";
      uninstalled = false;
    } else {
      uninstalled = extension_service()->UninstallExtension(
          id, extensions::UNINSTALL_REASON_SYNC, &error);
    }

    if (!uninstalled) {
      LOG(WARNING) << "Failed to uninstall extension with id '" << id
                   << "' from sync: " << error;
    }
    return;
  }

  // Extension from sync was uninstalled by the user as an external extension.
  // Honor user choice and skip installation/enabling.
  // TODO(treib): Should we still apply pref changes?
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  if (extension_prefs->IsExternalExtensionUninstalled(id)) {
    LOG(WARNING) << "Extension with id " << id
                 << " from sync was uninstalled as external extension";
    return;
  }

  enum {
    NOT_INSTALLED,
    INSTALLED_OUTDATED,
    INSTALLED_MATCHING,
    INSTALLED_NEWER,
  } state = NOT_INSTALLED;
  if (extension) {
    switch (extension->version().CompareTo(extension_sync_data.version())) {
      case -1: state = INSTALLED_OUTDATED; break;
      case 0: state = INSTALLED_MATCHING; break;
      case 1: state = INSTALLED_NEWER; break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Figure out the resulting set of disable reasons.
  int disable_reasons = extension_prefs->GetDisableReasons(id);

  // Chrome versions M37-M44 used |extension_sync_data.remote_install()| to tag
  // not-yet-approved remote installs. It's redundant now that disable reasons
  // are synced (DISABLE_REMOTE_INSTALL should be among them already), but some
  // old sync data may still be around, and it doesn't hurt to add the reason.
  // TODO(crbug.com/41240022): Deprecate and eventually remove |remote_install|.
  if (extension_sync_data.remote_install())
    disable_reasons |= extensions::disable_reason::DISABLE_REMOTE_INSTALL;

  // Add/remove disable reasons based on the incoming sync data.
  int incoming_disable_reasons = extension_sync_data.disable_reasons();
  if (!!incoming_disable_reasons == extension_sync_data.enabled()) {
    // The enabled flag disagrees with the presence of disable reasons. This
    // must either come from an old (<M45) client which doesn't sync disable
    // reasons, or the extension is blocklisted (which doesn't have a
    // corresponding disable reason).
    // Update |disable_reasons| based on the enabled flag.
    if (extension_sync_data.enabled())
      disable_reasons &= ~kKnownSyncableDisableReasons;
    else  // Assume the extension was likely disabled by the user.
      disable_reasons |= extensions::disable_reason::DISABLE_USER_ACTION;
  } else {
    // Replace the syncable disable reasons:
    // 1. Remove any syncable disable reasons we might have.
    disable_reasons &= ~kSyncableDisableReasons;
    // 2. Add the incoming reasons. Mask with |kSyncableDisableReasons|, because
    //    Chrome M45-M47 also wrote local disable reasons to sync, and we don't
    //    want those.
    disable_reasons |= incoming_disable_reasons & kSyncableDisableReasons;
  }

  // Enable/disable the extension.
  bool should_be_enabled =
      (disable_reasons == extensions::disable_reason::DISABLE_NONE);
  bool reenable_after_update = false;
  if (should_be_enabled && !extension_service()->IsExtensionEnabled(id)) {
    if (extension) {
      // Only grant permissions if the sync data explicitly sets the disable
      // reasons to extensions::disable_reason::DISABLE_NONE (as opposed to the
      // legacy
      // (<M45) case where they're not set at all), and if the version from sync
      // matches our local one.
      bool grant_permissions = extension_sync_data.supports_disable_reasons() &&
                               (state == INSTALLED_MATCHING);
      if (grant_permissions)
        extension_service()->GrantPermissions(extension);

      // Only enable if the extension has all required permissions.
      // (Even if the version doesn't match - if the new version needs more
      // permissions, it'll get disabled after the update.)
      bool has_all_permissions =
          grant_permissions ||
          !extensions::PermissionMessageProvider::Get()->IsPrivilegeIncrease(
              *extension_prefs->GetGrantedPermissions(id),
              extension->permissions_data()->active_permissions(),
              extension->GetType());
      if (has_all_permissions)
        extension_service()->EnableExtension(id);
      else if (extension_sync_data.supports_disable_reasons())
        reenable_after_update = true;
    } else {
      // The extension is not installed yet. Set it to enabled; we'll check for
      // permission increase (more accurately, for a version change) when it's
      // actually installed.
      extension_service()->EnableExtension(id);
    }
  } else if (!should_be_enabled) {
    // Note that |disable_reasons| includes any pre-existing reasons that
    // weren't explicitly removed above.
    if (extension_service()->IsExtensionEnabled(id))
      extension_service()->DisableExtension(id, disable_reasons);
    else  // Already disabled, just replace the disable reasons.
      extension_prefs->ReplaceDisableReasons(id, disable_reasons);
  }

  // Update the incognito flag.
  extensions::util::SetIsIncognitoEnabled(
      id, profile_, extension_sync_data.incognito_enabled());
  extension = nullptr;  // No longer safe to use.

  // Set app-specific data.
  if (extension_sync_data.is_app()) {
    // The corresponding validation of this value during ExtensionSyncData
    // population is in ExtensionSyncData::ToAppSpecifics.
    if (extension_sync_data.launch_type() >= extensions::LAUNCH_TYPE_FIRST &&
        extension_sync_data.launch_type() < extensions::NUM_LAUNCH_TYPES) {
      extensions::SetLaunchType(
          profile_, id, extension_sync_data.launch_type());
    }

    if (extension_sync_data.app_launch_ordinal().IsValid() &&
        extension_sync_data.page_ordinal().IsValid()) {
      AppSorting* app_sorting = system_->app_sorting();
      app_sorting->SetAppLaunchOrdinal(
          id, extension_sync_data.app_launch_ordinal());
      app_sorting->SetPageOrdinal(id, extension_sync_data.page_ordinal());
    }
  }

  // Notify the AccountExtensionTracker of an incoming extension via sync.
  if (!extension_sync_data.is_app() && state != NOT_INSTALLED) {
    AccountExtensionTracker::Get(profile_)->OnExtensionSyncDataApplied(id);
  }

  // Finally, trigger installation/update as required.
  bool check_for_updates = false;
  if (state == INSTALLED_OUTDATED) {
    // If the extension is installed but outdated, store the new version.
    pending_updates_[id] =
        PendingUpdate(extension_sync_data.version(), reenable_after_update);
    check_for_updates = true;
  } else if (state == NOT_INSTALLED) {
    if (!extension_service()->pending_extension_manager()->AddFromSync(
            id,
            extension_sync_data.update_url(),
            extension_sync_data.version(),
            ShouldAllowInstall,
            extension_sync_data.remote_install())) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      // This means that the extension is already pending installation, with a
      // non-INTERNAL location.  Add to pending_sync_data, even though it will
      // never be removed (we'll never install a syncable version of the
      // extension), so that GetAllSyncData() continues to send it.
    }
    // Track pending extensions so that we can return them in GetAllSyncData().
    bundle->AddPendingExtensionData(extension_sync_data);
    check_for_updates = true;
  }

  if (check_for_updates)
    extension_service()->CheckForUpdatesSoon();
}

void ExtensionSyncService::SetSyncStartFlareForTesting(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

void ExtensionSyncService::DeleteThemeDoNotUse(const Extension& theme) {
  DCHECK(theme.is_theme());
  GetSyncBundle(syncer::EXTENSIONS)->PushSyncDeletion(
      theme.id(), CreateSyncData(theme).GetSyncData());
}

extensions::ExtensionService* ExtensionSyncService::extension_service() const {
  return system_->extension_service();
}

void ExtensionSyncService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  DCHECK_EQ(profile_, browser_context);
  // Clear pending version if the installed one has caught up.
  auto it = pending_updates_.find(extension->id());
  if (it != pending_updates_.end()) {
    int compare_result = extension->version().CompareTo(it->second.version);
    if (compare_result == 0 && it->second.grant_permissions_and_reenable) {
      // The call to SyncExtensionChangeIfNeeded below will take care of syncing
      // changes to this extension, so we don't want to trigger sync activity
      // from the call to GrantPermissionsAndEnableExtension.
      base::AutoReset<bool> ignore_updates(&ignore_updates_, true);
      extension_service()->GrantPermissionsAndEnableExtension(extension);
    }
    if (compare_result >= 0)
      pending_updates_.erase(it);
  }
  SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionSyncService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK_EQ(profile_, browser_context);
  // Don't bother syncing if the extension will be re-installed momentarily.
  if (reason == extensions::UNINSTALL_REASON_REINSTALL ||
      !ShouldSync(profile_, *extension)) {
    return;
  }

  // TODO(tim): If we get here and IsSyncing is false, this will cause
  // "back from the dead" style bugs, because sync will add-back the extension
  // that was uninstalled here when MergeDataAndStartSyncing is called.
  // See crbug.com/256795.
  // Possible fix: Set NeedsSync here, then in MergeDataAndStartSyncing, if
  // NeedsSync is set but the extension isn't installed, send a sync deletion.
  if (!ignore_updates_) {
    syncer::DataType type =
        extension->is_app() ? syncer::APPS : syncer::EXTENSIONS;
    SyncBundle* bundle = GetSyncBundle(type);
    if (bundle->IsSyncing()) {
      bundle->PushSyncDeletion(extension->id(),
                               CreateSyncData(*extension).GetSyncData());
    } else if (system_->is_ready() && !flare_.is_null()) {
      flare_.Run(type);  // Tell sync to start ASAP.
    }
  }

  pending_updates_.erase(extension->id());
}

void ExtensionSyncService::OnExtensionStateChanged(
    const std::string& extension_id,
    bool state) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // We can get pref change notifications for extensions that aren't installed
  // (yet). In that case, we'll pick up the change later via ExtensionRegistry
  // observation (in OnExtensionInstalled).
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionSyncService::OnExtensionDisableReasonsChanged(
    const std::string& extension_id,
    int disabled_reasons) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // We can get pref change notifications for extensions that aren't installed
  // (yet). In that case, we'll pick up the change later via ExtensionRegistry
  // observation (in OnExtensionInstalled).
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

SyncBundle* ExtensionSyncService::GetSyncBundle(syncer::DataType type) {
  return const_cast<SyncBundle*>(
      const_cast<const ExtensionSyncService&>(*this).GetSyncBundle(type));
}

const SyncBundle* ExtensionSyncService::GetSyncBundle(
    syncer::DataType type) const {
  return (type == syncer::APPS) ? &app_sync_bundle_ : &extension_sync_bundle_;
}

std::vector<ExtensionSyncData> ExtensionSyncService::GetLocalSyncDataList(
    syncer::DataType type) const {
  // Collect the local state.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  std::vector<ExtensionSyncData> data;
  // Note: Maybe we should include blocklisted/blocked extensions here, i.e.
  // just call registry->GeneratedInstalledExtensionsSet().
  // It would be more consistent, but the danger is that the black/blocklist
  // hasn't been updated on all clients by the time sync has kicked in -
  // so it's safest not to. Take care to add any other extension lists here
  // in the future if they are added.
  FillSyncDataList(registry->enabled_extensions(), type, &data);
  FillSyncDataList(registry->disabled_extensions(), type, &data);
  FillSyncDataList(registry->terminated_extensions(), type, &data);
  return data;
}

void ExtensionSyncService::FillSyncDataList(
    const ExtensionSet& extensions,
    syncer::DataType type,
    std::vector<ExtensionSyncData>* sync_data_list) const {
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (IsCorrectSyncType(*extension, type) &&
        ShouldSync(profile_, *extension)) {
      // We should never have pending data for an installed extension.
      DCHECK(!GetSyncBundle(type)->HasPendingExtensionData(extension->id()));
      sync_data_list->push_back(CreateSyncData(*extension));
    }
  }
}
