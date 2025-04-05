// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "chrome/browser/extensions/delayed_install_manager.h"
#include "chrome/browser/extensions/extension_action_storage_manager.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/omaha_attributes_handler.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/browser/update_observer.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/ash/extensions/install_limiter.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace extensions {

using LoadErrorBehavior = ExtensionRegistrar::LoadErrorBehavior;

namespace {

// Wait this long after an extensions becomes idle before updating it.
constexpr base::TimeDelta kUpdateIdleDelay = base::Seconds(5);

// IDs of component extensions that have been obsoleted and need to be
// uninstalled.
// Note: We preserve at least one entry here for continued testing coverage.
const char* const kObsoleteComponentExtensionIds[] = {
    // The Video Player chrome app became obsolete in m93, but is preserved for
    // continued test coverage.
    "jcgeabjmjgoblfofpppfkcoakmfobdko",  // Video Player
};

const char kBlockLoadCommandline[] = "command_line";

// ExtensionUnpublishedAvailability policy default value.
constexpr int kAllowUnpublishedExtensions = 0;

bool ShouldBlockCommandLineExtension(Profile& profile) {
  const base::Value::List& list =
      profile.GetPrefs()->GetList(pref_names::kExtensionInstallTypeBlocklist);
  for (const auto& val : list) {
    if (val.is_string() && val.GetString() == kBlockLoadCommandline) {
      return true;
    }
  }

  return false;
}
}  // namespace

// ExtensionService.

void ExtensionService::BlocklistExtensionForTest(
    const std::string& extension_id) {
  extension_registrar_->BlocklistExtensionForTest(extension_id);  // IN-TEST
}

void ExtensionService::GreylistExtensionForTest(
    const std::string& extension_id,
    const BitMapBlocklistState& state) {
  extension_registrar_->GreylistExtensionForTest(extension_id,
                                                 state);  // IN-TEST
}

ExtensionService::ExtensionService(
    Profile* profile,
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    const base::FilePath& unpacked_install_directory,
    ExtensionPrefs* extension_prefs,
    Blocklist* blocklist,
    ExtensionErrorController* error_controller,
    bool autoupdate_enabled,
    bool extensions_enabled,
    base::OneShotEvent* ready)
    : Blocklist::Observer(blocklist),
      command_line_(command_line),
      profile_(profile),
      system_(ExtensionSystem::Get(profile)),
      extension_prefs_(extension_prefs),
      blocklist_(blocklist),
      allowlist_(ExtensionAllowlist::Get(profile)),
      safe_browsing_verdict_handler_(extension_prefs,
                                     ExtensionRegistry::Get(profile),
                                     this),
      extension_telemetry_service_verdict_handler_(
          extension_prefs,
          ExtensionRegistry::Get(profile),
          this),
      registry_(ExtensionRegistry::Get(profile)),
      pending_extension_manager_(PendingExtensionManager::Get(profile)),
      external_provider_manager_(ExternalProviderManager::Get(profile)),
      ready_(ready),
      updater_(ExtensionUpdater::Get(profile)),
      component_loader_(ComponentLoader::Get(profile_)),
      error_controller_(error_controller),
      external_install_manager_(ExternalInstallManager::Get(profile)),
      shared_module_service_(new SharedModuleService(profile_)),
      extension_registrar_delegate_(
          std::make_unique<ChromeExtensionRegistrarDelegate>(
              profile_,
              component_loader_)),
      extension_registrar_(ExtensionRegistrar::Get(profile)),
      omaha_attributes_handler_(extension_prefs,
                                ExtensionRegistry::Get(profile),
                                this,
                                extension_registrar_),
      force_installed_tracker_(registry_, profile_),
      force_installed_metrics_(registry_, profile_, &force_installed_tracker_),
      corrupted_extension_reinstaller_(
          CorruptedExtensionReinstaller::Get(profile_)),
      delayed_install_manager_(DelayedInstallManager::Get(profile_)) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::ExtensionService::ctor");
  extension_registrar_delegate_->Init(extension_registrar_);
  // Figure out if extension installation should be enabled.
  if (ExtensionsBrowserClient::Get()->AreExtensionsDisabled(*command_line,
                                                            profile)) {
    extensions_enabled = false;
  }
  extension_registrar_->Init(extension_registrar_delegate_.get(),
                             extensions_enabled, install_directory,
                             unpacked_install_directory);

  host_registry_observation_.Observe(ExtensionHostRegistry::Get(profile));

  // The ProfileManager may be null in unit tests.
  if (g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
  }

  UpgradeDetector::GetInstance()->AddObserver(this);

  cws_info_service_observation_.Observe(CWSInfoService::Get(profile_));

  ExtensionManagementFactory::GetForBrowserContext(profile_)->AddObserver(this);

  if (autoupdate_enabled) {
    // Initialize and enable the ExtensionUpdater.
    updater_->InitAndEnable(
        extension_prefs, profile->GetPrefs(), kDefaultUpdateFrequency,
        ExtensionsBrowserClient::Get()->GetExtensionCache(),
        base::BindRepeating(ChromeExtensionDownloaderFactory::CreateForProfile,
                            profile));
  }

  if (extensions_enabled) {
    external_provider_manager_->CreateExternalProviders();
  }

  // Set this as the ExtensionService for app sorting to ensure it causes syncs
  // if required.
  is_first_run_ = !extension_prefs_->SetAlertSystemFirstRun();

  error_controller_->set_is_first_run(is_first_run_);
  external_install_manager_->set_is_first_run(is_first_run_);

  extension_action_storage_manager_ =
      std::make_unique<ExtensionActionStorageManager>(profile_);

  SetCurrentDeveloperMode(
      util::GetBrowserContextId(profile),
      profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kExtensionsUIDeveloperMode,
      base::BindRepeating(&ExtensionService::OnDeveloperModePrefChanged,
                          base::Unretained(this)));
}

