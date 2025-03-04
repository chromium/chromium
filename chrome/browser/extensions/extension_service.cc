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
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/delayed_install_manager.h"
#include "chrome/browser/extensions/extension_action_storage_manager.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
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
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message_provider.h"
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

BASE_FEATURE(kCheckExternalExtensionInstallLocation,
             "CheckExternalExtensionInstallLocation",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool g_external_updates_disabled_for_test_ = false;

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

void ExtensionService::CheckExternalUninstall(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We get the list of external extensions to check from preferences.
  // It is possible that an extension has preferences but is not loaded.
  // For example, an extension that requires experimental permissions
  // will not be loaded if the experimental command line flag is not used.
  // In this case, do not uninstall.
  const Extension* extension = registry_->GetInstalledExtension(id);
  if (!extension) {
    // We can't call UninstallExtension with an unloaded/invalid
    // extension ID.
    LOG(WARNING) << "Checking uninstallation of unloaded/invalid extension "
                 << "with id: " << id;
    return;
  }

  // Check if the providers know about this extension.
  bool known_extension = false;
  for (const auto& provider : external_extension_providers_) {
    DCHECK(provider->IsReady());
    // TODO(https://crbug.com/397903880): Remove this if-check and always check
    // manifest location in M138.
    if (base::FeatureList::IsEnabled(kCheckExternalExtensionInstallLocation)) {
      if (provider->HasExtensionWithLocation(id, extension->location())) {
        known_extension = true;
        break;
      }
    } else if (provider->HasExtension(id)) {
      known_extension = true;
      break;
    }
  }
  if (known_extension) {
    // Yup, known extension, don't uninstall.
    return;
  }

  UninstallExtension(id, UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, nullptr);
}

void ExtensionService::ClearProvidersForTesting() {
  external_extension_providers_.clear();
}

void ExtensionService::AddProviderForTesting(
    std::unique_ptr<ExternalProviderInterface> test_provider) {
  CHECK(test_provider);
  external_extension_providers_.push_back(std::move(test_provider));
}

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

bool ExtensionService::OnExternalExtensionUpdateUrlFound(
    const ExternalInstallInfoUpdateUrl& info,
    bool force_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(crx_file::id_util::IdIsValid(info.extension_id));

  if (Manifest::IsExternalLocation(info.download_location)) {
    // All extensions that are not user specific can be cached.
    ExtensionsBrowserClient::Get()->GetExtensionCache()->AllowCaching(
        info.extension_id);
  }

  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);

  const Extension* extension = registry_->GetExtensionById(
      info.extension_id, ExtensionRegistry::EVERYTHING);
  if (extension) {
    // Already installed. Skip this install if the current location has higher
    // priority than |info.download_location|, and we aren't doing a
    // reinstall of a corrupt policy force-installed extension.
    ManifestLocation current = extension->location();
    if (!corrupted_extension_reinstaller()->IsReinstallForCorruptionExpected(
            info.extension_id) &&
        current == Manifest::GetHigherPriorityLocation(
                       current, info.download_location)) {
      install_stage_tracker->ReportFailure(
          info.extension_id,
          InstallStageTracker::FailureReason::ALREADY_INSTALLED);
      return false;
    }
    // If the installation is requested from a higher priority source, update
    // its install location.
    if (current !=
        Manifest::GetHigherPriorityLocation(current, info.download_location)) {
      UnloadExtension(info.extension_id, UnloadedExtensionReason::UPDATE);

      // Fetch the installation info from the prefs, and reload the extension
      // with a modified install location.
      std::optional<ExtensionInfo> installed_extension(
          extension_prefs_->GetInstalledExtensionInfo(info.extension_id));
      installed_extension->extension_location = info.download_location;

      // Load the extension with the new install location
      InstalledLoader(this).Load(*installed_extension, false);
      // Update the install location in the prefs.
      extension_prefs_->SetInstallLocation(info.extension_id,
                                           info.download_location);

      // If the extension was due to any of the following reasons, and it must
      // remain enabled, remove those reasons:
      // - Disabled by the user.
      // - User hasn't accepted a permissions increase.
      // - User hasn't accepted an external extension's prompt.
      if (registry_->disabled_extensions().GetByID(info.extension_id) &&
          system_->management_policy()->MustRemainEnabled(
              registry_->GetExtensionById(info.extension_id,
                                          ExtensionRegistry::EVERYTHING),
              nullptr)) {
        const DisableReasonSet to_remove = {
            disable_reason::DISABLE_USER_ACTION,
            disable_reason::DISABLE_EXTERNAL_EXTENSION,
            disable_reason::DISABLE_PERMISSIONS_INCREASE};
        extension_prefs_->RemoveDisableReasons(info.extension_id, to_remove);

        // Only re-enable the extension if there are no other disable reasons.
        if (extension_prefs_->GetDisableReasons(info.extension_id).empty()) {
          EnableExtension(info.extension_id);
        }
      }
      // If the extension is not corrupted, it is already installed with the
      // correct install location, so there is no need to add it to the pending
      // set of extensions. If the extension is corrupted, it should be
      // reinstalled, thus it should be added to the pending extensions for
      // installation.
      if (!corrupted_extension_reinstaller()->IsReinstallForCorruptionExpected(
              info.extension_id)) {
        return false;
      }
    }
    // Otherwise, overwrite the current installation.
  }

  // Add |info.extension_id| to the set of pending extensions.  If it can not
  // be added, then there is already a pending record from a higher-priority
  // install source.  In this case, signal that this extension will not be
  // installed by returning false.
  install_stage_tracker->ReportInstallationStage(
      info.extension_id, InstallStageTracker::Stage::PENDING);
  if (!pending_extension_manager_->AddFromExternalUpdateUrl(
          info.extension_id, info.install_parameter, info.update_url,
          info.download_location, info.creation_flags,
          info.mark_acknowledged)) {
    // We can reach here if the extension from an equal or higher priority
    // source is already present in the |pending_extension_list_|. No need to
    // report the failure in this case.
    if (!pending_extension_manager_->IsIdPending(info.extension_id)) {
      install_stage_tracker->ReportFailure(
          info.extension_id,
          InstallStageTracker::FailureReason::PENDING_ADD_FAILED);
    }
    return false;
  }

  if (force_update) {
    update_once_all_providers_are_ready_ = true;
  }
  return true;
}

