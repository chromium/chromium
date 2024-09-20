// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/version.h"
#include "chrome/browser/extensions/sync_bundle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/syncable_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionService;
class ExtensionSet;
class ExtensionSyncData;
}  // namespace extensions

// SyncableService implementation responsible for the APPS and EXTENSIONS data
// types, i.e. "proper" apps/extensions (not themes).
class ExtensionSyncService final : public syncer::SyncableService,
                                   public KeyedService,
                                   public extensions::ExtensionRegistryObserver,
                                   public extensions::ExtensionPrefsObserver {
 public:
  explicit ExtensionSyncService(Profile* profile);

  ExtensionSyncService(const ExtensionSyncService&) = delete;
  ExtensionSyncService& operator=(const ExtensionSyncService&) = delete;

  ~ExtensionSyncService() override;

  // Convenience function to get the ExtensionSyncService for a BrowserContext.
  static ExtensionSyncService* Get(content::BrowserContext* context);

  // Returns whether the given extension should be synced by this class.
  // Filters out unsyncable extensions as well as themes (which are handled by
  // ThemeSyncableService instead).
  static bool ShouldSync(content::BrowserContext* context,
                         const extensions::Extension& extension);

  // Notifies Sync (if needed) of a newly-installed extension or a change to
  // an existing extension. Call this when you change an extension setting that
  // is synced as part of ExtensionSyncData (e.g. incognito_enabled).
  void SyncExtensionChangeIfNeeded(const extensions::Extension& extension);

  // syncer::SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::DataType type) override;
  syncer::SyncDataList GetAllSyncDataForTesting(syncer::DataType type) const;
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  base::WeakPtr<SyncableService> AsWeakPtr() override;

  void SetSyncStartFlareForTesting(
      const syncer::SyncableService::StartSyncFlare& flare);

  // Special hack: There was a bug where themes incorrectly ended up in the
  // syncer::EXTENSIONS type. This is for cleaning up the data. crbug.com/558299
  // DO NOT USE FOR ANYTHING ELSE!
  // TODO(crbug.com/41401013): This *should* be safe to remove now, but it's
  // not.
  void DeleteThemeDoNotUse(const extensions::Extension& theme);

 private:
  FRIEND_TEST_ALL_PREFIXES(TwoClientExtensionAppsSyncTest,
                           UnexpectedLaunchType);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDisabledGlobalErrorTest,
                           HigherPermissionsFromSync);

  extensions::ExtensionService* extension_service() const;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // extensions::ExtensionPrefsObserver:
  void OnExtensionStateChanged(const std::string& extension_id,
                               bool state) override;
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disabled_reasons) override;

  // Gets the SyncBundle for the given |type|.
  extensions::SyncBundle* GetSyncBundle(syncer::DataType type);
  const extensions::SyncBundle* GetSyncBundle(syncer::DataType type) const;

  // Creates the ExtensionSyncData for the given app/extension.
  extensions::ExtensionSyncData CreateSyncData(
      const extensions::Extension& extension) const;

  // Applies the given change coming in from the server to the local state.
  void ApplySyncData(const extensions::ExtensionSyncData& extension_sync_data);

  // Collects the ExtensionSyncData for all installed apps or extensions.
  std::vector<extensions::ExtensionSyncData> GetLocalSyncDataList(
      syncer::DataType type) const;

  // Helper for GetLocalSyncDataList.
  void FillSyncDataList(
      const extensions::ExtensionSet& extensions,
      syncer::DataType type,
      std::vector<extensions::ExtensionSyncData>* sync_data_list) const;

  // The normal profile associated with this ExtensionSyncService.
  raw_ptr<Profile> profile_;

  raw_ptr<extensions::ExtensionSystem> system_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};
  base::ScopedObservation<extensions::ExtensionPrefs,
                          extensions::ExtensionPrefsObserver>
      prefs_observation_{this};

  // When this is set to true, any incoming updates (from the observers as well
  // as from explicit SyncExtensionChangeIfNeeded calls) are ignored. This is
  // set during ApplySyncData, so that ExtensionSyncService doesn't end up
  // notifying itself while applying sync changes.
  bool ignore_updates_;

  extensions::SyncBundle app_sync_bundle_;
  extensions::SyncBundle extension_sync_bundle_;

  // Map from extension id to pending update data. Used for two things:
  // - To send the new version back to the sync server while we're waiting for
  //   an extension to update.
  // - For re-enables, to defer granting permissions until the version matches.
  struct PendingUpdate;
  std::map<std::string, PendingUpdate> pending_updates_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  SyncableService::StartSyncFlare flare_;

  base::WeakPtrFactory<ExtensionSyncService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
