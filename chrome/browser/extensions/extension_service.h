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
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/cws_info_service.h"
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
#include "extensions/browser/blocklist.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/delayed_install_manager.h"
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

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
  // `install_immediately` - Whether the extension should be installed if it's
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
  // Constructor stores pointers to `profile` and `extension_prefs` but
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

  // Performs action based on Omaha attributes for the extension.
  void PerformActionBasedOnOmahaAttributes(const std::string& extension_id,
                                           const base::Value::Dict& attributes);

  // Performs action based on verdicts received from the Extension Telemetry
  // server. Currently, these verdicts are limited to off-store extensions.
  void PerformActionBasedOnExtensionTelemetryServiceVerdicts(
      const Blocklist::BlocklistStateMap& blocklist_state_map);

  // Disable non-default and non-managed extensions with ids not in
  // `except_ids`. Default extensions are those from the Web Store with
  // `was_installed_by_default` flag.
  void DisableUserExtensionsExcept(const std::vector<std::string>& except_ids);

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

  Profile* profile() { return profile_; }

  ForceInstalledTracker* force_installed_tracker() {
    return &force_installed_tracker_;
  }

  // TODO(crbug.com/404941806): Delete this method and use the KeyedService
  // directly.
  ExtensionAllowlist* allowlist() { return allowlist_; }

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

  void UninstallMigratedExtensionsForTest();

  bool HasShutDownExecutedForTest() const { return is_shut_down_executed_; }

#if defined(UNIT_TEST)
  void FinishInstallationForTest(const Extension* extension) {
    extension_registrar_->FinishInstallation(extension);
  }

  void ProfileMarkedForPermanentDeletionForTest() {
    OnProfileMarkedForPermanentDeletion(profile_);
  }
#endif

  // Load Extension Flags.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(LoadExtensionFlag)
  enum class LoadExtensionFlag {
    // --load-extension flag.
    kLoadExtension = 0,
    // --disable-extensions-except flag.
    kDisableExtensionsExcept = 1,

    kMaxValue = kDisableExtensionsExcept,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/extensions/enums.xml:LoadExtensionFlag)

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

  // Called when the Developer Mode preference is changed:
  // - Disables unpacked extensions if developer mode is OFF.
  // - Re-enables unpacked extensions if developer mode is ON and there are no
  // other disable reasons associated with them.
  void OnDeveloperModePrefChanged();

  // Logs a warning if --extensions-on-chrome-urls switch is used in Google
  // Chrome.
  void LogExtensionsOnChromeUrlsSwitchWarningIfNeeded();

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

  // Sets of enabled/disabled/terminated/blocklisted extensions. Not owned.
  raw_ptr<ExtensionRegistry> registry_ = nullptr;

  // Hold the set of pending extensions. Not owned.
  raw_ptr<PendingExtensionManager> pending_extension_manager_ = nullptr;

  // Manages external providers. Not owned.
  raw_ptr<ExternalProviderManager> external_provider_manager_ = nullptr;

  // Signaled when all extensions are loaded.
  const raw_ptr<base::OneShotEvent> ready_;

  // Our extension updater. May be disabled if updates are turned off.
  raw_ptr<ExtensionUpdater> updater_ = nullptr;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  // Keeps track of loading and unloading component extensions.
  raw_ptr<ComponentLoader> component_loader_ = nullptr;

  // Set to true if this is the first time this ExtensionService has run.
  // Used for specially handling external extensions that are installed the
  // first time.
  bool is_first_run_ = false;

  // Set to true if the ExtensionService::Shutdown() has been executed.
  // Used in test to ensure the service's shutdown method has been called.
  bool is_shut_down_executed_ = false;

  // The controller for the UI that alerts the user about any blocklisted
  // extensions. Not owned.
  raw_ptr<ExtensionErrorController> error_controller_ = nullptr;

  // The manager for extensions that were externally installed that is
  // responsible for prompting the user about suspicious extensions. Not owned.
  raw_ptr<ExternalInstallManager> external_install_manager_ = nullptr;

  std::unique_ptr<ExtensionActionStorageManager>
      extension_action_storage_manager_;

  std::unique_ptr<ChromeExtensionRegistrarDelegate>
      extension_registrar_delegate_;

  // Helper to register and unregister extensions.
  raw_ptr<ExtensionRegistrar> extension_registrar_ = nullptr;

  // Needs `extension_registrar_` during construction.
  SafeBrowsingVerdictHandler safe_browsing_verdict_handler_;

  // Needs `extension_registrar_` during construction.
  ExtensionTelemetryServiceVerdictHandler
      extension_telemetry_service_verdict_handler_;

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