void ExtensionService::OnExternalProviderUpdateComplete(
    const ExternalProviderInterface* provider,
    const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
    const std::vector<ExternalInstallInfoFile>& file_extensions,
    const std::set<std::string>& removed_extensions) {
  // Update pending_extension_manager_ with the new extensions first.
  for (const auto& extension : update_url_extensions) {
    OnExternalExtensionUpdateUrlFound(extension, false);
  }
  for (const auto& extension : file_extensions) {
    OnExternalExtensionFileFound(extension);
  }

#if DCHECK_IS_ON()
  for (const std::string& id : removed_extensions) {
    for (const auto& extension : update_url_extensions) {
      DCHECK_NE(id, extension.extension_id);
    }
    for (const auto& extension : file_extensions) {
      DCHECK_NE(id, extension.extension_id);
    }
  }
#endif

  // Then uninstall before running |updater_|.
  for (const std::string& id : removed_extensions) {
    CheckExternalUninstall(id);
  }

  if (!update_url_extensions.empty() && updater_) {
    // Empty params will cause pending extensions to be updated.
    updater_->CheckNow(ExtensionUpdater::CheckParams());
  }

  error_controller_->ShowErrorIfNeeded();
  external_install_manager_->UpdateExternalExtensionAlert();
}

ExtensionService::ExtensionService(
    Profile* profile,
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    const base::FilePath& unpacked_install_directory,
    ExtensionPrefs* extension_prefs,
    Blocklist* blocklist,
    bool autoupdate_enabled,
    bool extensions_enabled,
    base::OneShotEvent* ready)
    : Blocklist::Observer(blocklist),
      command_line_(command_line),
      profile_(profile),
      system_(ExtensionSystem::Get(profile)),
      extension_prefs_(extension_prefs),
      blocklist_(blocklist),
      allowlist_(profile_, extension_prefs, this),
      safe_browsing_verdict_handler_(extension_prefs,
                                     ExtensionRegistry::Get(profile),
                                     this),
      extension_telemetry_service_verdict_handler_(
          extension_prefs,
          ExtensionRegistry::Get(profile),
          this),
      registry_(ExtensionRegistry::Get(profile)),
      pending_extension_manager_(PendingExtensionManager::Get(profile)),
      install_directory_(install_directory),
      unpacked_install_directory_(unpacked_install_directory),
      extensions_enabled_(extensions_enabled),
      ready_(ready),
      component_loader_(std::make_unique<ComponentLoader>(system_, profile_)),
      shared_module_service_(new SharedModuleService(profile_)),
      extension_registrar_delegate_(
          std::make_unique<ChromeExtensionRegistrarDelegate>(
              profile_,
              this,
              component_loader_.get(),
              install_directory,
              unpacked_install_directory)),
      extension_registrar_(ExtensionRegistrar::Get(profile)),
      omaha_attributes_handler_(extension_prefs,
                                ExtensionRegistry::Get(profile),
                                this,
                                extension_registrar_),
      force_installed_tracker_(registry_, profile_),
      force_installed_metrics_(registry_, profile_, &force_installed_tracker_),
      corrupted_extension_reinstaller_(profile_),
      delayed_install_manager_(extension_prefs_, extension_registrar_) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::ExtensionService::ctor");
  extension_registrar_delegate_->Init(extension_registrar_,
                                      &delayed_install_manager_);
  extension_registrar_->SetDelegate(extension_registrar_delegate_.get());
  // Figure out if extension installation should be enabled.
  if (ExtensionsBrowserClient::Get()->AreExtensionsDisabled(*command_line,
                                                            profile)) {
    extensions_enabled_ = false;
  }

  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &ExtensionService::OnAppTerminating, base::Unretained(this)));

  host_registry_observation_.Observe(ExtensionHostRegistry::Get(profile));

  // The ProfileManager may be null in unit tests.
  if (g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
  }

  UpgradeDetector::GetInstance()->AddObserver(this);

  cws_info_service_observation_.Observe(CWSInfoService::Get(profile_));

  ExtensionManagementFactory::GetForBrowserContext(profile_)->AddObserver(this);

  // Set up the ExtensionUpdater.
  if (autoupdate_enabled) {
    updater_ = std::make_unique<ExtensionUpdater>(
        this, extension_prefs, profile->GetPrefs(), profile,
        kDefaultUpdateFrequencySeconds,
        ExtensionsBrowserClient::Get()->GetExtensionCache(),
        base::BindRepeating(ChromeExtensionDownloaderFactory::CreateForProfile,
                            profile));
  }

  if (extensions_enabled_) {
    ExternalProviderImpl::CreateExternalProviders(
        this, profile_, &external_extension_providers_);
  }

  // Set this as the ExtensionService for app sorting to ensure it causes syncs
  // if required.
  is_first_run_ = !extension_prefs_->SetAlertSystemFirstRun();

  error_controller_ =
      std::make_unique<ExtensionErrorController>(profile_, is_first_run_);
  external_install_manager_ =
      std::make_unique<ExternalInstallManager>(profile_, is_first_run_);

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

