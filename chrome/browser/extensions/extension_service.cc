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

#include "base/barrier_closure.h"
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
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_action_storage_manager.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/installed_loader.h"
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
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/crx_file/id_util.h"
#include "components/favicon_base/favicon_url_parser.h"
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
#include "extensions/common/crash_keys.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#include "chrome/browser/ash/extensions/install_limiter.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "storage/browser/file_system/file_system_context.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace extensions {

using LoadErrorBehavior = ExtensionRegistrar::LoadErrorBehavior;

namespace {

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

// When uninstalling an extension, determine if the extension's directory
// should be deleted when uninstalling. Returns `true` iff extension is
// unpacked and installed outside the unpacked extensions installations dir.
// Example: packed extensions are always deleted. But unpacked extensions are
// in a folder outside the profile dir are not deleted.
bool SkipDeleteExtensionDir(const Extension& extension,
                            const base::FilePath& profile_path) {
  bool is_unpacked_location =
      Manifest::IsUnpackedLocation(extension.location());
  bool extension_dir_not_direct_subdir_of_unpacked_extensions_install_dir =
      extension.path().DirName() !=
      profile_path.AppendASCII(extensions::kUnpackedInstallDirectoryName);
  return is_unpacked_location &&
         extension_dir_not_direct_subdir_of_unpacked_extensions_install_dir;
}

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

  // Check if the providers know about this extension.
  for (const auto& provider : external_extension_providers_) {
    DCHECK(provider->IsReady());
    if (provider->HasExtension(id))
      return;  // Yup, known extension, don't uninstall.
  }

  // We get the list of external extensions to check from preferences.
  // It is possible that an extension has preferences but is not loaded.
  // For example, an extension that requires experimental permissions
  // will not be loaded if the experimental command line flag is not used.
  // In this case, do not uninstall.
  if (!registry_->GetInstalledExtension(id)) {
    // We can't call UninstallExtension with an unloaded/invalid
    // extension ID.
    LOG(WARNING) << "Attempted uninstallation of unloaded/invalid extension "
                 << "with id: " << id;
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
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs_);
  OnBlocklistStateAdded(extension_id);
}