base::WeakPtr<ExtensionServiceInterface> ExtensionService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ExtensionService::~ExtensionService() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

void ExtensionService::Shutdown() {
  delayed_install_manager_ = nullptr;
  cws_info_service_observation_.Reset();
  ExtensionManagementFactory::GetForBrowserContext(profile())->RemoveObserver(
      this);
  external_install_manager_->Shutdown();
  corrupted_extension_reinstaller_ = nullptr;
  extension_registrar_->Shutdown();
  extension_registrar_delegate_->Shutdown();
  external_provider_manager_->Shutdown();
  pref_change_registrar_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Avoid dangling pointers.
  command_line_ = nullptr;
  system_ = nullptr;
  extension_prefs_ = nullptr;
  blocklist_ = nullptr;
  registry_ = nullptr;
  pending_extension_manager_ = nullptr;
  external_provider_manager_ = nullptr;
  error_controller_ = nullptr;
  allowlist_ = nullptr;
  external_install_manager_ = nullptr;
  updater_ = nullptr;
  component_loader_ = nullptr;
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::Init");

  DCHECK(!system_->is_ready());  // Can't redo init.
  DCHECK_EQ(registry_->enabled_extensions().size(), 0u);

  component_loader_->LoadAll();
  bool load_saved_extensions = true;
  bool load_command_line_extensions =
      extension_registrar_->extensions_enabled();
#if BUILDFLAG(IS_CHROMEOS)
  if (!ash::ProfileHelper::IsUserProfile(profile_)) {
    load_saved_extensions = false;
    load_command_line_extensions = false;
  }

  const bool load_autotest_ext =
      command_line_->HasSwitch(switches::kLoadSigninProfileTestExtension);
  const bool is_signin_profile = ash::ProfileHelper::IsSigninProfile(profile_);
  if (load_autotest_ext && is_signin_profile) {
    LoadSigninProfileTestExtension(command_line_->GetSwitchValueASCII(
        switches::kLoadSigninProfileTestExtension));
  }
#endif
  if (load_saved_extensions) {
    InstalledLoader(profile_).LoadAllExtensions();
  }

  CheckManagementPolicy();
  OnInstalledExtensionsLoaded();

  LoadExtensionsFromCommandLineFlag(::switches::kDisableExtensionsExcept);
  if (load_command_line_extensions) {
    bool command_line_blocked = true;
    if (base::FeatureList::IsEnabled(
            extensions_features::kDisableLoadExtensionCommandLineSwitch)) {
      LOG(WARNING)
          << "--load-extension is not allowed in Google Chrome, ignoring.";
    } else if (safe_browsing::IsEnhancedProtectionEnabled(
                   *profile_->GetPrefs())) {
      VLOG(1) << "--load-extension is not allowed for users opted into "
              << "Enhanced Safe Browsing, ignoring.";
    } else if (ShouldBlockCommandLineExtension(*profile_)) {
      // TODO(crbug.com/401529219): Deprecate this restriction once
      // --load-extension switch is restricted on Chrome builds.
      VLOG(1)
          << "--load-extension is not allowed for users that have the policy "
          << "ExtensionInstallTypeBlocklist::command_line, ignoring.";
    } else {
      LoadExtensionsFromCommandLineFlag(switches::kLoadExtension);
      command_line_blocked = false;
    }
    base::UmaHistogramBoolean("Extensions.LoadingFromCommandLineBlocked",
                              command_line_blocked);
  }
  EnabledReloadableExtensions();
  delayed_install_manager_->FinishInstallationsDelayedByShutdown();
  SetReadyAndNotifyListeners();

  UninstallMigratedExtensions();

  // TODO(erikkay): this should probably be deferred to a future point
  // rather than running immediately at startup.
  external_provider_manager_->CheckForExternalUpdates();

  safe_browsing_verdict_handler_.Init();

  // Must be called after extensions are loaded.
  allowlist_->Init();

  // Check for updates especially for corrupted user installed extension from
  // the webstore. This will do nothing if an extension update check was
  // triggered before and is still running.
  if (corrupted_extension_reinstaller_->HasAnyReinstallForCorruption()) {
    CheckForUpdatesSoon();
  }
}