CorruptedExtensionReinstaller*
ExtensionService::corrupted_extension_reinstaller() {
  return &corrupted_extension_reinstaller_;
}

base::WeakPtr<ExtensionServiceInterface> ExtensionService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ExtensionService::~ExtensionService() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
  // No need to unload extensions here because they are profile-scoped, and the
  // profile is in the process of being deleted.
  for (const auto& provider : external_extension_providers_) {
    provider->ServiceShutdown();
  }
}

void ExtensionService::Shutdown() {
  delayed_install_manager_.Shutdown();
  cws_info_service_observation_.Reset();
  ExtensionManagementFactory::GetForBrowserContext(profile())->RemoveObserver(
      this);
  external_install_manager_->Shutdown();
  corrupted_extension_reinstaller_.Shutdown();
  extension_registrar_->Shutdown();
  extension_registrar_delegate_->Shutdown();
  pref_change_registrar_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Avoid dangling pointers.
  command_line_ = nullptr;
  system_ = nullptr;
  extension_prefs_ = nullptr;
  blocklist_ = nullptr;
  registry_ = nullptr;
  pending_extension_manager_ = nullptr;
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::Init");

  DCHECK(!system_->is_ready());  // Can't redo init.
  DCHECK_EQ(registry_->enabled_extensions().size(), 0u);

  component_loader_->LoadAll();
  bool load_saved_extensions = true;
  bool load_command_line_extensions = extensions_enabled_;
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
    InstalledLoader(this).LoadAllExtensions();
  }

  CheckManagementPolicy();
  OnInstalledExtensionsLoaded();

  LoadExtensionsFromCommandLineFlag(::switches::kDisableExtensionsExcept);
  if (load_command_line_extensions) {
    if (safe_browsing::IsEnhancedProtectionEnabled(*profile_->GetPrefs())) {
      VLOG(1) << "--load-extension is not allowed for users opted into "
              << "Enhanced Safe Browsing, ignoring.";
    } else if (ShouldBlockCommandLineExtension(*profile_)) {
      VLOG(1)
          << "--load-extension is not allowed for users that have the policy "
          << "have the policy ExtensionInstallTypeBlocklist::command_line, "
          << "ignoring.";
    } else {
      LoadExtensionsFromCommandLineFlag(switches::kLoadExtension);
    }
  }
  EnabledReloadableExtensions();
  delayed_install_manager_.FinishInstallationsDelayedByShutdown();
  SetReadyAndNotifyListeners();

  UninstallMigratedExtensions();

  // TODO(erikkay): this should probably be deferred to a future point
  // rather than running immediately at startup.
  CheckForExternalUpdates();

  safe_browsing_verdict_handler_.Init();

  // Must be called after extensions are loaded.
  allowlist_.Init();

  // Check for updates especially for corrupted user installed extension from
  // the webstore. This will do nothing if an extension update check was
  // triggered before and is still running.
  if (corrupted_extension_reinstaller()->HasAnyReinstallForCorruption()) {
    CheckForUpdatesSoon();
  }
}

