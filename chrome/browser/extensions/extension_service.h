// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/delayed_install_manager.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_telemetry_service_verdict_handler.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_metrics.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/omaha_attributes_handler.h"
#include "chrome/browser/extensions/safe_browsing_verdict_handler.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

class BlocklistedExtensionSyncServiceTest;
class Profile;
class ProfileManager;

namespace base {
class CommandLine;
class OneShotEvent;
}  // namespace base

FORWARD_DECLARE_TEST(BlocklistedExtensionSyncServiceTest,
                     SyncBlocklistedExtension);

namespace extensions {
class ChromeExtensionRegistrarDelegate;
class ComponentLoader;
class CorruptedExtensionReinstaller;
class DelayedInstallManager;
class ExtensionActionStorageManager;
class ExtensionAllowlist;
class ExtensionErrorController;
class ExtensionRegistry;
class ExtensionSystem;
class ExtensionUpdater;
class ExternalInstallManager;
class ExternalProviderManager;
class PendingExtensionManager;
class SharedModuleService;
class UpdateObserver;
enum class UnloadedExtensionReason;

// This is an interface class to encapsulate the dependencies that
// various classes have on ExtensionService. This allows easy mocking.
class ExtensionServiceInterface {
 public:
  virtual ~ExtensionServiceInterface() = default;

  // Returns an update for an extension with the specified id, if installation
  // of that update was previously delayed because the extension was in use. If
  // no updates are pending for the extension returns NULL.
  virtual const Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const = 0;

  // Attempts finishing installation of an update for an extension with the
  // specified id, when installation of that extension was previously delayed.
  // |install_immediately| - Whether the extension should be installed if it's
  //     currently in use.
  // Returns whether the extension installation was finished.
  virtual bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                                bool install_immediately) = 0;

  // Go through each extension and unload those that are not allowed to run by
  // management policy providers (ie. network admin and Google-managed
  // blocklist).
  virtual void CheckManagementPolicy() = 0;

  // Safe to call multiple times in a row.
  //
  // TODO(akalin): Remove this method (and others) once we refactor
  // themes sync to not use it directly.
  virtual void CheckForUpdatesSoon() = 0;

  // Adds |extension| to this ExtensionService and notifies observers that the
  // extension has been loaded.
  virtual void AddExtension(const Extension* extension) = 0;

  // Check if we have preferences for the component extension and, if not or if
  // the stored version differs, install the extension (without requirements
  // checking) before calling AddExtension.
  virtual void AddComponentExtension(const Extension* extension) = 0;

  // Unload the specified extension.
  virtual void UnloadExtension(const std::string& extension_id,
                               UnloadedExtensionReason reason) = 0;

  // Remove the specified component extension.
  virtual void RemoveComponentExtension(const std::string& extension_id) = 0;

  // Whether a user is able to disable a given extension.
  virtual bool UserCanDisableInstalledExtension(
      const std::string& extension_id) = 0;

  virtual base::WeakPtr<ExtensionServiceInterface> AsWeakPtr() = 0;
};