void ExtensionService::GreylistExtensionForTest(
    const std::string& extension_id,
    const BitMapBlocklistState& state) {
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(extension_id, state,
                                                          extension_prefs_);
  OnGreylistStateAdded(extension_id, state);
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
        int disable_reasons =
            extension_prefs_->GetDisableReasons(info.extension_id);
        disable_reasons &= (~(disable_reason::DISABLE_USER_ACTION |
                              disable_reason::DISABLE_EXTERNAL_EXTENSION |
                              disable_reason::DISABLE_PERMISSIONS_INCREASE));
        extension_prefs_->ReplaceDisableReasons(info.extension_id,
                                                disable_reasons);

        // Only re-enable the extension if there are no other disable reasons.
        if (extension_prefs_->GetDisableReasons(info.extension_id) ==
            disable_reason::DISABLE_NONE) {
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
  if (!pending_extension_manager()->AddFromExternalUpdateUrl(
          info.extension_id, info.install_parameter, info.update_url,
          info.download_location, info.creation_flags,
          info.mark_acknowledged)) {
    // We can reach here if the extension from an equal or higher priority
    // source is already present in the |pending_extension_list_|. No need to
    // report the failure in this case.
    if (!pending_extension_manager()->IsIdPending(info.extension_id)) {
      install_stage_tracker->ReportFailure(
          info.extension_id,
          InstallStageTracker::FailureReason::PENDING_ADD_FAILED);
    }
    return false;
  }

  if (force_update)
    update_once_all_providers_are_ready_ = true;
  return true;
}

void ExtensionService::OnExternalProviderUpdateComplete(
    const ExternalProviderInterface* provider,
    const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
    const std::vector<ExternalInstallInfoFile>& file_extensions,
    const std::set<std::string>& removed_extensions) {
  // Update pending_extension_manager() with the new extensions first.
  for (const auto& extension : update_url_extensions)
    OnExternalExtensionUpdateUrlFound(extension, false);
  for (const auto& extension : file_extensions)
    OnExternalExtensionFileFound(extension);

#if DCHECK_IS_ON()
  for (const std::string& id : removed_extensions) {
    for (const auto& extension : update_url_extensions)
      DCHECK_NE(id, extension.extension_id);
    for (const auto& extension : file_extensions)
      DCHECK_NE(id, extension.extension_id);
  }
#endif

  // Then uninstall before running |updater_|.
  for (const std::string& id : removed_extensions)
    CheckExternalUninstall(id);

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
      omaha_attributes_handler_(extension_prefs,
                                ExtensionRegistry::Get(profile),
                                this),
      extension_telemetry_service_verdict_handler_(
          extension_prefs,
          ExtensionRegistry::Get(profile),
          this),
      registry_(ExtensionRegistry::Get(profile)),
      pending_extension_manager_(profile),
      install_directory_(install_directory),
      unpacked_install_directory_(unpacked_install_directory),
      extensions_enabled_(extensions_enabled),
      ready_(ready),
      shared_module_service_(new SharedModuleService(profile_)),
      extension_registrar_(profile_, this),
      force_installed_tracker_(registry_, profile_),
      force_installed_metrics_(registry_, profile_, &force_installed_tracker_),
      corrupted_extension_reinstaller_(profile_)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      ash_keeplist_manager_(profile, extension_prefs, this)
#endif
{
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::ExtensionService::ctor");

  // Figure out if extension installation should be enabled.
  if (ExtensionsBrowserClient::Get()->AreExtensionsDisabled(*command_line,
                                                            profile))
    extensions_enabled_ = false;

  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &ExtensionService::OnAppTerminating, base::Unretained(this)));

  host_registry_observation_.Observe(ExtensionHostRegistry::Get(profile));

  // The ProfileManager may be null in unit tests.
  if (g_browser_process->profile_manager())
    profile_manager_observation_.Observe(g_browser_process->profile_manager());

  UpgradeDetector::GetInstance()->AddObserver(this);

  if (base::FeatureList::IsEnabled(kCWSInfoService)) {
    cws_info_service_observation_.Observe(CWSInfoService::Get(profile_));
  }

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

  component_loader_ = std::make_unique<ComponentLoader>(system_, profile);

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

PendingExtensionManager* ExtensionService::pending_extension_manager() {
  return &pending_extension_manager_;
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
  for (const auto& provider : external_extension_providers_)
    provider->ServiceShutdown();
}

void ExtensionService::Shutdown() {
  if (base::FeatureList::IsEnabled(kCWSInfoService)) {
    cws_info_service_observation_.Reset();
  }
  ExtensionManagementFactory::GetForBrowserContext(profile())->RemoveObserver(
      this);
  external_install_manager_->Shutdown();
  corrupted_extension_reinstaller_.Shutdown();
  extension_registrar_.Shutdown();
  pref_change_registrar_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::Init");

  DCHECK(!system_->is_ready());  // Can't redo init.
  DCHECK_EQ(registry_->enabled_extensions().size(), 0u);

  component_loader_->LoadAll();
  bool load_saved_extensions = true;
  bool load_command_line_extensions = extensions_enabled_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  if (load_saved_extensions)
    InstalledLoader(this).LoadAllExtensions();

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
  MaybeFinishShutdownDelayed();
  SetReadyAndNotifyListeners();

  UninstallMigratedExtensions();

  // TODO(erikkay): this should probably be deferred to a future point
  // rather than running immediately at startup.
  CheckForExternalUpdates();

  safe_browsing_verdict_handler_.Init();

  // Must be called after extensions are loaded.
  allowlist_.Init();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash_keeplist_manager_.Init();
#endif

  // Check for updates especially for corrupted user installed extension from
  // the webstore. This will do nothing if an extension update check was
  // triggered before and is still running.
  if (corrupted_extension_reinstaller()->HasAnyReinstallForCorruption())
    CheckForUpdatesSoon();
}

void ExtensionService::EnabledReloadableExtensions() {
  TRACE_EVENT0("browser,startup",
               "ExtensionService::EnabledReloadableExtensions");

  std::vector<std::string> extensions_to_enable;
  for (const auto& e : registry_->disabled_extensions()) {
    if (extension_prefs_->GetDisableReasons(e->id()) ==
        disable_reason::DISABLE_RELOAD)
      extensions_to_enable.push_back(e->id());
  }
  for (const std::string& extension : extensions_to_enable) {
    EnableExtension(extension);
  }
}

void ExtensionService::MaybeFinishShutdownDelayed() {
  TRACE_EVENT0("browser,startup",
               "ExtensionService::MaybeFinishShutdownDelayed");

  const ExtensionPrefs::ExtensionsInfo delayed_info =
      extension_prefs_->GetAllDelayedInstallInfo();
  for (const auto& info : delayed_info) {
    scoped_refptr<const Extension> extension;
    if (info.extension_manifest) {
      std::string error;
      extension = Extension::Create(
          info.extension_path, info.extension_location,
          *info.extension_manifest,
          extension_prefs_->GetDelayedInstallCreationFlags(info.extension_id),
          info.extension_id, &error);
      if (extension.get()) {
        delayed_installs_.Insert(extension);
      }
    }
  }
  MaybeFinishDelayedInstallations();
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
      pending_extension_manager()->GetById(id);

  const Extension* extension = registry_->GetInstalledExtension(id);
  if (!pending_extension_info && !extension) {
    LOG(WARNING) << "Will not update extension " << id
                 << " because it is not installed or pending";
    // Delete extension_path since we're not creating a CrxInstaller
    // that would do it for us.
    if (file_ownership_passed &&
        !GetExtensionFileTaskRunner()->PostTask(
            FROM_HERE, base::GetDeleteFileCallback(file.path))) {
      NOTREACHED_IN_MIGRATION();
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
    if (pending_extension_info->mark_acknowledged())
      external_install_manager_->AcknowledgeExternalExtension(id);
    // If the extension was installed from or has migrated to the webstore, or
    // its auto-update URL is from the webstore, treat it as a webstore install.
    // Note that we ignore some older extensions with blank auto-update URLs
    // because we are mostly concerned with restrictions on NaCl extensions,
    // which are newer.
    if (!extension && extension_urls::IsWebstoreUpdateUrl(
                          pending_extension_info->update_url()))
      creation_flags |= Extension::FROM_WEBSTORE;
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
  if (crx_info_expected_version.IsValid())
    installer->set_expected_version(crx_info_expected_version,
                                    true /* fail_install_if_unexpected */);

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
      if (switch_name == ::switches::kDisableExtensionsExcept)
        disable_flag_exempted_extensions_.insert(extension_id);
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

// TODO(michaelpg): Group with other ExtensionRegistrar::Delegate overrides
// according to header file once diffs have settled down.
void ExtensionService::LoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path,
    LoadErrorBehavior load_error_behavior) {
  if (delayed_installs_.Contains(extension_id) &&
      FinishDelayedInstallationIfReady(extension_id,
                                       true /*install_immediately*/)) {
    return;
  }

  // If we're reloading a component extension, use the component extension
  // loader's reloader.
  if (component_loader_->Exists(extension_id)) {
    component_loader_->Reload(extension_id);
    return;
  }

  // Check the installed extensions to see if what we're reloading was already
  // installed.
  std::optional<ExtensionInfo> installed_extension(
      extension_prefs_->GetInstalledExtensionInfo(extension_id));
  if (installed_extension && installed_extension->extension_manifest.get()) {
    InstalledLoader(this).Load(*installed_extension, false);
  } else {
    // Otherwise, the extension is unpacked (location LOAD). We must load it
    // from the path.
    CHECK(!path.empty()) << "ExtensionRegistrar should never ask to load an "
                            "unknown extension with no path";
    scoped_refptr<UnpackedInstaller> unpacked_installer =
        UnpackedInstaller::Create(this);
    unpacked_installer->set_be_noisy_on_failure(load_error_behavior ==
                                                LoadErrorBehavior::kNoisy);
    unpacked_installer->set_completion_callback(
        base::BindOnce(&ExtensionService::OnUnpackedReloadFailure,
                       AsExtensionServiceWeakPtr()));
    unpacked_installer->Load(path);
  }
}

void ExtensionService::OnUnpackedReloadFailure(const Extension* extension,
                                               const base::FilePath& file_path,
                                               const std::string& error) {
  if (!error.empty())
    extension_registrar_.OnUnpackedExtensionReloadFailed(file_path);
}

void ExtensionService::ReloadExtension(const std::string& extension_id) {
  extension_registrar_.ReloadExtension(extension_id, LoadErrorBehavior::kNoisy);
}

void ExtensionService::ReloadExtensionWithQuietFailure(
    const std::string& extension_id) {
  extension_registrar_.ReloadExtension(extension_id, LoadErrorBehavior::kQuiet);
}

bool ExtensionService::UninstallExtension(
    // "transient" because the process of uninstalling may cause the reference
    // to become invalid. Instead, use |extension->id()|.
    const std::string& transient_extension_id,
    UninstallReason reason,
    std::u16string* error,
    base::OnceClosure done_callback) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<const Extension> extension =
      registry_->GetInstalledExtension(transient_extension_id);

  // Callers should not send us nonexistent extensions.
  CHECK(extension.get());

  ManagementPolicy* by_policy = system_->management_policy();
  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  // Shared modules being uninstalled will also set |external_uninstall| to true
  // so that we can guarantee users don't uninstall a shared module.
  // (crbug.com/273300)
  // TODO(rdevlin.cronin): This is probably not right. We should do something
  // else, like include an enum IS_INTERNAL_UNINSTALL or IS_USER_UNINSTALL so
  // we don't do this.
  bool external_uninstall =
      (reason == UNINSTALL_REASON_INTERNAL_MANAGEMENT) ||
      (reason == UNINSTALL_REASON_COMPONENT_REMOVED) ||
      (reason == UNINSTALL_REASON_MIGRATED) ||
      (reason == UNINSTALL_REASON_REINSTALL) ||
      (reason == UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION) ||
      (reason == UNINSTALL_REASON_ORPHANED_SHARED_MODULE);
  if (!external_uninstall &&
      (!by_policy->UserMayModifySettings(extension.get(), error) ||
       by_policy->MustRemainInstalled(extension.get(), error))) {
    ExtensionRegistry::Get(profile_)->TriggerOnUninstallationDenied(
        extension.get());
    return false;
  }

  InstallVerifier::Get(GetBrowserContext())->Remove(extension->id());

  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallType", extension->GetType(),
                            100);

  // Unload before doing more cleanup to ensure that nothing is hanging on to
  // any of these resources.
  UnloadExtension(extension->id(), UnloadedExtensionReason::UNINSTALL);

  // `UnloadExtension` ignores extensions that are `BLOCKLISTED` or `BLOCKED`
  if (registry_->blocklisted_extensions().Contains(extension->id()))
    registry_->RemoveBlocklisted(extension->id());
  if (registry_->blocked_extensions().Contains(extension->id()))
    registry_->RemoveBlocked(extension->id());

  // Prepare barrier closure for UninstallExtensionOnFileThread() task (if
  // applicable) and DataDeleter::StartDeleting().
  bool is_unpacked_location =
      Manifest::IsUnpackedLocation(extension->location());
  base::RepeatingClosure subtask_done_callback = base::DoNothing();
  if (!done_callback.is_null()) {
    int num_tasks = is_unpacked_location ? 1 : 2;
    subtask_done_callback =
        base::BarrierClosure(num_tasks, std::move(done_callback));
  }

  // Delete extensions in profile directory (from webstore, or from .crx), but
  // do not delete unpacked in a folder outside the profile directory.
  if (!SkipDeleteExtensionDir(*extension, profile_->GetPath())) {
    // Extensions installed from webstore or .crx are versioned in subdirs so we
    // delete the parent dir. Unpacked (installed from .zip rather than folder)
    // are not versioned so we just delete the single installation directory.
    base::FilePath extension_dir_to_delete =
        is_unpacked_location ? extension->path() : extension->path().DirName();

    base::FilePath extensions_install_dir =
        is_unpacked_location ? unpacked_install_directory_ : install_directory_;

    // Tell the backend to start deleting the installed extension on the file
    // thread.
    if (!GetExtensionFileTaskRunner()->PostTaskAndReply(
            FROM_HERE,
            base::BindOnce(&ExtensionService::UninstallExtensionOnFileThread,
                           extension->id(), profile_->GetProfileUserName(),
                           std::move(extensions_install_dir),
                           std::move(extension_dir_to_delete),
                           profile_->GetPath()),
            subtask_done_callback)) {
      NOTREACHED_IN_MIGRATION();
    }
  }

  DataDeleter::StartDeleting(profile_, extension.get(), subtask_done_callback);

  extension_registrar_.UntrackTerminatedExtension(extension->id());

  // Notify interested parties that we've uninstalled this extension.
  ExtensionRegistry::Get(profile_)->TriggerOnUninstalled(extension.get(),
                                                         reason);

  delayed_installs_.Remove(extension->id());
  extension_prefs_->OnExtensionUninstalled(
      extension->id(), extension->location(), external_uninstall);

  return true;
}

// static
void ExtensionService::UninstallExtensionOnFileThread(
    const std::string& id,
    const std::string& profile_user_name,
    const base::FilePath& extensions_install_dir,
    const base::FilePath& extension_dir_to_delete,
    const base::FilePath& profile_dir) {
  ExtensionAssetsManager* assets_manager =
      ExtensionAssetsManager::GetInstance();
  assets_manager->UninstallExtension(id, profile_user_name,
                                     extensions_install_dir,
                                     extension_dir_to_delete, profile_dir);
}

bool ExtensionService::IsExtensionEnabled(
    const std::string& extension_id) const {
  return extension_registrar_.IsExtensionEnabled(extension_id);
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
  bool is_on_sb_list = (blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                            extension_id, extension_prefs_) !=
                        BitMapBlocklistState::NOT_BLOCKLISTED);
  bool is_on_omaha_list =
      blocklist_prefs::HasAnyOmahaGreylistState(extension_id, extension_prefs_);
  if (is_on_sb_list || is_on_omaha_list) {
    return;
  }
  // Clear all acknowledged states so the extension will still get disabled if
  // it is added to the greylist again.
  blocklist_prefs::ClearAcknowledgedGreylistStates(extension_id,
                                                   extension_prefs_);
  RemoveDisableReasonAndMaybeEnable(extension_id,
                                    disable_reason::DISABLE_GREYLIST);
}