void ExtensionService::EnabledReloadableExtensions() {
  TRACE_EVENT0("browser,startup",
               "ExtensionService::EnabledReloadableExtensions");
  extension_registrar_->EnabledReloadableExtensions();
}

scoped_refptr<CrxInstaller> ExtensionService::CreateUpdateInstaller(
    const CRXFileInfo& file,
    bool file_ownership_passed) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (browser_terminating_) {
    LOG(WARNING) << "Skipping UpdateExtension due to browser shutdown";
    // Leak the temp file at extension_path. We don't want to add to the disk
    // I/O burden at shutdown, we can't rely on the I/O completing anyway, and
    // the file is in the OS temp directory which should be cleaned up for us.
    return nullptr;
  }

  const std::string& id = file.extension_id;

  const PendingExtensionInfo* pending_extension_info =
      pending_extension_manager_->GetById(id);

  const Extension* extension = registry_->GetInstalledExtension(id);
  if (!pending_extension_info && !extension) {
    LOG(WARNING) << "Will not update extension " << id
                 << " because it is not installed or pending";
    // Delete extension_path since we're not creating a CrxInstaller
    // that would do it for us.
    if (file_ownership_passed &&
        !GetExtensionFileTaskRunner()->PostTask(
            FROM_HERE, base::GetDeleteFileCallback(file.path))) {
      NOTREACHED();
    }

    return nullptr;
  }
  // Either |pending_extension_info| or |extension| or both must not be null.
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(this));
  installer->set_expected_id(id);
  installer->set_expected_hash(file.expected_hash);
  int creation_flags = Extension::NO_FLAGS;
  if (pending_extension_info) {
    installer->set_install_source(pending_extension_info->install_source());
    installer->set_allow_silent_install(true);
    // If the extension came in disabled due to a permission increase, then
    // don't grant it all the permissions. crbug.com/484214
    bool has_permissions_increase =
        ExtensionPrefs::Get(profile_)->HasDisableReason(
            id, disable_reason::DISABLE_PERMISSIONS_INCREASE);
    const base::Version& expected_version = pending_extension_info->version();
    if (has_permissions_increase || pending_extension_info->remote_install() ||
        !expected_version.IsValid()) {
      installer->set_grant_permissions(false);
    } else {
      installer->set_expected_version(expected_version,
                                      false /* fail_install_if_unexpected */);
    }
    creation_flags = pending_extension_info->creation_flags();
    if (pending_extension_info->mark_acknowledged()) {
      external_install_manager_->AcknowledgeExternalExtension(id);
    }
    // If the extension was installed from or has migrated to the webstore, or
    // its auto-update URL is from the webstore, treat it as a webstore install.
    // Note that we ignore some older extensions with blank auto-update URLs
    // because we are mostly concerned with restrictions on NaCl extensions,
    // which are newer.
    if (!extension && extension_urls::IsWebstoreUpdateUrl(
                          pending_extension_info->update_url())) {
      creation_flags |= Extension::FROM_WEBSTORE;
    }
  } else {
    // |extension| must not be null.
    installer->set_install_source(extension->location());
  }

  if (extension) {
    installer->InitializeCreationFlagsForUpdate(extension, creation_flags);
    installer->set_do_not_sync(extension_prefs_->DoNotSync(id));
  } else {
    installer->set_creation_flags(creation_flags);
  }

  // If CRXFileInfo has a valid version from the manifest fetch result, it
  // should take priority over the one in pending extension info.
  base::Version crx_info_expected_version(file.expected_version);
  if (crx_info_expected_version.IsValid()) {
    installer->set_expected_version(crx_info_expected_version,
                                    true /* fail_install_if_unexpected */);
  }

  installer->set_delete_source(file_ownership_passed);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_UPDATE);

  return installer;
}