// Manages installed and running Chromium extensions. An instance is shared
// between normal and incognito profiles.
class ExtensionService : public ExtensionServiceInterface,
                         public content::RenderProcessHostCreationObserver,
                         public content::RenderProcessHostObserver,
                         public Blocklist::Observer,
                         public CWSInfoService::Observer,
                         public ExtensionManagement::Observer,
                         public UpgradeObserver,
                         public ExtensionHostRegistry::Observer,
                         public ProfileManagerObserver {
 public:
  // Constructor stores pointers to |profile| and |extension_prefs| but
  // ownership remains at caller.
  ExtensionService(Profile* profile,
                   const base::CommandLine* command_line,
                   const base::FilePath& install_directory,
                   const base::FilePath& unpacked_install_directory,
                   ExtensionPrefs* extension_prefs,
                   Blocklist* blocklist,
                   ExtensionErrorController* error_controller,
                   bool autoupdate_enabled,
                   bool extensions_enabled,
                   base::OneShotEvent* ready);

  ExtensionService(const ExtensionService&) = delete;
  ExtensionService& operator=(const ExtensionService&) = delete;

  ~ExtensionService() override;

  // ExtensionServiceInterface implementation.
  //
  void UnloadExtension(const std::string& extension_id,
                       UnloadedExtensionReason reason) override;
  void RemoveComponentExtension(const std::string& extension_id) override;
  void AddExtension(const Extension* extension) override;
  void AddComponentExtension(const Extension* extension) override;
  const Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;
  void CheckManagementPolicy() override;
  void CheckForUpdatesSoon() override;
  base::WeakPtr<ExtensionServiceInterface> AsWeakPtr() override;

  // ExtensionManagement::Observer implementation:
  void OnExtensionManagementSettingsChanged() override;

  // Initialize and start all installed extensions.
  void Init();

  // Called when the associated Profile is going to be destroyed, as part of
  // KeyedService two-phase shutdown.
  void Shutdown();

  // Reloads the specified extension, sending the onLaunched() event to it if it
  // currently has any window showing.
  // Allows noisy failures.
  // NOTE: Reloading an extension can invalidate |extension_id| and Extension
  // pointers for the given extension. Consider making a copy of |extension_id|
  // first and retrieving a new Extension pointer afterwards.
  void ReloadExtension(const std::string& extension_id);

  // Suppresses noisy failures.
  void ReloadExtensionWithQuietFailure(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |reason| lets the caller specify why the
  // extension is uninstalled.
  // Note: this method synchronously removes the extension from the
  // set of installed extensions stored in the ExtensionRegistry, but will
  // asynchronously remove site-related data and the files stored on disk.
  // Returns true if an uninstall was successfully triggered; this can fail if
  // the extension cannot be uninstalled (such as a policy force-installed
  // extension).
  // |done_callback| is synchronously invoked once the site-related data and the
  // files stored on disk are removed. If such a callback is not needed, pass in
  // a null callback (base::NullCallback()).
  bool UninstallExtension(
      const std::string& extension_id,
      UninstallReason reason,
      std::u16string* error,
      base::OnceClosure done_callback = base::NullCallback());

  // Enables the extension. If the extension is already enabled, does
  // nothing.
  void EnableExtension(const std::string& extension_id);

  // Takes Safe Browsing and Omaha blocklist states into account and decides
  // whether to remove greylist disabled reason. Called when a greylisted
  // state is removed from the Safe Browsing blocklist or Omaha blocklist. Also
  // clears all acknowledged states if the greylist disabled reason is removed.
  void OnGreylistStateRemoved(const std::string& extension_id);

  // Takes acknowledged blocklist states into account and decides whether to
  // disable the greylisted extension. Called when a new greylisted state is
  // added to the Safe Browsing blocklist or Omaha blocklist.
  void OnGreylistStateAdded(const std::string& extension_id,
                            BitMapBlocklistState new_state);

  // Takes Safe Browsing and Omaha malware blocklist states into account and
  // decides whether to remove the extension from the blocklist and reload it.
  // Called when a blocklisted extension is removed from the Safe Browsing
  // malware blocklist or Omaha malware blocklist. Also clears the acknowledged
  // state if the extension is reloaded.
  void OnBlocklistStateRemoved(const std::string& extension_id);

  // Takes acknowledged malware blocklist state into account and decides whether
  // to add the extension to the blocklist and unload it. Called when the
  // extension is added to the Safe Browsing malware blocklist or the Omaha
  // malware blocklist.
  void OnBlocklistStateAdded(const std::string& extension_id);

  // Removes the disable reason and enable the extension if there are no disable
  // reasons left and is not blocked for another reason.
  void RemoveDisableReasonAndMaybeEnable(const std::string& extension_id,
                                         disable_reason::DisableReason reason);

  // Performs action based on Omaha attributes for the extension.
  void PerformActionBasedOnOmahaAttributes(const std::string& extension_id,
                                           const base::Value::Dict& attributes);

  // Performs action based on verdicts received from the Extension Telemetry
  // server. Currently, these verdicts are limited to off-store extensions.
  void PerformActionBasedOnExtensionTelemetryServiceVerdicts(
      const Blocklist::BlocklistStateMap& blocklist_state_map);

  // Disables the extension. If the extension is already disabled, just adds
  // the incoming disable reason(s). If the extension cannot be disabled (due to
  // policy), does nothing.
  void DisableExtension(const ExtensionId& extension_id,
                        disable_reason::DisableReason disable_reason);
  void DisableExtension(const ExtensionId& extension_id,
                        const DisableReasonSet& disable_reasons);

  // Any code which needs to write unknown reasons should use the
  // methods below, which operate on raw integers. This is needed for scenarios
  // like Sync where unknown reasons can be synced from newer versions of the
  // browser to older versions. The methods above will trigger undefined
  // behavior when unknown values are casted to DisableReason while constructing
  // DisableReasonSet. Most code should use the methods above. We want to limit
  // the usage of the method below, so it is guarded by a passkey.
  void DisableExtensionWithRawReasons(
      ExtensionPrefs::DisableReasonRawManipulationPasskey,
      const ExtensionId& extension_id,
      const base::flat_set<int>& disable_reasons);

  // Same as |DisableExtension|, but assumes that the request to disable
  // |extension_id| originates from |source_extension| when evaluating whether
  // the extension can be disabled. Please see |ExtensionMayModifySettings|
  // for details.
  void DisableExtensionWithSource(const Extension* source_extension,
                                  const ExtensionId& extension_id,
                                  disable_reason::DisableReason disable_reason);

  // Disable non-default and non-managed extensions with ids not in
  // |except_ids|. Default extensions are those from the Web Store with
  // |was_installed_by_default| flag.
  void DisableUserExtensionsExcept(const std::vector<std::string>& except_ids);

  // Puts all extensions in a blocked state: Unloading every extension, and
  // preventing them from ever loading until UnblockAllExtensions is called.
  // This state is stored in preferences, so persists until Chrome restarts.
  //
  // Component, external component and allowlisted policy installed extensions
  // are exempt from being Blocked (see CanBlockExtension in .cc file).
  void BlockAllExtensions();

  // All blocked extensions are reverted to their previous state, and are
  // reloaded. Newly added extensions are no longer automatically blocked.
  void UnblockAllExtensions();

  // Informs the service that an extension's files are in place for loading.
  //
  // |extension|                the extension
  // |page_ordinal|             the location of the extension in the app
  //                            launcher
  // |install_flags|            a bitmask of InstallFlags
  // |ruleset_install_prefs|    Install prefs needed for the Declarative Net
  //                            Request API.
  void OnExtensionInstalled(const Extension* extension,
                            const syncer::StringOrdinal& page_ordinal,
                            int install_flags,
                            base::Value::Dict ruleset_install_prefs = {});
  void OnExtensionInstalled(const Extension* extension,
                            const syncer::StringOrdinal& page_ordinal) {
    OnExtensionInstalled(extension, page_ordinal,
                         static_cast<int>(kInstallFlagNone));
  }

  // ExtensionHost of background page calls this method right after its renderer
  // main frame has been created.
  void DidCreateMainFrameForBackgroundPage(ExtensionHost* host);

  // Unloads the given extension and marks the extension as terminated. This
  // doesn't notify the user that the extension was terminated, if such a
  // notification is desired the calling code is responsible for doing that.
  void TerminateExtension(const std::string& extension_id);

  // Adds/Removes update observers.
  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);

  // Returns whether a user is able to disable a given extension or if that is
  // not possible (for instance, extension was enabled by policy).
  bool UserCanDisableInstalledExtension(
      const std::string& extension_id) override;

  //////////////////////////////////////////////////////////////////////////////
  // Simple Accessors

  // Returns a WeakPtr to the ExtensionService.
  base::WeakPtr<ExtensionService> AsExtensionServiceWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Returns profile_ as a BrowserContext.
  content::BrowserContext* GetBrowserContext() const;

  bool block_extensions() const { return block_extensions_; }

  Profile* profile() { return profile_; }

  ComponentLoader* component_loader() { return component_loader_.get(); }

  SharedModuleService* shared_module_service() {
    return shared_module_service_.get();
  }

  ForceInstalledTracker* force_installed_tracker() {
    return &force_installed_tracker_;
  }

  // TODO(crbug.com/404941806): Delete this method and use the KeyedService
  // directly.
  ExtensionAllowlist* allowlist() { return allowlist_; }

  const std::set<std::string>& disable_flag_exempted_extensions() const {
    return disable_flag_exempted_extensions_;
  }

  //////////////////////////////////////////////////////////////////////////////
  // For Testing

  // Unload all extensions. Does not send notifications.
  void UnloadAllExtensionsForTest();

  // Reloads all extensions. Does not notify that extensions are ready.
  void ReloadExtensionsForTest();

  // Simulate an extension being blocklisted for tests.
  void BlocklistExtensionForTest(const std::string& extension_id);

  // Simulate an extension being greylisted for tests.
  void GreylistExtensionForTest(const std::string& extension_id,
                                const BitMapBlocklistState& state);

#if defined(UNIT_TEST)
  void FinishInstallationForTest(const Extension* extension) {
    extension_registrar_->FinishInstallation(extension);
  }

  void UninstallMigratedExtensionsForTest() { UninstallMigratedExtensions(); }

  void ProfileMarkedForPermanentDeletionForTest() {
    OnProfileMarkedForPermanentDeletion(profile_);
  }
#endif

 private:
  // Loads extensions specified via a command line flag/switch.
  void LoadExtensionsFromCommandLineFlag(const char* switch_name);
#if BUILDFLAG(IS_CHROMEOS)
  void LoadSigninProfileTestExtension(const std::string& path);
#endif

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostRenderProcessGone(
      content::BrowserContext* browser_context,
      ExtensionHost* extension_host) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Blocklist::Observer implementation.
  void OnBlocklistUpdated() override;

  // CWSInfoService::Observer implementation.
  void OnCWSInfoChanged() override;

  // UpgradeObserver implementation.
  void OnUpgradeRecommended() override;

  // ProfileManagerObserver implementation.
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  // Attempt to enable all disabled extensions which the only disabled reason is
  // reloading.
  void EnabledReloadableExtensions();

  // Signals *ready_ and sends a notification to the listeners.
  void SetReadyAndNotifyListeners();

  // Update preferences for a new or updated extension; notify observers that
  // the extension is installed, e.g., to update event handlers on background
  // pages; and perform other extension install tasks before calling
  // AddExtension.
  // |install_flags| is a bitmask of InstallFlags.
  void AddNewOrUpdatedExtension(const Extension* extension,
                                const base::flat_set<int>& disable_reasons,
                                int install_flags,
                                const syncer::StringOrdinal& page_ordinal,
                                const std::string& install_parameter,
                                base::Value::Dict ruleset_install_prefs);

  // Manages the blocklisted extensions, intended as callback from
  // Blocklist::GetBlocklistedIDs.
  void ManageBlocklist(const Blocklist::BlocklistStateMap& blocklisted_ids);

  // Used only by test code.
  void UnloadAllExtensionsInternal();

  // Disable apps & extensions now to stop them from running after a profile
  // has been conceptually deleted. Don't wait for full browser shutdown and
  // the actual profile objects to be destroyed.
  void OnProfileDestructionStarted();

  // Called when the initial extensions load has completed.
  void OnInstalledExtensionsLoaded();

  // Uninstall extensions that have been migrated to component extensions.
  void UninstallMigratedExtensions();

  // Called when the Developer Mode preference is changed:
  // - Disables unpacked extensions if developer mode is OFF.
  // - Re-enables unpacked extensions if developer mode is ON and there are no
  // other disable reasons associated with them.
  void OnDeveloperModePrefChanged();

  raw_ptr<const base::CommandLine> command_line_ = nullptr;

  // The normal profile associated with this ExtensionService.
  raw_ptr<Profile> profile_ = nullptr;

  // The ExtensionSystem for the profile above.
  raw_ptr<ExtensionSystem> system_ = nullptr;

  // Preferences for the owning profile.
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;

  // Blocklist for the owning profile.
  raw_ptr<Blocklist> blocklist_ = nullptr;

  raw_ptr<ExtensionAllowlist> allowlist_ = nullptr;

  SafeBrowsingVerdictHandler safe_browsing_verdict_handler_;

  ExtensionTelemetryServiceVerdictHandler
      extension_telemetry_service_verdict_handler_;

  // Sets of enabled/disabled/terminated/blocklisted extensions. Not owned.
  raw_ptr<ExtensionRegistry> registry_ = nullptr;

  // Set of allowlisted enabled extensions loaded from the
  // --disable-extensions-except command line flag.
  std::set<std::string> disable_flag_exempted_extensions_;

  // Hold the set of pending extensions. Not owned.
  raw_ptr<PendingExtensionManager> pending_extension_manager_ = nullptr;

  // Manages external providers. Not ownedd.
  raw_ptr<ExternalProviderManager> external_provider_manager_ = nullptr;

  // Signaled when all extensions are loaded.
  const raw_ptr<base::OneShotEvent> ready_;

  // Our extension updater. May be disabled if updates are turned off.
  raw_ptr<ExtensionUpdater> updater_ = nullptr;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  // Keeps track of loading and unloading component extensions.
  std::unique_ptr<ComponentLoader> component_loader_;

  // Set to true if this is the first time this ExtensionService has run.
  // Used for specially handling external extensions that are installed the
  // first time.
  bool is_first_run_ = false;

  // Set to true if extensions are all to be blocked.
  bool block_extensions_ = false;

  // The controller for the UI that alerts the user about any blocklisted
  // extensions. Not owned.
  raw_ptr<ExtensionErrorController> error_controller_ = nullptr;

  // The manager for extensions that were externally installed that is
  // responsible for prompting the user about suspicious extensions. Not owned.
  raw_ptr<ExternalInstallManager> external_install_manager_ = nullptr;

  std::unique_ptr<ExtensionActionStorageManager>
      extension_action_storage_manager_;

  // The SharedModuleService used to check for import dependencies.
  std::unique_ptr<SharedModuleService> shared_module_service_;

  base::ObserverList<UpdateObserver, true>::Unchecked update_observers_;

  std::unique_ptr<ChromeExtensionRegistrarDelegate>
      extension_registrar_delegate_;

  // Helper to register and unregister extensions.
  raw_ptr<ExtensionRegistrar> extension_registrar_ = nullptr;

  // Needs `extension_registrar_` during construction.
  OmahaAttributesHandler omaha_attributes_handler_;

  // Tracker of enterprise policy forced installation.
  ForceInstalledTracker force_installed_tracker_;

  // Reports force-installed extension metrics to UMA.
  ForceInstalledMetrics force_installed_metrics_;

  // Schedules downloads/reinstalls of the corrupted extensions.
  raw_ptr<CorruptedExtensionReinstaller> corrupted_extension_reinstaller_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};

  base::ScopedObservation<CWSInfoService, CWSInfoService::Observer>
      cws_info_service_observation_{this};

  raw_ptr<DelayedInstallManager> delayed_install_manager_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<ExtensionService> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           DestroyingProfileClearsExtensions);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, SetUnsetBlocklistInPrefs);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, NoUnsetBlocklistInPrefs);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           BlocklistedExtensionWillNotInstall);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           UnloadBlocklistedExtensionPolicy);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           WillNotLoadBlocklistedExtensionsFromDirectory);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, ReloadBlocklistedExtension);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, RemoveExtensionFromBlocklist);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, BlocklistedInPrefsFromStartup);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           ManagementPolicyProhibitsEnableOnInstalled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           BlockAndUnblockBlocklistedExtension);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           CanAddDisableReasonToBlocklistedExtension);
  FRIEND_TEST_ALL_PREFIXES(::BlocklistedExtensionSyncServiceTest,
                           SyncBlocklistedExtension);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAllowlistUnitTest,
                           ExtensionsNotAllowlistedThenBlocklisted);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAllowlistUnitTest,
                           ExtensionsBlocklistedThenNotAllowlisted);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           GreylistedExtensionDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           GreylistDontEnableManuallyDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           GreylistUnknownDontChange);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           UnblocklistedExtensionStillGreylisted);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           GreylistedExtensionDoesNotDisableAgain);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           GreylistedExtensionDisableAgainIfReAdded);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           DisableExtensionForDifferentGreylistState);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           DisableExtensionWhenSwitchingBetweenGreylistStates);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           AcknowledgedStateBackFilled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           ExtensionUninstalledWhenBlocklisted);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingVerdictHandlerUnitTest,
                           ExtensionUninstalledWhenBlocklistFetching);
  friend class ::BlocklistedExtensionSyncServiceTest;
  friend class SafeBrowsingVerdictHandlerUnitTest;
  friend class BlocklistStatesInteractionUnitTest;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