void ExtensionService::OnGreylistStateAdded(const std::string& extension_id,
                                            BitMapBlocklistState new_state) {
#if DCHECK_IS_ON()
  bool has_new_state_on_sb_list =
      (blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
           extension_id, extension_prefs_) == new_state);
  bool has_new_state_on_omaha_list = blocklist_prefs::HasOmahaBlocklistState(
      extension_id, new_state, extension_prefs_);
  DCHECK(has_new_state_on_sb_list || has_new_state_on_omaha_list);
#endif
  if (blocklist_prefs::HasAcknowledgedBlocklistState(extension_id, new_state,
                                                     extension_prefs_)) {
    // If the extension is already acknowledged, don't disable it again
    // because it can be already re-enabled by the user. This could happen if
    // the extension is added to the SafeBrowsing blocklist, and then
    // subsequently marked by Omaha. In this case, we don't want to disable the
    // extension twice.
    return;
  }

  // Set the current greylist states to acknowledge immediately because the
  // extension is disabled silently. Clear the other acknowledged state because
  // when the state changes to another greylist state in the future, we'd like
  // to disable the extension again.
  blocklist_prefs::UpdateCurrentGreylistStatesAsAcknowledged(extension_id,
                                                             extension_prefs_);
  DisableExtension(extension_id, disable_reason::DISABLE_GREYLIST);
}

void ExtensionService::OnBlocklistStateRemoved(
    const std::string& extension_id) {
  if (blocklist_prefs::IsExtensionBlocklisted(extension_id, extension_prefs_)) {
    return;
  }

  // Clear acknowledged state.
  blocklist_prefs::RemoveAcknowledgedBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs_);

  scoped_refptr<const Extension> extension =
      registry_->blocklisted_extensions().GetByID(extension_id);
  DCHECK(extension);
  registry_->RemoveBlocklisted(extension_id);
  AddExtension(extension.get());
}

void ExtensionService::OnBlocklistStateAdded(const std::string& extension_id) {
  DCHECK(
      blocklist_prefs::IsExtensionBlocklisted(extension_id, extension_prefs_));
  // The extension was already acknowledged by the user, it should already be in
  // the unloaded state.
  if (blocklist_prefs::HasAcknowledgedBlocklistState(
          extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
          extension_prefs_)) {
    DCHECK(base::Contains(registry_->blocklisted_extensions().GetIDs(),
                          extension_id));
    return;
  }

  scoped_refptr<const Extension> extension =
      registry_->GetInstalledExtension(extension_id);
  registry_->AddBlocklisted(extension);
  UnloadExtension(extension_id, UnloadedExtensionReason::BLOCKLIST);
}

void ExtensionService::RemoveDisableReasonAndMaybeEnable(
    const std::string& extension_id,
    disable_reason::DisableReason reason_to_remove) {
  auto disable_reason = extension_prefs_->GetDisableReasons(extension_id);
  if ((disable_reason & reason_to_remove) == 0)
    return;

  extension_prefs_->RemoveDisableReason(extension_id, reason_to_remove);
  if (disable_reason == reason_to_remove) {
    EnableExtension(extension_id);
  }
}