void ExtensionService::EnabledReloadableExtensions() {
  TRACE_EVENT0("browser,startup",
               "ExtensionService::EnabledReloadableExtensions");
  extension_registrar_->EnabledReloadableExtensions();
}

void ExtensionService::LoadExtensionsFromCommandLineFlag(
    const char* switch_name) {
  if (command_line_->HasSwitch(switch_name)) {
    base::CommandLine::StringType path_list =
        command_line_->GetSwitchValueNative(switch_name);
    base::StringTokenizerT<base::CommandLine::StringType,
                           base::CommandLine::StringType::const_iterator>
        t(path_list, FILE_PATH_LITERAL(","));
    while (t.GetNext()) {
      std::string extension_id;
      UnpackedInstaller::Create(this)->LoadFromCommandLine(
          base::FilePath(t.token_piece()), &extension_id,
          false /*only-allow-apps*/);
      if (switch_name == ::switches::kDisableExtensionsExcept) {
        extension_registrar_->AddDisableFlagExemptedExtension(extension_id);
      }
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void ExtensionService::LoadSigninProfileTestExtension(const std::string& path) {
  base::SysInfo::CrashIfChromeOSNonTestImage();
  std::string extension_id;
  const bool installing = UnpackedInstaller::Create(this)->LoadFromCommandLine(
      base::FilePath(path), &extension_id, false /*only-allow-apps*/);
  CHECK(installing);
  CHECK_EQ(extension_id, extension_misc::kSigninProfileTestExtensionId)
      << extension_id
      << " extension not allowed to load from the command line in the "
         "signin profile";
}
#endif

void ExtensionService::ReloadExtension(const std::string& extension_id) {
  extension_registrar_->ReloadExtension(extension_id,
                                        LoadErrorBehavior::kNoisy);
}

void ExtensionService::ReloadExtensionWithQuietFailure(
    const std::string& extension_id) {
  extension_registrar_->ReloadExtension(extension_id,
                                        LoadErrorBehavior::kQuiet);
}

bool ExtensionService::UninstallExtension(
    // "transient" because the process of uninstalling may cause the reference
    // to become invalid. Instead, use |extension->id()|.
    const std::string& transient_extension_id,
    UninstallReason reason,
    std::u16string* error,
    base::OnceClosure done_callback) {
  return extension_registrar_->UninstallExtension(
      transient_extension_id, reason, error, std::move(done_callback));
}

void ExtensionService::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value::Dict& attributes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  omaha_attributes_handler_.PerformActionBasedOnOmahaAttributes(extension_id,
                                                                attributes);
  allowlist_->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
  // Show an error for the newly blocklisted extension.
  error_controller_->ShowErrorIfNeeded();
}

void ExtensionService::PerformActionBasedOnExtensionTelemetryServiceVerdicts(
    const Blocklist::BlocklistStateMap& blocklist_state_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  extension_telemetry_service_verdict_handler_.PerformActionBasedOnVerdicts(
      blocklist_state_map);
  error_controller_->ShowErrorIfNeeded();
}

void ExtensionService::OnGreylistStateRemoved(const std::string& extension_id) {
  extension_registrar_->OnGreylistStateRemoved(extension_id);
}

void ExtensionService::OnGreylistStateAdded(const std::string& extension_id,
                                            BitMapBlocklistState new_state) {
  extension_registrar_->OnGreylistStateAdded(extension_id, new_state);
}

void ExtensionService::OnBlocklistStateRemoved(
    const std::string& extension_id) {
  extension_registrar_->OnBlocklistStateRemoved(extension_id);
}

void ExtensionService::OnBlocklistStateAdded(const std::string& extension_id) {
  extension_registrar_->OnBlocklistStateAdded(extension_id);
}

void ExtensionService::RemoveDisableReasonAndMaybeEnable(
    const std::string& extension_id,
    disable_reason::DisableReason reason_to_remove) {
  extension_registrar_->RemoveDisableReasonAndMaybeEnable(extension_id,
                                                          reason_to_remove);
}

void ExtensionService::EnableExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_registrar_->EnableExtension(extension_id);
}

void ExtensionService::DisableExtension(
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) {
  DisableExtension(extension_id, DisableReasonSet({disable_reason}));
}

void ExtensionService::DisableExtension(
    const ExtensionId& extension_id,
    const DisableReasonSet& disable_reasons) {
  extension_registrar_->DisableExtension(extension_id, disable_reasons);
}

void ExtensionService::DisableExtensionWithRawReasons(
    ExtensionPrefs::DisableReasonRawManipulationPasskey,
    const ExtensionId& extension_id,
    const base::flat_set<int>& disable_reasons) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto passkey = ExtensionPrefs::DisableReasonRawManipulationPasskey();
  extension_registrar_->DisableExtensionWithRawReasons(passkey, extension_id,
                                                      disable_reasons);
}

