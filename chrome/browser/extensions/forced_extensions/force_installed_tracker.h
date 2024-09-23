// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_TRACKER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

// Used to track status of force-installed extensions for the profile: are they
// successfully loaded, failed to install, or neither happened yet.
// ExtensionService owns this class and outlives it.
class ForceInstalledTracker : public ExtensionRegistryObserver,
                              public InstallStageTracker::Observer,
                              public policy::PolicyService::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called after every force-installed extension is loaded (not yet
    // installed) or reported as failure.
    //
    // Called exactly once, during startup (may take several minutes). Use
    // IsDoneLoading() to know if it has already been called. If there are no
    // force-installed extensions configured, this method still gets called.
    virtual void OnForceInstalledExtensionsLoaded() {}

    // Same as OnForceInstalledExtensionsLoaded(), but after they're ready
    // instead of loaded.
    //
    // Called exactly once, during startup (may take several minutes). Use
    // IsReady() to know if it has already been called. If there are no
    // force-installed extensions configured, this method still gets called.
    virtual void OnForceInstalledExtensionsReady() {}

    // Called when a force-installed extension with id |extension_id| fails to
    // install with failure reason |reason|.
    //
    // Can be called multiple times, one for each failed extension install.
    virtual void OnForceInstalledExtensionFailed(
        const ExtensionId& extension_id,
        InstallStageTracker::FailureReason reason,
        bool is_from_store) {}

    // Called when cache status is retrieved from InstallationStageTracker.
    virtual void OnExtensionDownloadCacheStatusRetrieved(
        const ExtensionId& id,
        ExtensionDownloaderDelegate::CacheStatus cache_status) {}
  };

  ForceInstalledTracker(ExtensionRegistry* registry, Profile* profile);

  ~ForceInstalledTracker() override;

  ForceInstalledTracker(const ForceInstalledTracker&) = delete;
  ForceInstalledTracker& operator=(const ForceInstalledTracker&) = delete;

  // Returns true if all extensions loaded/failed loading.
  bool IsDoneLoading() const;

  // Returns true if all extensions installed/failed installing.
  bool IsReady() const;

  // Returns true if all extensions installed/failed installing and there is
  // at least one such extension.
  bool IsComplete() const;

  // Adds observers to this object, to get notified when installation is
  // finished.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;
  void OnShutdown(ExtensionRegistry*) override;

  // InstallStageTracker::Observer overrides:
  void OnExtensionInstallationFailed(
      const ExtensionId& extension_id,
      InstallStageTracker::FailureReason reason) override;
  void OnExtensionDownloadCacheStatusRetrieved(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status) override;

  // policy::PolicyService::Observer overrides:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

  enum class ExtensionStatus {
    // Extension appears in force-install list, but it not installed yet.
    kPending,

    // Extension was successfully loaded.
    kLoaded,

    // Extension is ready. This happens after loading.
    kReady,

    // Extension installation failure was reported.
    kFailed
  };

  // Helper struct with supplementary info for extensions from force-install
  // list.
  struct ExtensionInfo {
    // Current status of the extension: loaded, failed, or still installing.
    ExtensionStatus status;

    // Additional info: whether extension is from Chrome Web Store, or
    // self-hosted.
    bool is_from_store;
  };

  const std::map<ExtensionId, ExtensionInfo>& extensions() const {
    return extensions_;
  }

  // Returns true only in case of some well-known admin side misconfigurations
  // which are easy to detect. Can return false for misconfigurations which are
  // hard to distinguish with other errors.
  bool IsMisconfiguration(
      const InstallStageTracker::InstallationData& installation_data,
      const ExtensionId& id) const;

  static bool IsExtensionFetchedFromCache(
      const std::optional<ExtensionDownloaderDelegate::CacheStatus>& status);

 private:
  policy::PolicyService* policy_service();

  // Fires OnForceInstallationFinished() on observers, then changes `status_` to
  // kComplete.
  void MaybeNotifyObservers();

  // Increments (or decrements) `load_pending_count_` and
  // `install_pending_count_` by `delta`, depending on `status`.
  void UpdateCounters(ExtensionStatus status, int delta);

  // Modifies `extensions_` and bounded counter by adding extension
  // to the collection.
  void AddExtensionInfo(const ExtensionId& extension_id,
                        ExtensionStatus status,
                        bool is_from_store);

  // Modifies `extensions_` and bounded counter by changing status
  // of one extension.
  void ChangeExtensionStatus(const ExtensionId& extension_id,
                             ExtensionStatus status);

  // Proceeds and returns true if `kInstallForceList` pref is not empty.
  bool ProceedIfForcedExtensionsPrefReady();
  // Loads list of force-installed extensions if available. Only called once.
  void OnForcedExtensionsPrefReady();

  void OnInstallForcelistChanged();

  raw_ptr<const ExtensionManagement> extension_management_;

  // Unowned, but guaranteed to outlive this object.
  raw_ptr<ExtensionRegistry> registry_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;

  // Collection of all extensions we are interested in here. Don't update
  // directly, use AddExtensionInfo/RemoveExtensionInfo/ChangeExtensionStatus
  // methods, as |pending_extension_counter_| has to be in sync with contents of
  // this collection.
  std::map<ExtensionId, ExtensionInfo> extensions_;

  // Number of extensions in |extensions_| with status |PENDING|.
  size_t load_pending_count_ = 0;
  // Number of extensions in |extensions_| with status |PENDING| or |LOADED|.
  // (ie. could be loaded, but not ready yet).
  size_t ready_pending_count_ = 0;

  // Stores the current state of this tracker, to know when it's complete, and
  // to perform sanity DCHECK()s.
  enum Status {
    // Waiting for PolicyService to finish initializing. Listening for
    // OnPolicyServiceInitialized().
    kWaitingForPolicyService,
    // At the startup the `kInstallForceList` preference might be empty, meaning
    // that no extensions are yet specified to be force installed.
    // Waiting for `kInstallForceList` to be populated.
    kWaitingForInstallForcelistPref,
    // Waiting for one or more extensions to finish loading. Listening for
    // |ExtensionRegistryObserver| events.
    kWaitingForExtensionLoads,
    // Waiting for one or more extensions to finish loading. Listening for
    // |ExtensionRegistryObserver| events. Extensions have already finished
    // loading; we're still waiting for the "ready" state. IsDoneLoading()
    // returns true, but IsReady() returns false.
    kWaitingForExtensionReady,
    // All extensions have finished installing (successfully or not); observers
    // have been called exactly once, and IsDoneLoading() and IsReady()
    // both return true.
    kComplete,
  };
  Status status_ = kWaitingForPolicyService;
  bool forced_extensions_pref_ready_ = false;
  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
  base::ScopedObservation<InstallStageTracker, InstallStageTracker::Observer>
      collector_observation_{this};

  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_TRACKER_H_