void ExtensionService::EnableExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_registrar_.EnableExtension(extension_id);
}

void ExtensionService::DisableExtension(const std::string& extension_id,
                                        int disable_reasons) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_registrar_.DisableExtension(extension_id, disable_reasons);
}

void ExtensionService::DisableExtensionWithSource(
    const Extension* source_extension,
    const std::string& extension_id,
    disable_reason::DisableReason disable_reasons) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(disable_reasons == disable_reason::DISABLE_USER_ACTION ||
         disable_reasons == disable_reason::DISABLE_BLOCKED_BY_POLICY);
  if (disable_reasons == disable_reason::DISABLE_BLOCKED_BY_POLICY) {
    DCHECK(Manifest::IsPolicyLocation(source_extension->location()) ||
           Manifest::IsComponentLocation(source_extension->location()));
  }

  const Extension* extension =
      registry_->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  CHECK(system_->management_policy()->ExtensionMayModifySettings(
      source_extension, extension, nullptr));
  extension_registrar_.DisableExtension(extension_id, disable_reasons);
}

void ExtensionService::DisableUserExtensionsExcept(
    const std::vector<std::string>& except_ids) {
  ManagementPolicy* management_policy = system_->management_policy();
  ExtensionList to_disable;

  for (const auto& extension : registry_->enabled_extensions()) {
    if (management_policy->UserMayModifySettings(extension.get(), nullptr))
      to_disable.push_back(extension);
  }

  for (const auto& extension : registry_->terminated_extensions()) {
    if (management_policy->UserMayModifySettings(extension.get(), nullptr))
      to_disable.push_back(extension);
  }

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile());
  for (const auto& extension : to_disable) {
    if (extension->was_installed_by_default() &&
        extension_management->UpdatesFromWebstore(*extension))
      continue;
    const std::string& id = extension->id();
    if (!base::Contains(except_ids, id))
      DisableExtension(id, disable_reason::DISABLE_USER_ACTION);
  }
}

// Extensions that are not locked, components or forced by policy should be
// locked. Extensions are no longer considered enabled or disabled. Blocklisted
// extensions are now considered both blocklisted and locked.
void ExtensionService::BlockAllExtensions() {
  if (block_extensions_)
    return;
  block_extensions_ = true;

  // Blocklisted extensions are already unloaded, need not be blocked.
  const ExtensionSet extensions = registry_->GenerateInstalledExtensionsSet(
      ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
      ExtensionRegistry::TERMINATED);

  for (const auto& extension : extensions) {
    const std::string& id = extension->id();

    if (!CanBlockExtension(extension.get()))
      continue;

    registry_->AddBlocked(extension.get());
    UnloadExtension(id, UnloadedExtensionReason::LOCK_ALL);
  }
}

// All locked extensions should revert to being either enabled or disabled
// as appropriate.
void ExtensionService::UnblockAllExtensions() {
  block_extensions_ = false;
  const ExtensionSet to_unblock =
      registry_->GenerateInstalledExtensionsSet(ExtensionRegistry::BLOCKED);

  for (const auto& extension : to_unblock) {
    registry_->RemoveBlocked(extension->id());
    AddExtension(extension.get());
  }
  // While extensions are blocked, we won't display any external install
  // warnings. Now that they are unblocked, we should update the error.
  external_install_manager_->UpdateExternalExtensionAlert();
}