void ExtensionService::DisableExtensionWithSource(
    const Extension* source_extension,
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) {
  extension_registrar_->DisableExtensionWithSource(
      source_extension, extension_id, disable_reason);
}

void ExtensionService::DisableUserExtensionsExcept(
    const std::vector<std::string>& except_ids) {
  ManagementPolicy* management_policy = system_->management_policy();
  ExtensionList to_disable;

  for (const auto& extension : registry_->enabled_extensions()) {
    if (management_policy->UserMayModifySettings(extension.get(), nullptr)) {
      to_disable.push_back(extension);
    }
  }

  for (const auto& extension : registry_->terminated_extensions()) {
    if (management_policy->UserMayModifySettings(extension.get(), nullptr)) {
      to_disable.push_back(extension);
    }
  }

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile());
  for (const auto& extension : to_disable) {
    if (extension->was_installed_by_default() &&
        extension_management->UpdatesFromWebstore(*extension)) {
      continue;
    }
    const std::string& id = extension->id();
    if (!base::Contains(except_ids, id)) {
      DisableExtension(id, disable_reason::DISABLE_USER_ACTION);
    }
  }
}

content::BrowserContext* ExtensionService::GetBrowserContext() const {
  // Implemented in the .cc file to avoid adding a profile.h dependency to
  // extension_service.h.
  return profile_;
}