base::AutoReset<bool> ExtensionService::DisableExternalUpdatesForTesting() {
  return base::AutoReset<bool>(&g_external_updates_disabled_for_test_, true);
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
        disable_flag_exempted_extensions_.insert(extension_id);
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

bool ExtensionService::IsExtensionEnabled(
    const std::string& extension_id) const {
  return extension_registrar_->IsExtensionEnabled(extension_id);
}

void ExtensionService::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value::Dict& attributes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  omaha_attributes_handler_.PerformActionBasedOnOmahaAttributes(extension_id,
                                                                attributes);
  allowlist_.PerformActionBasedOnOmahaAttributes(extension_id, attributes);
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

// Extensions that are not locked, components or forced by policy should be
// locked. Extensions are no longer considered enabled or disabled. Blocklisted
// extensions are now considered both blocklisted and locked.
void ExtensionService::BlockAllExtensions() {
  if (block_extensions_) {
    return;
  }
  block_extensions_ = true;

  extension_registrar_->BlockAllExtensions();
}

// All locked extensions should revert to being either enabled or disabled
// as appropriate.
void ExtensionService::UnblockAllExtensions() {
  block_extensions_ = false;

  extension_registrar_->UnblockAllExtensions();

  // While extensions are blocked, we won't display any external install
  // warnings. Now that they are unblocked, we should update the error.
  external_install_manager_->UpdateExternalExtensionAlert();
}

void ExtensionService::GrantPermissions(const Extension* extension) {
  CHECK(extension);
  PermissionsUpdater(profile()).GrantActivePermissions(extension);
}

// static
void ExtensionService::RecordPermissionMessagesHistogram(
    const Extension* extension,
    const char* histogram_basename,
    bool log_user_profile_histograms) {
  PermissionIDSet permissions =
      PermissionMessageProvider::Get()->GetAllPermissionIDs(
          extension->permissions_data()->active_permissions(),
          extension->GetType());
  base::UmaHistogramBoolean(
      base::StringPrintf("Extensions.HasPermissions_%s3", histogram_basename),
      !permissions.empty());

  std::string permissions_histogram_name =
      base::StringPrintf("Extensions.Permissions_%s3", histogram_basename);
  for (const PermissionID& id : permissions) {
    base::UmaHistogramEnumeration(permissions_histogram_name, id.id());
  }

  if (log_user_profile_histograms) {
    base::UmaHistogramBoolean(
        base::StringPrintf("Extensions.HasPermissions_%s4", histogram_basename),
        !permissions.empty());

    std::string permissions_histogram_name_incremented =
        base::StringPrintf("Extensions.Permissions_%s4", histogram_basename);
    for (const PermissionID& id : permissions) {
      base::UmaHistogramEnumeration(permissions_histogram_name_incremented,
                                    id.id());
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

  if (updater_.get()) {
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
  if (!updater_.get()) {
    return;
  }

  updater_->CheckSoon();
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package. To support
// these, the extension will register its location in the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
// Errors are reported through LoadErrorReporter. Success is not reported.
void ExtensionService::CheckForExternalUpdates() {
  if (g_external_updates_disabled_for_test_) {
    return;
  }

  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::CheckForExternalUpdates");

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtension(File|UpdateUrl)Found.
  for (const auto& provider : external_extension_providers_) {
    provider->VisitRegisteredExtension();
  }

  // Do any required work that we would have done after completion of all
  // providers.
  if (external_extension_providers_.empty()) {
    OnAllExternalProvidersReady();
  }
}

void ExtensionService::ReinstallProviderExtensions() {
  for (const auto& provider : external_extension_providers_) {
    provider->TriggerOnExternalExtensionFound();
  }
}

void ExtensionService::OnExternalProviderReady(
    const ExternalProviderInterface* provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(provider->IsReady());

  // An external provider has finished loading.  We only take action
  // if all of them are finished. So we check them first.
  if (AreAllExternalProvidersReady()) {
    OnAllExternalProvidersReady();
  }
}

bool ExtensionService::AreAllExternalProvidersReady() const {
  for (const auto& provider : external_extension_providers_) {
    if (!provider->IsReady()) {
      return false;
    }
  }
  return true;
}

void ExtensionService::OnAllExternalProvidersReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS)
  auto* install_limiter = InstallLimiter::Get(profile_);
  if (install_limiter) {
    install_limiter->OnAllExternalProvidersReady();
  }

#endif  // BUILDFLAG(IS_CHROMEOS)

  // Install any pending extensions.
  if (update_once_all_providers_are_ready_ && updater()) {
    update_once_all_providers_are_ready_ = false;
    ExtensionUpdater::CheckParams params;
    params.callback = external_updates_finished_callback_
                          ? std::move(external_updates_finished_callback_)
                          : base::OnceClosure();
    updater()->CheckNow(std::move(params));
  } else if (external_updates_finished_callback_) {
    std::move(external_updates_finished_callback_).Run();
  }

  // Uninstall all the unclaimed extensions.
  ExtensionPrefs::ExtensionsInfo extensions_info =
      extension_prefs_->GetInstalledExtensionsInfo();
  for (const auto& info : extensions_info) {
    if (Manifest::IsExternalLocation(info.extension_location)) {
      CheckExternalUninstall(info.extension_id);
    }
  }

  error_controller_->ShowErrorIfNeeded();

  external_install_manager_->UpdateExternalExtensionAlert();
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
  InstalledLoader(this).LoadAllExtensions();
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
  extension_prefs_->ClearInapplicableDisableReasonsForComponentExtension(
      extension->id());
  const std::string old_version_string(
      extension_prefs_->GetVersionString(extension->id()));
  const base::Version old_version(old_version_string);

  VLOG(1) << "AddComponentExtension " << extension->name();
  if (!old_version.IsValid() || old_version != extension->version()) {
    VLOG(1) << "Component extension " << extension->name() << " ("
            << extension->id() << ") installing/upgrading from '"
            << old_version_string << "' to "
            << extension->version().GetString();

    // TODO(crbug.com/40508457): If needed, add support for Declarative Net
    // Request to component extensions and pass the ruleset install prefs here.
    AddNewOrUpdatedExtension(extension, {}, kInstallFlagNone,
                             syncer::StringOrdinal(), std::string(),
                             /*ruleset_install_prefs=*/{});
    return;
  }

  AddExtension(extension);
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
      corrupted_extension_reinstaller()->IsReinstallForCorruptionExpected(
          extension->id());

  if (is_reinstall_for_corruption) {
    corrupted_extension_reinstaller()->MarkResolved(id);
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
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource.User",
                                extension->GetType(), 100);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource.NonUser",
                                extension->GetType(), 100);
    }
    // TODO(crbug.com/40878021): Address Install metrics below in a follow-up
    // CL.
    RecordPermissionMessagesHistogram(extension, "Install", is_user_profile);
  }

  allowlist()->OnExtensionInstalled(id, install_flags);

  ExtensionPrefs::DelayReason delay_reason;
  InstallGate::Action action =
      delayed_install_manager_.ShouldDelayExtensionInstall(
          extension, !!(install_flags & kInstallFlagInstallImmediately),
          &delay_reason);
  switch (action) {
    case InstallGate::INSTALL:
      AddNewOrUpdatedExtension(extension, disable_reasons, install_flags,
                               page_ordinal, install_parameter,
                               std::move(ruleset_install_prefs));
      return;
    case InstallGate::DELAY:
      extension_prefs_->SetDelayedInstallInfo(
          extension, disable_reasons, install_flags, delay_reason, page_ordinal,
          install_parameter, std::move(ruleset_install_prefs));

      // Transfer ownership of |extension|.
      delayed_install_manager_.Insert(extension);

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

void ExtensionService::AddNewOrUpdatedExtension(
    const Extension* extension,
    const base::flat_set<int>& disable_reasons,
    int install_flags,
    const syncer::StringOrdinal& page_ordinal,
    const std::string& install_parameter,
    base::Value::Dict ruleset_install_prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_prefs_->OnExtensionInstalled(
      extension, disable_reasons, page_ordinal, install_flags,
      install_parameter, std::move(ruleset_install_prefs));
  delayed_install_manager_.Remove(extension->id());
  if (InstallVerifier::NeedsVerification(*extension, GetBrowserContext())) {
    InstallVerifier::Get(GetBrowserContext())->VerifyExtension(extension->id());
  }

  extension_registrar_->FinishInstallation(extension);
}

bool ExtensionService::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  return delayed_install_manager_.FinishDelayedInstallationIfReady(
      extension_id, install_immediately);
}

const Extension* ExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  return delayed_install_manager_.GetPendingExtensionUpdate(id);
}