void ExtensionService::GrantPermissionsAndEnableExtension(
    const Extension* extension) {
  GrantPermissions(extension);
  EnableExtension(extension->id());
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
  for (const PermissionID& id : permissions)
    base::UmaHistogramEnumeration(permissions_histogram_name, id.id());

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

// TODO(michaelpg): Group with other ExtensionRegistrar::Delegate overrides
// according to header file once diffs have settled down.
void ExtensionService::PostActivateExtension(
    scoped_refptr<const Extension> extension) {
  // Update policy permissions in case they were changed while extension was not
  // active.
  PermissionsUpdater(profile()).ApplyPolicyHostRestrictions(*extension);

  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  profile_->GetExtensionSpecialStoragePolicy()->GrantRightsForExtension(
      extension.get(), profile_);

  // TODO(kalman): This is broken. The crash reporter is process-wide so doesn't
  // work properly multi-profile. Besides which, it should be using
  // ExtensionRegistryObserver. See http://crbug.com/355029.
  UpdateActiveExtensionsInCrashReporter();

  const PermissionsData* permissions_data = extension->permissions_data();

  // If the extension has permission to load chrome://favicon/ resources we need
  // to make sure that the FaviconSource is registered with the
  // ChromeURLDataManager.
  if (permissions_data->HasHostPermission(GURL(chrome::kChromeUIFaviconURL))) {
    content::URLDataSource::Add(
        profile_, std::make_unique<FaviconSource>(
                      profile_, chrome::FaviconUrlFormat::kFaviconLegacy));
  }

  // Same for chrome://theme/ resources.
  if (permissions_data->HasHostPermission(GURL(chrome::kChromeUIThemeURL))) {
    content::URLDataSource::Add(profile_,
                                std::make_unique<ThemeSource>(profile_));
  }
}

// TODO(michaelpg): Group with other ExtensionRegistrar::Delegate overrides
// according to header file once diffs have settled down.
void ExtensionService::PostDeactivateExtension(
    scoped_refptr<const Extension> extension) {
  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  profile_->GetExtensionSpecialStoragePolicy()->RevokeRightsForExtension(
      extension.get(), profile_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Revoke external file access for the extension from its file system context.
  // It is safe to access the extension's storage partition at this point. The
  // storage partition may get destroyed only after the extension gets unloaded.
  storage::FileSystemContext* filesystem_context =
      util::GetStoragePartitionForExtensionId(extension->id(), profile_)
          ->GetFileSystemContext();
  if (filesystem_context && ash::FileSystemBackend::Get(*filesystem_context)) {
    ash::FileSystemBackend::Get(*filesystem_context)
        ->RevokeAccessForOrigin(extension->origin());
  }
#endif

  // TODO(kalman): This is broken. The crash reporter is process-wide so doesn't
  // work properly multi-profile. Besides which, it should be using
  // ExtensionRegistryObserver::OnExtensionLoaded. See http://crbug.com/355029.
  UpdateActiveExtensionsInCrashReporter();
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
    if (system_->management_policy()->MustRemainDisabled(
            extension.get(), &disable_reason, nullptr))
      to_disable[extension->id()] = disable_reason;
  }

  ExtensionManagement* management =
      ExtensionManagementFactory::GetForBrowserContext(profile());

  PermissionsUpdater(profile()).SetDefaultPolicyHostRestrictions(
      management->GetDefaultPolicyBlockedHosts(),
      management->GetDefaultPolicyAllowedHosts());

  for (const auto& extension : registry_->enabled_extensions()) {
    PermissionsUpdater(profile()).ApplyPolicyHostRestrictions(*extension);
  }

  // Loop through the disabled extension list, find extensions to re-enable
  // automatically. These extensions are exclusive from the |to_disable| list
  // constructed above, since disabled_extensions() and enabled_extensions() are
  // supposed to be mutually exclusive.
  for (const auto& extension : registry_->disabled_extensions()) {
    int disable_reasons = extension_prefs_->GetDisableReasons(extension->id());

    // Find all extensions disabled due to minimum version requirement and
    // management policy but now satisfying it.
    if (management->CheckMinimumVersion(extension.get(), nullptr)) {
      disable_reasons &= (~disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY);
    }

    // Check published-in-store status against policy requirement and update
    // the disable reasons accordingly.
    if (management->IsAllowedByUnpublishedAvailabilityPolicy(extension.get())) {
      disable_reasons &=
          ~disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY;
    } else {
      disable_reasons |=
          disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY;
    }

    if (management->IsAllowedByUnpackedDeveloperModePolicy(*extension)) {
      disable_reasons &=
          ~disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION;
    } else {
      disable_reasons |=
          disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION;
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
        ->MustRemainDisabled(extension.get(), &install_verifier_disable_reason,
                             nullptr);
    if (install_verifier_disable_reason == disable_reason::DISABLE_NONE &&
        !management->ShouldBlockForceInstalledOffstoreExtension(*extension)) {
      disable_reasons &= ~disable_reason::DISABLE_NOT_VERIFIED;
    }

    if (!system_->management_policy()->MustRemainDisabled(extension.get(),
                                                          nullptr, nullptr)) {
      disable_reasons &= (~disable_reason::DISABLE_BLOCKED_BY_POLICY);
    }

    // If this profile is not supervised, then remove any supervised user
    // related disable reasons.
    bool is_supervised = profile() && profile()->IsChild();
    if (!is_supervised) {
      disable_reasons &= (~disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
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
      disable_reasons &= (~disable_reason::DISABLE_EXTERNAL_EXTENSION);
    }

    extension_prefs_->ReplaceDisableReasons(extension->id(), disable_reasons);
    if (disable_reasons == disable_reason::DISABLE_NONE)
      to_enable.push_back(extension->id());
  }

  for (const auto& i : to_disable)
    DisableExtension(i.first, i.second);

  // No extension is getting re-enabled here after disabling because |to_enable|
  // is mutually exclusive to |to_disable|.
  for (const std::string& id : to_enable)
    EnableExtension(id);

  if (updater_.get()) {
    // Find all extensions disabled due to minimum version requirement from
    // policy (including the ones that got disabled just now), and check
    // for update.
    ExtensionUpdater::CheckParams to_recheck;
    for (const auto& extension : registry_->disabled_extensions()) {
      if (extension_prefs_->GetDisableReasons(extension->id()) ==
          disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY) {
        // The minimum version check is the only thing holding this extension
        // back, so check if it can be updated to fix that.
        to_recheck.ids.push_back(extension->id());
      }
    }
    if (!to_recheck.ids.empty())
      updater_->CheckNow(std::move(to_recheck));
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
  if (!updater_.get())
    return;

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
  if (g_external_updates_disabled_for_test_)
    return;

  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup", "ExtensionService::CheckForExternalUpdates");

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtension(File|UpdateUrl)Found.
  for (const auto& provider : external_extension_providers_)
    provider->VisitRegisteredExtension();

  // Do any required work that we would have done after completion of all
  // providers.
  if (external_extension_providers_.empty())
    OnAllExternalProvidersReady();
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
  if (AreAllExternalProvidersReady())
    OnAllExternalProvidersReady();
}

bool ExtensionService::AreAllExternalProvidersReady() const {
  for (const auto& provider : external_extension_providers_) {
    if (!provider->IsReady())
      return false;
  }
  return true;
}

void ExtensionService::OnAllExternalProvidersReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  InstallLimiter::Get(profile_)->OnAllExternalProvidersReady();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  extension_registrar_.RemoveExtension(extension_id, reason);
}

void ExtensionService::RemoveComponentExtension(
    const std::string& extension_id) {
  scoped_refptr<const Extension> extension(
      registry_->enabled_extensions().GetByID(extension_id));
  UnloadExtension(extension_id, UnloadedExtensionReason::UNINSTALL);
  if (extension.get()) {
    ExtensionRegistry::Get(profile_)->TriggerOnUninstalled(
        extension.get(), UNINSTALL_REASON_COMPONENT_REMOVED);
  }
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
  if (!Manifest::IsValidLocation(extension->location())) {
    // TODO(devlin): We should *never* add an extension with an invalid
    // location, but some bugs (e.g. crbug.com/692069) seem to indicate we do.
    // Track down the cases when this can happen, and remove this
    // DumpWithoutCrashing() (possibly replacing it with a CHECK).
    NOTREACHED_IN_MIGRATION();
    DEBUG_ALIAS_FOR_CSTR(extension_id_copy, extension->id().c_str(), 33);
    ManifestLocation location = extension->location();
    int creation_flags = extension->creation_flags();
    Manifest::Type type = extension->manifest()->type();
    base::debug::Alias(&location);
    base::debug::Alias(&creation_flags);
    base::debug::Alias(&type);
    base::debug::DumpWithoutCrashing();
    return;
  }

  // TODO(jstritar): We may be able to get rid of this branch by overriding the
  // default extension state to DISABLED when the --disable-extensions flag
  // is set (http://crbug.com/29067).
  if (!extensions_enabled_ &&
      !Manifest::ShouldAlwaysLoadExtension(extension->location(),
                                           extension->is_theme()) &&
      disable_flag_exempted_extensions_.count(extension->id()) == 0) {
    return;
  }

  extension_registrar_.AddExtension(extension);

  if (registry_->disabled_extensions().Contains(extension->id())) {
    // Show the extension disabled error if a permissions increase or a remote
    // installation is the reason it was disabled, and no other reasons exist.
    int reasons = extension_prefs_->GetDisableReasons(extension->id());
    const int kReasonMask = disable_reason::DISABLE_PERMISSIONS_INCREASE |
                            disable_reason::DISABLE_REMOTE_INSTALL;
    if (reasons & kReasonMask && !(reasons & ~kReasonMask)) {
      AddExtensionDisabledError(
          this, extension,
          extension_prefs_->HasDisableReason(
              extension->id(), disable_reason::DISABLE_REMOTE_INSTALL));
    }
  }
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
    AddNewOrUpdatedExtension(extension, Extension::ENABLED, kInstallFlagNone,
                             syncer::StringOrdinal(), std::string(),
                             /*ruleset_install_prefs=*/{});
    return;
  }

  AddExtension(extension);
}

void ExtensionService::CheckPermissionsIncrease(const Extension* extension,
                                                bool is_extension_loaded) {
  PermissionsUpdater(profile_).InitializePermissions(extension);

  // We keep track of all permissions the user has granted each extension.
  // This allows extensions to gracefully support backwards compatibility
  // by including unknown permissions in their manifests. When the user
  // installs the extension, only the recognized permissions are recorded.
  // When the unknown permissions become recognized (e.g., through browser
  // upgrade), we can prompt the user to accept these new permissions.
  // Extensions can also silently upgrade to less permissions, and then
  // silently upgrade to a version that adds these permissions back.
  //
  // For example, pretend that Chrome 10 includes a permission "omnibox"
  // for an API that adds suggestions to the omnibox. An extension can
  // maintain backwards compatibility while still having "omnibox" in the
  // manifest. If a user installs the extension on Chrome 9, the browser
  // will record the permissions it recognized, not including "omnibox."
  // When upgrading to Chrome 10, "omnibox" will be recognized and Chrome
  // will disable the extension and prompt the user to approve the increase
  // in privileges. The extension could then release a new version that
  // removes the "omnibox" permission. When the user upgrades, Chrome will
  // still remember that "omnibox" had been granted, so that if the
  // extension once again includes "omnibox" in an upgrade, the extension
  // can upgrade without requiring this user's approval.
  int disable_reasons = extension_prefs_->GetDisableReasons(extension->id());

  // Silently grant all active permissions to pre-installed apps and apps
  // installed in kiosk mode.
  bool auto_grant_permission =
      extension->was_installed_by_default() ||
      ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode();
  if (auto_grant_permission)
    GrantPermissions(extension);

  bool is_privilege_increase = false;
  // We only need to compare the granted permissions to the current permissions
  // if the extension has not been auto-granted its permissions above and is
  // installed internally.
  if (extension->location() == ManifestLocation::kInternal &&
      !auto_grant_permission) {
    // Add all the recognized permissions if the granted permissions list
    // hasn't been initialized yet.
    std::unique_ptr<const PermissionSet> granted_permissions =
        extension_prefs_->GetGrantedPermissions(extension->id());
    CHECK(granted_permissions.get());
    // We check the union of both granted permissions and runtime granted
    // permissions as it is possible for permissions which were withheld during
    // installation to have never entered the granted set, but to have later
    // been granted as runtime permissions.
    std::unique_ptr<const PermissionSet> runtime_granted_permissions =
        extension_prefs_->GetRuntimeGrantedPermissions(extension->id());
    std::unique_ptr<const PermissionSet> total_permissions =
        PermissionSet::CreateUnion(*granted_permissions,
                                   *runtime_granted_permissions);

    // Here, we check if an extension's privileges have increased in a manner
    // that requires the user's approval. This could occur because the browser
    // upgraded and recognized additional privileges, or an extension upgrades
    // to a version that requires additional privileges.
    is_privilege_increase =
        PermissionMessageProvider::Get()->IsPrivilegeIncrease(
            *total_permissions,
            extension->permissions_data()->active_permissions(),
            extension->GetType());

    // If there was no privilege increase, the extension might still have new
    // permissions (which either don't generate a warning message, or whose
    // warning messages are suppressed by existing permissions). Grant the new
    // permissions.
    if (!is_privilege_increase)
      GrantPermissions(extension);
  }

  bool previously_disabled =
      extension_prefs_->IsExtensionDisabled(extension->id());
  // TODO(devlin): Is the |is_extension_loaded| check needed here?
  if (is_extension_loaded && previously_disabled) {
    // Legacy disabled extensions do not have a disable reason. Infer that it
    // was likely disabled by the user.
    if (disable_reasons == disable_reason::DISABLE_NONE)
      disable_reasons |= disable_reason::DISABLE_USER_ACTION;
  }

  // If the extension is disabled due to a permissions increase, but does in
  // fact have all permissions, remove that disable reason.
  if (disable_reasons & disable_reason::DISABLE_PERMISSIONS_INCREASE &&
      !is_privilege_increase) {
    disable_reasons &= ~disable_reason::DISABLE_PERMISSIONS_INCREASE;
    extension_prefs_->RemoveDisableReason(
        extension->id(), disable_reason::DISABLE_PERMISSIONS_INCREASE);
  }

  // Extension has changed permissions significantly. Disable it. A
  // notification should be sent by the caller. If the extension is already
  // disabled because it was installed remotely, don't add another disable
  // reason.
  if (is_privilege_increase &&
      !(disable_reasons & disable_reason::DISABLE_REMOTE_INSTALL)) {
    disable_reasons |= disable_reason::DISABLE_PERMISSIONS_INCREASE;
  }

  if (disable_reasons == disable_reason::DISABLE_NONE)
    extension_prefs_->SetExtensionEnabled(extension->id());
  else
    extension_prefs_->SetExtensionDisabled(extension->id(), disable_reasons);
}

void ExtensionService::UpdateActiveExtensionsInCrashReporter() {
  std::set<std::string> extension_ids;
  for (const auto& extension : registry_->enabled_extensions()) {
    if (!extension->is_theme() &&
        extension->location() != ManifestLocation::kComponent)
      extension_ids.insert(extension->id());
  }

  // TODO(kalman): This is broken. ExtensionService is per-profile.
  // crash_keys::SetActiveExtensions is per-process. See
  // http://crbug.com/355029.
  crash_keys::SetActiveExtensions(extension_ids);
}

void ExtensionService::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    base::Value::Dict ruleset_install_prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string& id = extension->id();
  int disable_reasons = GetDisableReasonsOnInstalled(extension);
  std::string install_parameter;
  const PendingExtensionInfo* pending_extension_info =
      pending_extension_manager()->GetById(id);
  bool is_reinstall_for_corruption =
      corrupted_extension_reinstaller()->IsReinstallForCorruptionExpected(
          extension->id());

  if (is_reinstall_for_corruption)
    corrupted_extension_reinstaller()->MarkResolved(id);

  if (pending_extension_info) {
    if (!pending_extension_info->ShouldAllowInstall(extension, profile())) {
      // Hack for crbug.com/558299, see comment on DeleteThemeDoNotUse.
      if (extension->is_theme() && pending_extension_info->is_from_sync())
        ExtensionSyncService::Get(profile_)->DeleteThemeDoNotUse(*extension);

      pending_extension_manager()->Remove(id);

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
        NOTREACHED_IN_MIGRATION();
      }
      return;
    }

    install_parameter = pending_extension_info->install_parameter();
    pending_extension_manager()->Remove(id);
  } else if (!is_reinstall_for_corruption) {
    // We explicitly want to re-enable an uninstalled external
    // extension; if we're here, that means the user is manually
    // installing the extension.
    if (extension_prefs_->IsExternalExtensionUninstalled(id)) {
      disable_reasons = disable_reason::DISABLE_NONE;
    }
  }

  // If the old version of the extension was disabled due to corruption, this
  // new install may correct the problem.
  disable_reasons &= ~disable_reason::DISABLE_CORRUPTED;

  // Unsupported requirements overrides the management policy.
  if (install_flags & kInstallFlagHasRequirementErrors) {
    disable_reasons |= disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT;
  } else {
    // Requirement is supported now, remove the corresponding disable reason
    // instead.
    disable_reasons &= ~disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT;
  }

  // Check if the extension was disabled because of the minimum version
  // requirements from enterprise policy, and satisfies it now.
  if (ExtensionManagementFactory::GetForBrowserContext(profile())
          ->CheckMinimumVersion(extension, nullptr)) {
    // And remove the corresponding disable reason.
    disable_reasons &= ~disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY;
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

  const Extension::State initial_state =
      disable_reasons == disable_reason::DISABLE_NONE ? Extension::ENABLED
                                                      : Extension::DISABLED;
  if (initial_state == Extension::ENABLED)
    extension_prefs_->SetExtensionEnabled(id);
  else
    extension_prefs_->SetExtensionDisabled(id, disable_reasons);

  allowlist()->OnExtensionInstalled(id, install_flags);

  ExtensionPrefs::DelayReason delay_reason;
  InstallGate::Action action = ShouldDelayExtensionInstall(
      extension, !!(install_flags & kInstallFlagInstallImmediately),
      &delay_reason);
  switch (action) {
    case InstallGate::INSTALL:
      AddNewOrUpdatedExtension(extension, initial_state, install_flags,
                               page_ordinal, install_parameter,
                               std::move(ruleset_install_prefs));
      return;
    case InstallGate::DELAY:
      extension_prefs_->SetDelayedInstallInfo(
          extension, initial_state, install_flags, delay_reason, page_ordinal,
          install_parameter, std::move(ruleset_install_prefs));

      // Transfer ownership of |extension|.
      delayed_installs_.Insert(extension);

      if (delay_reason == ExtensionPrefs::DelayReason::kWaitForIdle) {
        // Notify observers that app update is available.
        for (auto& observer : update_observers_)
          observer.OnAppUpdateAvailable(extension);
      }
      return;
    case InstallGate::ABORT:
      // Do nothing to abort the install. One such case is the shared module
      // service gets IMPORT_STATUS_UNRECOVERABLE status for the pending
      // install.
      return;
  }

  NOTREACHED_IN_MIGRATION() << "Unknown action for delayed install: " << action;
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
        CanBlockExtension(extension.get())) {
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
  if (base::FeatureList::IsEnabled(kCWSInfoService)) {
    if (profile_->GetPrefs()->GetInteger(
            pref_names::kExtensionUnpublishedAvailability) !=
        kAllowUnpublishedExtensions) {
      CWSInfoService::Get(profile_)->CheckAndMaybeFetchInfo();
    }
  }
}

void ExtensionService::AddNewOrUpdatedExtension(
    const Extension* extension,
    Extension::State initial_state,
    int install_flags,
    const syncer::StringOrdinal& page_ordinal,
    const std::string& install_parameter,
    base::Value::Dict ruleset_install_prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_prefs_->OnExtensionInstalled(extension, initial_state, page_ordinal,
                                         install_flags, install_parameter,
                                         std::move(ruleset_install_prefs));
  delayed_installs_.Remove(extension->id());
  if (InstallVerifier::NeedsVerification(*extension, GetBrowserContext()))
    InstallVerifier::Get(GetBrowserContext())->VerifyExtension(extension->id());

  FinishInstallation(extension);
}

bool ExtensionService::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  // Check if the extension already got installed.
  const Extension* extension = delayed_installs_.GetByID(extension_id);
  if (!extension)
    return false;

  ExtensionPrefs::DelayReason reason;
  const InstallGate::Action action =
      ShouldDelayExtensionInstall(extension, install_immediately, &reason);
  switch (action) {
    case InstallGate::INSTALL:
      break;
    case InstallGate::DELAY:
      // Bail out and continue to delay the install.
      return false;
    case InstallGate::ABORT:
      delayed_installs_.Remove(extension_id);
      // Make sure no version of the extension is actually installed, (i.e.,
      // that this delayed install was not an update).
      CHECK(!extension_prefs_->GetInstalledExtensionInfo(extension_id));
      extension_prefs_->DeleteExtensionPrefs(extension_id);
      return false;
  }

  scoped_refptr<const Extension> delayed_install =
      GetPendingExtensionUpdate(extension_id);
  CHECK(delayed_install.get());
  delayed_installs_.Remove(extension_id);

  if (!extension_prefs_->FinishDelayedInstallInfo(extension_id))
    NOTREACHED_IN_MIGRATION();

  FinishInstallation(delayed_install.get());
  return true;
}