void ExtensionService::CheckManagementPolicy() {
  std::map<std::string, disable_reason::DisableReason> to_disable;
  std::vector<std::string> to_enable;

  // Loop through the extensions list, finding extensions we need to disable.
  for (const auto& extension : registry_->enabled_extensions()) {
    disable_reason::DisableReason disable_reason = disable_reason::DISABLE_NONE;
    if (system_->management_policy()->MustRemainDisabled(extension.get(),
                                                         &disable_reason)) {
      DCHECK_NE(disable_reason, disable_reason::DISABLE_NONE);
      to_disable[extension->id()] = disable_reason;
    }
  }

  ExtensionManagement* management =
      ExtensionManagementFactory::GetForBrowserContext(profile());

  PermissionsUpdater(profile()).SetDefaultPolicyHostRestrictions(
      management->GetDefaultPolicyBlockedHosts(),
      management->GetDefaultPolicyAllowedHosts());

  for (const auto& extension : registry_->enabled_extensions()) {
    PermissionsUpdater(profile()).ApplyPolicyHostRestrictions(*extension);
  }

  ManifestV2ExperimentManager* mv2_experiment_manager =
      ManifestV2ExperimentManager::Get(profile_);

  // Loop through the disabled extension list, find extensions to re-enable
  // automatically. These extensions are exclusive from the |to_disable| list
  // constructed above, since disabled_extensions() and enabled_extensions() are
  // supposed to be mutually exclusive.
  for (const auto& extension : registry_->disabled_extensions()) {
    DisableReasonSet to_add;
    DisableReasonSet to_remove;

    // Find all extensions disabled due to minimum version requirement and
    // management policy but now satisfying it.
    if (management->CheckMinimumVersion(extension.get(), nullptr)) {
      to_remove.insert(disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY);
    }

    // Check published-in-store status against policy requirement and update
    // the disable reasons accordingly.
    if (management->IsAllowedByUnpublishedAvailabilityPolicy(extension.get())) {
      to_remove.insert(
          disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY);
    } else {
      to_add.insert(
          disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY);
    }

    if (management->IsAllowedByUnpackedDeveloperModePolicy(*extension)) {
      to_remove.insert(disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION);
    } else {
      to_add.insert(disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION);
    }

    // Check if the `DISABLE_NOT_VERIFIED` reason is still applicable. This
    // disable reason is used in multiple disabling flows (e.g., by
    // InstallVerifier and StandardManagementPolicyProvider). Only clear the
    // disable reason if none of these flows require the extension to be
    // disabled.
    // TODO(crbug.com/362756477): Refactor duplicated disable reason logic
    // between CheckManagementPolicy() and policy providers.
    disable_reason::DisableReason install_verifier_disable_reason =
        disable_reason::DISABLE_NONE;
    InstallVerifier::Get(GetBrowserContext())
        ->MustRemainDisabled(extension.get(), &install_verifier_disable_reason);
    if (install_verifier_disable_reason == disable_reason::DISABLE_NONE &&
        !management->ShouldBlockForceInstalledOffstoreExtension(*extension)) {
      to_remove.insert(disable_reason::DISABLE_NOT_VERIFIED);
    }

    if (!system_->management_policy()->MustRemainDisabled(extension.get(),
                                                          nullptr)) {
      to_remove.insert(disable_reason::DISABLE_BLOCKED_BY_POLICY);
    }

    // Note: `mv2_experiment_manager` may be null for certain types of profiles
    // (such as the sign-in profile). We can ignore this check in this case,
    // since users can't install extensions in these profiles.
    // TODO(https://crbug.com/362756477): As above, this is effectively
    // fragmenting logic between the policy provider and here to ensure that
    // the extension gets properly re-enabled when appropriate.
    if (mv2_experiment_manager &&
        mv2_experiment_manager->GetCurrentExperimentStage() ==
            MV2ExperimentStage::kUnsupported &&
        !mv2_experiment_manager->ShouldBlockExtensionEnable(*extension)) {
      to_remove.insert(disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);
    }

    // If this profile is not supervised, then remove any supervised user
    // related disable reasons.
    bool is_supervised = profile() && profile()->IsChild();
    if (!is_supervised) {
      to_remove.insert(disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
    }

    if (system_->management_policy()->MustRemainEnabled(extension.get(),
                                                        nullptr)) {
      // Extensions installed from the Windows Registry should re-enable when
      // they become force-installed. Normally this is handled in
      // OnExternalExtensionUpdateUrlFound(), but already-broken browsers (from
      // previous Chromium versions) also need to be fixed here.
      //
      // TODO(crbug.com/40144051): This won't be needed after a few milestones.
      // It should be safe to remove in M107.
      to_remove.insert(disable_reason::DISABLE_EXTERNAL_EXTENSION);
    }

    DisableReasonSet shared_disable_reasons =
        base::STLSetIntersection<DisableReasonSet>(to_remove, to_add);
    CHECK(shared_disable_reasons.empty())
        << "Found a disable reason in both `to_add` and `to_remove`: "
        << static_cast<int>(*shared_disable_reasons.begin());

    if (!to_add.empty()) {
      extension_prefs_->AddDisableReasons(extension->id(), to_add);
    }

    if (!to_remove.empty()) {
      extension_prefs_->RemoveDisableReasons(extension->id(), to_remove);
    }

    if (extension_prefs_->GetDisableReasons(extension->id()).empty()) {
      to_enable.push_back(extension->id());
    }
  }

  for (const auto& i : to_disable) {
    DisableExtension(i.first, i.second);
  }

  // No extension is getting re-enabled here after disabling because |to_enable|
  // is mutually exclusive to |to_disable|.
  for (const std::string& id : to_enable) {
    EnableExtension(id);
  }

  if (updater_ && updater_->enabled()) {
    // Find all extensions disabled due to minimum version requirement from
    // policy (including the ones that got disabled just now), and check
    // for update.
    ExtensionUpdater::CheckParams to_recheck;
    for (const auto& extension : registry_->disabled_extensions()) {
      if (extension_prefs_->HasOnlyDisableReason(
              extension->id(),
              disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY)) {
        // The minimum version check is the only thing holding this extension
        // back, so check if it can be updated to fix that.
        to_recheck.ids.push_back(extension->id());
      }
    }
    if (!to_recheck.ids.empty()) {
      updater_->CheckNow(std::move(to_recheck));
    }
  }

  // Check the disabled extensions to see if any should be force uninstalled.
  std::vector<ExtensionId> remove_list;
  for (const auto& extension : registry_->disabled_extensions()) {
    if (system_->management_policy()->ShouldForceUninstall(extension.get(),
                                                           nullptr /*error*/)) {
      remove_list.push_back(extension->id());
    }
  }
  for (auto extension_id : remove_list) {
    std::u16string error;
    if (!UninstallExtension(extension_id, UNINSTALL_REASON_INTERNAL_MANAGEMENT,
                            &error)) {
      SYSLOG(WARNING) << "Extension with id " << extension_id
                      << " failed to be uninstalled via policy: " << error;
    }
  }
}

void ExtensionService::CheckForUpdatesSoon() {
  // This can legitimately happen in unit tests.
  if (!updater_ || !updater_->enabled()) {
    return;
  }

  updater_->CheckSoon();
}

void ExtensionService::UnloadExtension(const std::string& extension_id,
                                       UnloadedExtensionReason reason) {
  extension_registrar_->RemoveExtension(extension_id, reason);
}

void ExtensionService::RemoveComponentExtension(
    const std::string& extension_id) {
  extension_registrar_->RemoveComponentExtension(extension_id);
}

void ExtensionService::UnloadAllExtensionsForTest() {
  UnloadAllExtensionsInternal();
}

void ExtensionService::ReloadExtensionsForTest() {
  // Calling UnloadAllExtensionsForTest here triggers a false-positive presubmit
  // warning about calling test code in production.
  UnloadAllExtensionsInternal();
  component_loader_->LoadAll();
  InstalledLoader(profile_).LoadAllExtensions();
  OnInstalledExtensionsLoaded();
  // Don't call SetReadyAndNotifyListeners() since tests call this multiple
  // times.
}

void ExtensionService::SetReadyAndNotifyListeners() {
  TRACE_EVENT0("browser,startup",
               "ExtensionService::SetReadyAndNotifyListeners");
  ready_->Signal();
}

void ExtensionService::AddExtension(const Extension* extension) {
  extension_registrar_->AddExtension(extension);
}

void ExtensionService::AddComponentExtension(const Extension* extension) {
  extension_registrar_->AddComponentExtension(extension);
}

void ExtensionService::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    base::Value::Dict ruleset_install_prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string& id = extension->id();
  base::flat_set<int> disable_reasons =
      extension_registrar_->GetDisableReasonsOnInstalled(extension);
  std::string install_parameter;
  const PendingExtensionInfo* pending_extension_info =
      pending_extension_manager_->GetById(id);
  bool is_reinstall_for_corruption =
      corrupted_extension_reinstaller_->IsReinstallForCorruptionExpected(
          extension->id());

  if (is_reinstall_for_corruption) {
    corrupted_extension_reinstaller_->MarkResolved(id);
  }

  if (pending_extension_info) {
    if (!pending_extension_info->ShouldAllowInstall(extension, profile())) {
      // Hack for crbug.com/558299, see comment on DeleteThemeDoNotUse.
      if (extension->is_theme() && pending_extension_info->is_from_sync()) {
        ExtensionSyncService::Get(profile_)->DeleteThemeDoNotUse(*extension);
      }

      pending_extension_manager_->Remove(id);

      ExtensionManagement* management =
          ExtensionManagementFactory::GetForBrowserContext(profile());
      LOG(WARNING) << "ShouldAllowInstall() returned false for " << id
                   << " of type " << extension->GetType() << " and update URL "
                   << management->GetEffectiveUpdateURL(*extension).spec()
                   << "; not installing";

      // Delete the extension directory since we're not going to
      // load it.
      if (!GetExtensionFileTaskRunner()->PostTask(
              FROM_HERE,
              base::GetDeletePathRecursivelyCallback(extension->path()))) {
        NOTREACHED();
      }
      return;
    }

    install_parameter = pending_extension_info->install_parameter();
    pending_extension_manager_->Remove(id);
  } else if (!is_reinstall_for_corruption) {
    // We explicitly want to re-enable an uninstalled external
    // extension; if we're here, that means the user is manually
    // installing the extension.
    if (extension_prefs_->IsExternalExtensionUninstalled(id)) {
      disable_reasons.clear();
    }
  }

  // If the old version of the extension was disabled due to corruption, this
  // new install may correct the problem.
  disable_reasons.erase(disable_reason::DISABLE_CORRUPTED);

  // Unsupported requirements overrides the management policy.
  if (install_flags & kInstallFlagHasRequirementErrors) {
    disable_reasons.insert(disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT);
  } else {
    // Requirement is supported now, remove the corresponding disable reason
    // instead.
    disable_reasons.erase(disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT);
  }

  // Check if the extension was disabled because of the minimum version
  // requirements from enterprise policy, and satisfies it now.
  if (ExtensionManagementFactory::GetForBrowserContext(profile())
          ->CheckMinimumVersion(extension, nullptr)) {
    // And remove the corresponding disable reason.
    disable_reasons.erase(disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY);
  }

  if (install_flags & kInstallFlagIsBlocklistedForMalware) {
    // Installation of a blocklisted extension can happen from sync, policy,
    // etc, where to maintain consistency we need to install it, just never
    // load it (see AddExtension). Usually it should be the job of callers to
    // intercept blocklisted extensions earlier (e.g. CrxInstaller, before even
    // showing the install dialogue).
    extension_prefs_->AcknowledgeBlocklistedExtension(id);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.SilentInstall",
                              extension->location());
  }

  bool is_user_profile =
      extensions::profile_util::ProfileCanUseNonComponentExtensions(profile_);

  if (!registry_->GetInstalledExtension(extension->id())) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType", extension->GetType(),
                              100);
    if (is_user_profile) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType.User",
                                extension->GetType(), 100);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType.NonUser",
                                extension->GetType(), 100);
    }
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource",
                              extension->location());
    if (is_user_profile) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource.User2",
                                extension->location(), 100);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource.NonUser2",
                                extension->location(), 100);
    }
    // TODO(crbug.com/40878021): Address Install metrics below in a follow-up
    // CL.
    InstalledLoader::RecordPermissionMessagesHistogram(extension, "Install",
                                                       is_user_profile);
  }

  allowlist()->OnExtensionInstalled(id, install_flags);

  ExtensionPrefs::DelayReason delay_reason;
  InstallGate::Action action =
      delayed_install_manager_->ShouldDelayExtensionInstall(
          extension, !!(install_flags & kInstallFlagInstallImmediately),
          &delay_reason);
  switch (action) {
    case InstallGate::INSTALL:
      extension_registrar_->AddNewOrUpdatedExtension(
          extension, disable_reasons, install_flags, page_ordinal,
          install_parameter, std::move(ruleset_install_prefs));
      return;
    case InstallGate::DELAY:
      extension_prefs_->SetDelayedInstallInfo(
          extension, disable_reasons, install_flags, delay_reason, page_ordinal,
          install_parameter, std::move(ruleset_install_prefs));

      // Transfer ownership of |extension|.
      delayed_install_manager_->Insert(extension);

      if (delay_reason == ExtensionPrefs::DelayReason::kWaitForIdle) {
        // Notify observers that app update is available.
        for (auto& observer : update_observers_) {
          observer.OnAppUpdateAvailable(extension);
        }
      }
      return;
    case InstallGate::ABORT:
      // Do nothing to abort the install. One such case is the shared module
      // service gets IMPORT_STATUS_UNRECOVERABLE status for the pending
      // install.
      return;
  }

  NOTREACHED() << "Unknown action for delayed install: " << action;
}