void ExtensionService::TerminateExtension(const std::string& extension_id) {
  extension_registrar_->TerminateExtension(extension_id);
}

bool ExtensionService::OnExternalExtensionFileFound(
    const ExternalInstallInfoFile& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(crx_file::id_util::IdIsValid(info.extension_id));
  if (extension_prefs_->IsExternalExtensionUninstalled(info.extension_id)) {
    return false;
  }

  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  const Extension* existing = registry_->GetExtensionById(
      info.extension_id, ExtensionRegistry::EVERYTHING);

  if (existing) {
    // The pre-installed apps will have the location set as INTERNAL. Since
    // older pre-installed apps are installed as EXTERNAL, we override them.
    // However, if the app is already installed as internal, then do the version
    // check.
    // TODO(grv) : Remove after Q1-2013.
    bool is_preinstalled_apps_migration =
        (info.crx_location == mojom::ManifestLocation::kInternal &&
         Manifest::IsExternalLocation(existing->location()));

    if (!is_preinstalled_apps_migration) {
      switch (existing->version().CompareTo(info.version)) {
        case -1:  // existing version is older, we should upgrade
          break;
        case 0:  // existing version is same, do nothing
          return false;
        case 1:  // existing version is newer, uh-oh
          LOG(WARNING) << "Found external version of extension "
                       << info.extension_id
                       << " that is older than current version. Current version"
                       << " is: " << existing->VersionString() << ". New "
                       << "version is: " << info.version.GetString()
                       << ". Keeping current version.";
          return false;
      }
    }
  }

  // If the extension is already pending, don't start an install.
  if (!pending_extension_manager_->AddFromExternalFile(
          info.extension_id, info.crx_location, info.version,
          info.creation_flags, info.mark_acknowledged)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (extension_misc::IsDemoModeChromeApp(info.extension_id)) {
    pending_extension_manager_->Remove(info.extension_id);
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // no client (silent install)
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(this));
  installer->AddInstallerCallback(
      base::BindOnce(&ExtensionService::InstallationFromExternalFileFinished,
                     AsExtensionServiceWeakPtr(), info.extension_id));
  installer->set_install_source(info.crx_location);
  installer->set_expected_id(info.extension_id);
  installer->set_expected_version(info.version,
                                  true /* fail_install_if_unexpected */);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_EXTERNAL_FILE);
  installer->set_install_immediately(info.install_immediately);
  installer->set_creation_flags(info.creation_flags);

  CRXFileInfo file_info(
      info.path, info.crx_location == mojom::ManifestLocation::kExternalPolicy
                     ? GetPolicyVerifierFormat()
                     : GetExternalVerifierFormat());
#if BUILDFLAG(IS_CHROMEOS)
  auto* install_limiter = InstallLimiter::Get(profile_);
  if (install_limiter) {
    install_limiter->Add(installer, file_info);
  } else {
    installer->InstallCrxFile(file_info);
  }
#else
  installer->InstallCrxFile(file_info);
#endif

  // Depending on the source, a new external extension might not need a user
  // notification on installation. For such extensions, mark them acknowledged
  // now to suppress the notification.
  if (info.mark_acknowledged) {
    external_install_manager_->AcknowledgeExternalExtension(info.extension_id);
  }

  return true;
}

void ExtensionService::InstallationFromExternalFileFinished(
    const std::string& extension_id,
    const std::optional<CrxInstallError>& error) {
  if (error != std::nullopt) {
    // When installation is finished, the extension should not remain in the
    // pending extension manager. For successful installations this is done in
    // OnExtensionInstalled handler.
    pending_extension_manager_->Remove(extension_id);
  }
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

void ExtensionService::OnAppTerminating() {
  // Shutdown has started. Don't start any more extension installs.
  // (We cannot use ExtensionService::Shutdown() for this because it
  // happens too late in browser teardown.)
  browser_terminating_ = true;
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
      if (delayed_install_manager_.Contains(id)) {
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
  if (updater_) {
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