void ExtensionService::FinishInstallation(const Extension* extension) {
  const Extension* existing_extension =
      registry_->GetInstalledExtension(extension->id());
  bool is_update = false;
  std::string old_name;
  if (existing_extension) {
    is_update = true;
    old_name = existing_extension->name();
  }
  registry_->TriggerOnWillBeInstalled(extension, is_update, old_name);

  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  if (Manifest::ShouldAlwaysAllowFileAccess(extension->location()) &&
      !extension_prefs_->HasAllowFileAccessSetting(extension->id())) {
    extension_prefs_->SetAllowFileAccess(extension->id(), true);
  }

  AddExtension(extension);

  // Notify observers that need to know when an installation is complete.
  registry_->TriggerOnInstalled(extension, is_update);

  // Check extensions that may have been delayed only because this shared module
  // was not available.
  if (SharedModuleInfo::IsSharedModule(extension))
    MaybeFinishDelayedInstallations();
}

const Extension* ExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  return delayed_installs_.GetByID(id);
}

void ExtensionService::TerminateExtension(const std::string& extension_id) {
  extension_registrar_.TerminateExtension(extension_id);
}

bool ExtensionService::OnExternalExtensionFileFound(
    const ExternalInstallInfoFile& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(crx_file::id_util::IdIsValid(info.extension_id));
  if (extension_prefs_->IsExternalExtensionUninstalled(info.extension_id))
    return false;

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
                       << "that is older than current version. Current version "
                       << "is: " << existing->VersionString() << ". New "
                       << "version is: " << info.version.GetString()
                       << ". Keeping current version.";
          return false;
      }
    }
  }

  // If the extension is already pending, don't start an install.
  if (!pending_extension_manager()->AddFromExternalFile(
          info.extension_id, info.crx_location, info.version,
          info.creation_flags, info.mark_acknowledged)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (extension_misc::IsDemoModeChromeApp(info.extension_id)) {
    pending_extension_manager()->Remove(info.extension_id);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  InstallLimiter::Get(profile_)->Add(installer, file_info);
#else
  installer->InstallCrxFile(file_info);
#endif

  // Depending on the source, a new external extension might not need a user
  // notification on installation. For such extensions, mark them acknowledged
  // now to suppress the notification.
  if (info.mark_acknowledged)
    external_install_manager_->AcknowledgeExternalExtension(info.extension_id);

  return true;
}

void ExtensionService::InstallationFromExternalFileFinished(
    const std::string& extension_id,
    const std::optional<CrxInstallError>& error) {
  if (error != std::nullopt) {
    // When installation is finished, the extension should not remain in the
    // pending extension manager. For successful installations this is done in
    // OnExtensionInstalled handler.
    pending_extension_manager()->Remove(extension_id);
  }
}

void ExtensionService::DidCreateMainFrameForBackgroundPage(
    ExtensionHost* host) {
  extension_registrar_.DidCreateMainFrameForBackgroundPage(host);
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
  host_observation_.RemoveObservation(host);

  Profile* host_profile =
      Profile::FromBrowserContext(host->GetBrowserContext());
  if (!profile_->IsSameOrParent(host_profile->GetOriginalProfile()))
    return;

  ProcessMap* process_map = ProcessMap::Get(profile_);

  // An extension process was terminated, this might have resulted in an
  // app or extension becoming idle.
  if (std::optional<std::string> extension_id =
          process_map->GetExtensionIdForProcess(host->GetID())) {
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
      if (delayed_installs_.Contains(id)) {
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
  process_map->Remove(host->GetID());
}

int ExtensionService::GetDisableReasonsOnInstalled(const Extension* extension) {
  bool is_update_from_same_type = false;
  {
    const Extension* existing_extension =
        registry_->GetInstalledExtension(extension->id());
    is_update_from_same_type =
        existing_extension &&
        existing_extension->manifest()->type() == extension->manifest()->type();
  }
  disable_reason::DisableReason disable_reason = disable_reason::DISABLE_NONE;
  // Extensions disabled by management policy should always be disabled, even
  // if it's force-installed.
  if (system_->management_policy()->MustRemainDisabled(
          extension, &disable_reason, nullptr)) {
    // A specified reason is required to disable the extension.
    DCHECK(disable_reason != disable_reason::DISABLE_NONE);
    return disable_reason;
  }

  // Extensions installed by policy can't be disabled. So even if a previous
  // installation disabled the extension, make sure it is now enabled.
  if (system_->management_policy()->MustRemainEnabled(extension, nullptr))
    return disable_reason::DISABLE_NONE;

  // An already disabled extension should inherit the disable reasons and
  // remain disabled.
  if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    int disable_reasons = extension_prefs_->GetDisableReasons(extension->id());
    // If an extension was disabled without specified reason, presume it's
    // disabled by user.
    return disable_reasons == disable_reason::DISABLE_NONE
               ? disable_reason::DISABLE_USER_ACTION
               : disable_reasons;
  }

  if (ExternalInstallManager::IsPromptingEnabled()) {
    // External extensions are initially disabled. We prompt the user before
    // enabling them. Hosted apps are excepted because they are not dangerous
    // (they need to be launched by the user anyway). We also don't prompt for
    // extensions updating; this is because the extension will be disabled from
    // the initial install if it is supposed to be, and this allows us to turn
    // this on for other platforms without disabling already-installed
    // extensions.
    if (extension->GetType() != Manifest::TYPE_HOSTED_APP &&
        Manifest::IsExternalLocation(extension->location()) &&
        !extension_prefs_->IsExternalExtensionAcknowledged(extension->id()) &&
        !is_update_from_same_type) {
      return disable_reason::DISABLE_EXTERNAL_EXTENSION;
    }
  }

  return disable_reason::DISABLE_NONE;
}

// Helper method to determine if an extension can be blocked.
bool ExtensionService::CanBlockExtension(const Extension* extension) const {
  DCHECK(extension);
  return extension->location() != ManifestLocation::kComponent &&
         extension->location() != ManifestLocation::kExternalComponent &&
         !system_->management_policy()->MustRemainEnabled(extension, nullptr);
}

InstallGate::Action ExtensionService::ShouldDelayExtensionInstall(
    const Extension* extension,
    bool install_immediately,
    ExtensionPrefs::DelayReason* reason) const {
  for (const auto& entry : install_delayer_registry_) {
    InstallGate* const delayer = entry.second;
    InstallGate::Action action =
        delayer->ShouldDelay(extension, install_immediately);
    if (action != InstallGate::INSTALL) {
      *reason = entry.first;
      return action;
    }
  }

  return InstallGate::INSTALL;
}

void ExtensionService::MaybeFinishDelayedInstallations() {
  std::vector<std::string> to_be_installed;
  for (const auto& extension : delayed_installs_) {
    to_be_installed.push_back(extension->id());
  }
  for (const auto& extension_id : to_be_installed) {
    FinishDelayedInstallationIfReady(extension_id,
                                     false /*install_immediately*/);
  }
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
  for (auto& observer : update_observers_)
    observer.OnChromeUpdateAvailable();
}

void ExtensionService::PreAddExtension(const Extension* extension,
                                       const Extension* old_extension) {
  // An extension may have updated to no longer support incognito. When this
  // is the case, we don't show the toggle in the chrome://extensions page.
  // In order to ensure an extension doesn't keep an unrevokable permission,
  // reset the stored pref.
  if (old_extension && !IncognitoInfo::IsIncognitoAllowed(extension))
    extension_prefs_->SetIsIncognitoEnabled(extension->id(), false);

  // Check if the extension's privileges have changed and mark the
  // extension disabled if necessary.
  CheckPermissionsIncrease(extension, !!old_extension);
}

bool ExtensionService::CanEnableExtension(const Extension* extension) {
  return !system_->management_policy()->MustRemainDisabled(extension, nullptr,
                                                           nullptr);
}

bool ExtensionService::CanDisableExtension(const Extension* extension) {
  // Some extensions cannot be disabled by users:
  // - |extension| can be null if sync disables an extension that is not
  //   installed yet; allow disablement in this case.
  if (!extension)
    return true;

  // - Shared modules are just resources used by other extensions, and are not
  //   user-controlled.
  if (SharedModuleInfo::IsSharedModule(extension))
    return false;

  // - EXTERNAL_COMPONENT extensions are not generally modifiable by users, but
  //   can be uninstalled by the browser if the user sets extension-specific
  //   preferences.
  if (extension->location() == ManifestLocation::kExternalComponent)
    return true;

  return system_->management_policy()->UserMayModifySettings(extension,
                                                             nullptr);
}

bool ExtensionService::ShouldBlockExtension(const Extension* extension) {
  if (!block_extensions_)
    return false;

  // Blocked extensions aren't marked as such in prefs, thus if
  // |block_extensions_| is true then CanBlockExtension() must be called with an
  // Extension object. If |extension| is not loaded, assume it should be
  // blocked.
  return !extension || CanBlockExtension(extension);
}

void ExtensionService::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  if (profile != profile_)
    return;

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

void ExtensionService::RegisterInstallGate(ExtensionPrefs::DelayReason reason,
                                           InstallGate* install_delayer) {
  DCHECK(install_delayer_registry_.end() ==
         install_delayer_registry_.find(reason));
  install_delayer_registry_[reason] = install_delayer;
}

void ExtensionService::UnregisterInstallGate(InstallGate* install_delayer) {
  for (auto it = install_delayer_registry_.begin();
       it != install_delayer_registry_.end(); ++it) {
    if (it->second == install_delayer) {
      install_delayer_registry_.erase(it);
      return;
    }
  }
}

bool ExtensionService::UserCanDisableInstalledExtension(
    const std::string& extension_id) {
  const Extension* extension = registry_->GetInstalledExtension(extension_id);
  return CanDisableExtension(extension);
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
  if (updater_)
    updater_->Start();

  // Enable any Shared Modules that incorrectly got disabled previously.
  // This is temporary code to fix incorrect behavior from previous versions of
  // Chrome and can be removed after several releases (perhaps M60).
  ExtensionList to_enable;
  for (const auto& extension : registry_->disabled_extensions()) {
    if (SharedModuleInfo::IsSharedModule(extension.get()))
      to_enable.push_back(extension);
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
  const ExtensionSet installed_extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const std::string& extension_id : kObsoleteComponentExtensionIds) {
    auto* extension = installed_extensions.GetByID(extension_id);
    if (extension) {
      UninstallExtension(extension_id, UNINSTALL_REASON_COMPONENT_REMOVED,
                         nullptr);
      extension_prefs_->MarkObsoleteComponentExtensionAsRemoved(
          extension->id(), extension->location());
    }
  }
}

void ExtensionService::OnDeveloperModePrefChanged() {
  CheckManagementPolicy();
}

}  // namespace extensions