void ExtensionService::OnExtensionManagementSettingsChanged() {
  error_controller_->ShowErrorIfNeeded();

  // Revokes blocked permissions from active_permissions for all extensions.
  ExtensionManagement* settings =
      ExtensionManagementFactory::GetForBrowserContext(profile());
  CHECK(settings);
  const ExtensionSet all_extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : all_extensions) {
    if (!settings->IsPermissionSetAllowed(
            extension.get(),
            extension->permissions_data()->active_permissions()) &&
        extension_registrar_->CanBlockExtension(extension.get())) {
      PermissionsUpdater(profile()).RemovePermissionsUnsafe(
          extension.get(), *settings->GetBlockedPermissions(extension.get()));
    }
  }

  CheckManagementPolicy();

  // Request an out-of-cycle update of extension metadata information from CWS
  // if the ExtensionUnpublishedAvailability policy setting is such that
  // unpublished extensions should not be enabled. This update allows
  // unpublished extensions to be disabled sooner rather than waiting till the
  // next regularly scheduled fetch.
  if (profile_->GetPrefs()->GetInteger(
          pref_names::kExtensionUnpublishedAvailability) !=
      kAllowUnpublishedExtensions) {
    CWSInfoService::Get(profile_)->CheckAndMaybeFetchInfo();
  }
}

bool ExtensionService::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  return delayed_install_manager_->FinishDelayedInstallationIfReady(
      extension_id, install_immediately);
}

const Extension* ExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  return delayed_install_manager_->GetPendingExtensionUpdate(id);
}

void ExtensionService::TerminateExtension(const std::string& extension_id) {
  extension_registrar_->TerminateExtension(extension_id);
}

void ExtensionService::DidCreateMainFrameForBackgroundPage(
    ExtensionHost* host) {
  extension_registrar_->DidCreateMainFrameForBackgroundPage(host);
}

void ExtensionService::OnExtensionHostRenderProcessGone(
    content::BrowserContext* browser_context,
    ExtensionHost* extension_host) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));

  // Mark the extension as terminated and deactivated. We want it to
  // be in a consistent state: either fully working or not loaded
  // at all, but never half-crashed.  We do it in a PostTask so
  // that other handlers of this notification will still have
  // access to the Extension and ExtensionHost.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionService::TerminateExtension,
                                AsExtensionServiceWeakPtr(),
                                extension_host->extension_id()));
}

void ExtensionService::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
  }
}

void ExtensionService::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  // If you hit this from a KeyedService you might be missing a DependsOn()
  // for ChromeExtensionSystemFactory.
  CHECK(registry_) << "ExtensionService used after Shutdown()";

  host_observation_.RemoveObservation(host);

  Profile* host_profile =
      Profile::FromBrowserContext(host->GetBrowserContext());
  if (!profile_->IsSameOrParent(host_profile->GetOriginalProfile())) {
    return;
  }

  ProcessMap* process_map = ProcessMap::Get(profile_);

  // An extension process was terminated, this might have resulted in an
  // app or extension becoming idle.
  if (std::optional<std::string> extension_id =
          process_map->GetExtensionIdForProcess(host->GetDeprecatedID())) {
    // The extension running in this process might also be referencing a shared
    // module which is waiting for idle to update. Check all imports of this
    // extension too.
    std::set<std::string> affected_ids;
    affected_ids.insert(*extension_id);

    if (auto* extension = registry_->GetExtensionById(
            *extension_id, ExtensionRegistry::EVERYTHING)) {
      const std::vector<SharedModuleInfo::ImportInfo>& imports =
          SharedModuleInfo::GetImports(extension);
      for (const auto& import_info : imports) {
        affected_ids.insert(import_info.extension_id);
      }
    }

    for (const auto& id : affected_ids) {
      if (delayed_install_manager_->Contains(id)) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                base::IgnoreResult(
                    &ExtensionService::FinishDelayedInstallationIfReady),
                AsExtensionServiceWeakPtr(), id, false /*install_immediately*/),
            kUpdateIdleDelay);
      }
    }
  }
  process_map->Remove(host->GetDeprecatedID());
}

void ExtensionService::OnBlocklistUpdated() {
  blocklist_->GetBlocklistedIDs(
      registry_->GenerateInstalledExtensionsSet().GetIDs(),
      base::BindOnce(&ExtensionService::ManageBlocklist,
                     AsExtensionServiceWeakPtr()));
}

void ExtensionService::OnCWSInfoChanged() {
  CheckManagementPolicy();
}

void ExtensionService::OnUpgradeRecommended() {
  // Notify observers that chrome update is available.
  for (auto& observer : update_observers_) {
    observer.OnChromeUpdateAvailable();
  }
}

void ExtensionService::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  if (profile != profile_) {
    return;
  }

  ExtensionIdSet ids_to_unload = registry_->enabled_extensions().GetIDs();
  for (const auto& id : ids_to_unload) {
    UnloadExtension(id, UnloadedExtensionReason::PROFILE_SHUTDOWN);
  }
}

void ExtensionService::ManageBlocklist(
    const Blocklist::BlocklistStateMap& state_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  safe_browsing_verdict_handler_.ManageBlocklist(state_map);
  error_controller_->ShowErrorIfNeeded();
}

void ExtensionService::AddUpdateObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void ExtensionService::RemoveUpdateObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

bool ExtensionService::UserCanDisableInstalledExtension(
    const std::string& extension_id) {
  const Extension* extension = registry_->GetInstalledExtension(extension_id);
  return extension_registrar_delegate_->CanDisableExtension(extension);
}

// Used only by test code.
void ExtensionService::UnloadAllExtensionsInternal() {
  profile_->GetExtensionSpecialStoragePolicy()->RevokeRightsForAllExtensions();

  const ExtensionSet extensions = registry_->GenerateInstalledExtensionsSet(
      ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
      ExtensionRegistry::TERMINATED);

  for (const auto& extension : extensions) {
    UnloadExtension(extension->id(), UnloadedExtensionReason::UNINSTALL);
  }

  // TODO(erikkay) should there be a notification for this?  We can't use
  // EXTENSION_UNLOADED since that implies that the extension has
  // been disabled or uninstalled.
}

void ExtensionService::OnInstalledExtensionsLoaded() {
  if (updater_ && updater_->enabled()) {
    updater_->Start();
  }

  // Enable any Shared Modules that incorrectly got disabled previously.
  // This is temporary code to fix incorrect behavior from previous versions of
  // Chrome and can be removed after several releases (perhaps M60).
  ExtensionList to_enable;
  for (const auto& extension : registry_->disabled_extensions()) {
    if (SharedModuleInfo::IsSharedModule(extension.get())) {
      to_enable.push_back(extension);
    }
  }
  for (const auto& extension : to_enable) {
    EnableExtension(extension->id());
  }

  // Check installed extensions against the blocklist if and only if the
  // database is ready; otherwise, the database is effectively empty and we'll
  // re-enable all blocked extensions.

  blocklist_->IsDatabaseReady(base::BindOnce(
      [](base::WeakPtr<ExtensionService> service, bool is_ready) {
        DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
        if (!service || !is_ready) {
          // Either the service was torn down or the database isn't
          // ready yet (and is effectively empty). Either way, no need
          // to update the blocklisted extensions.
          return;
        }
        service->OnBlocklistUpdated();
      },
      AsExtensionServiceWeakPtr()));
}

void ExtensionService::UninstallMigratedExtensions() {
  extension_registrar_->UninstallMigratedExtensions(
      kObsoleteComponentExtensionIds);
}

void ExtensionService::OnDeveloperModePrefChanged() {
  CheckManagementPolicy();
}

}  // namespace extensions
