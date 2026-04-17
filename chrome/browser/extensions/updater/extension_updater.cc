// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/extensions/updater/extension_updater_factory.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/forced_extensions/install_stage_tracker.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pending_extension_info.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/time/time.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using base::RandDouble;
using base::UnguessableToken;
using content::BrowserThread;
using Error = extensions::ExtensionDownloaderDelegate::Error;
using PingResult = extensions::ExtensionDownloaderDelegate::PingResult;

namespace extensions {

namespace {

// For sanity checking on update frequency - enforced in release mode only.
#if defined(NDEBUG)
constexpr base::TimeDelta kMinUpdateFrequency = base::Seconds(30);
#endif
constexpr base::TimeDelta kMaxUpdateFrequency = base::Days(7);

bool g_skip_scheduled_checks_for_tests = false;

bool g_force_use_update_service_for_tests = false;

// When we've computed a days value, we want to make sure we don't send a
// negative value (due to the system clock being set backwards, etc.), since -1
// is a special sentinel value that means "never pinged", and other negative
// values don't make sense.
int SanitizeDays(int days) {
  return days < 0 ? 0 : days;
}

// Calculates the value to use for the ping days parameter.
int CalculatePingDaysForExtension(const base::Time& last_ping_day) {
  int days = extensions::ManifestFetchData::kNeverPinged;
  if (!last_ping_day.is_null()) {
    days = SanitizeDays((base::Time::Now() - last_ping_day).InDays());
  }
  return days;
}

int CalculateActivePingDays(const base::Time& last_active_ping_day,
                            bool hasActiveBit) {
  if (!hasActiveBit) {
    return 0;
  }
  if (last_active_ping_day.is_null()) {
    return extensions::ManifestFetchData::kNeverPinged;
  }
  return SanitizeDays((base::Time::Now() - last_active_ping_day).InDays());
}

std::string GetUpdateURLData(const extensions::ExtensionPrefs* prefs,
                             const std::string& extension_id) {
  std::string data;
  prefs->ReadPrefAsString(extension_id, extensions::kUpdateURLData, &data);
  return data;
}

}  // namespace

ExtensionUpdater::CheckParams::CheckParams() = default;

ExtensionUpdater::CheckParams::~CheckParams() = default;

ExtensionUpdater::CheckParams::CheckParams(
    ExtensionUpdater::CheckParams&& other) = default;
ExtensionUpdater::CheckParams& ExtensionUpdater::CheckParams::operator=(
    ExtensionUpdater::CheckParams&& other) = default;

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const std::set<int>& request_ids,
    InstallCallback callback)
    : info(file),
      file_ownership_passed(file_ownership_passed),
      request_ids(request_ids),
      callback(std::move(callback)) {}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile()
    : file_ownership_passed(true) {}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(FetchedCRXFile&& other) =
    default;

ExtensionUpdater::FetchedCRXFile& ExtensionUpdater::FetchedCRXFile::operator=(
    FetchedCRXFile&& other) = default;

ExtensionUpdater::FetchedCRXFile::~FetchedCRXFile() = default;

ExtensionUpdater::InProgressCheck::InProgressCheck() = default;

ExtensionUpdater::InProgressCheck::~InProgressCheck() = default;

// static
ExtensionUpdater* ExtensionUpdater::Get(
    content::BrowserContext* browser_context) {
  return ExtensionUpdaterFactory::GetForBrowserContext(browser_context);
}

ExtensionUpdater::ExtensionUpdater(Profile* profile)
    : profile_(profile),
      registry_(ExtensionRegistry::Get(profile)),
      registrar_(ExtensionRegistrar::Get(profile)),
      delayed_install_manager_(DelayedInstallManager::Get(profile)),
      pending_extension_manager_(PendingExtensionManager::Get(profile)),
      external_install_manager_(ExternalInstallManager::Get(profile)),
      corrupted_extension_reinstaller_(
          CorruptedExtensionReinstaller::Get(profile)) {}

void ExtensionUpdater::InitAndEnable(
    ExtensionPrefs* extension_prefs,
    PrefService* prefs,
    base::TimeDelta frequency,
    ExtensionCache* cache,
    const ExtensionDownloader::Factory& downloader_factory) {
  enabled_ = true;
  downloader_factory_ = downloader_factory;
  frequency_ = frequency;
  extension_prefs_ = extension_prefs;
  prefs_ = prefs;
  extension_cache_ = cache;
  DCHECK_LE(frequency_, kMaxUpdateFrequency);
#if defined(NDEBUG)
  // In Release mode we enforce that update checks don't happen too often.
  frequency_ = std::max(frequency_, kMinUpdateFrequency);
#endif
  frequency_ = std::min(frequency_, kMaxUpdateFrequency);
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &ExtensionUpdater::OnAppTerminating, base::Unretained(this)));
}

ExtensionUpdater::~ExtensionUpdater() {
  Stop();
}

void ExtensionUpdater::Shutdown() {
  Stop();
}

void ExtensionUpdater::EnsureDownloaderCreated() {
  if (!downloader_.get()) {
    downloader_ = downloader_factory_.Run(this);
  }
  if (!update_service_) {
    update_service_ = UpdateService::Get(profile_);
  }
}

void ExtensionUpdater::Start() {
  CHECK(enabled_);
  DCHECK(!alive_);
  // If these are NULL, then that means we've been called after Stop()
  // has been called.
  DCHECK(extension_prefs_);
  DCHECK(prefs_);
  DCHECK(profile_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  DCHECK(registry_);
  alive_ = true;
  // Check soon, and set up the first delayed check.
  if (!g_skip_scheduled_checks_for_tests) {
    CheckSoon();
    ScheduleNextCheck();
  }
}

void ExtensionUpdater::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  alive_ = false;
  extension_prefs_ = nullptr;
  prefs_ = nullptr;
  profile_ = nullptr;
  will_check_soon_ = false;
  downloader_.reset();
  update_service_ = nullptr;
  registry_ = nullptr;
  registrar_ = nullptr;
  delayed_install_manager_ = nullptr;
  pending_extension_manager_ = nullptr;
  external_install_manager_ = nullptr;
  extension_cache_ = nullptr;
  corrupted_extension_reinstaller_ = nullptr;
}

void ExtensionUpdater::ScheduleNextCheck() {
  DCHECK(alive_);
  // Jitter the frequency by +/- 20%.
  const double jitter_factor = RandDouble() * 0.4 + 0.8;
  base::TimeDelta delay = base::Milliseconds(
      static_cast<int64_t>(frequency_.InMilliseconds() * jitter_factor));
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&ExtensionUpdater::NextCheck,
                                       weak_ptr_factory_.GetWeakPtr()),
                        delay);
}

void ExtensionUpdater::NextCheck() {
  if (!alive_) {
    return;
  }
  CheckNow(CheckParams());
  ScheduleNextCheck();
}

void ExtensionUpdater::CheckSoon() {
  DCHECK(alive_);
  if (will_check_soon_) {
    return;
  }
  if (content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&ExtensionUpdater::DoCheckSoon,
                                    weak_ptr_factory_.GetWeakPtr()))) {
    will_check_soon_ = true;
  } else {
    NOTREACHED();
  }
}

bool ExtensionUpdater::WillCheckSoon() const {
  return will_check_soon_;
}

void ExtensionUpdater::AddObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void ExtensionUpdater::RemoveObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

void ExtensionUpdater::NotifyChromeUpdateAvailable() {
  for (auto& observer : update_observers_) {
    observer.OnChromeUpdateAvailable();
  }
}

void ExtensionUpdater::NotifyAppUpdateAvailable(const Extension& extension) {
  for (auto& observer : update_observers_) {
    observer.OnAppUpdateAvailable(extension);
  }
}

void ExtensionUpdater::SetExtensionCacheForTesting(
    ExtensionCache* extension_cache) {
  extension_cache_ = extension_cache;
}

void ExtensionUpdater::SetExtensionDownloaderForTesting(
    std::unique_ptr<ExtensionDownloader> downloader) {
  downloader_.swap(downloader);
}

void ExtensionUpdater::SetBackoffPolicyForTesting(
    const net::BackoffEntry::Policy& backoff_policy) {
  EnsureDownloaderCreated();
  downloader_->SetBackoffPolicy(backoff_policy);
}

// static
base::AutoReset<bool> ExtensionUpdater::GetScopedUseUpdateServiceForTesting() {
  return base::AutoReset<bool>(&g_force_use_update_service_for_tests, true);
}

void ExtensionUpdater::SetUpdatingStartedCallbackForTesting(
    base::RepeatingClosure callback) {
  updating_started_callback_ = callback;
}

void ExtensionUpdater::SetCrxInstallerResultCallbackForTesting(
    CrxInstaller::InstallerResultCallback callback) {
  installer_result_callback_for_testing_ = std::move(callback);
}

void ExtensionUpdater::DoCheckSoon() {
  if (!will_check_soon_) {
    // Another caller called CheckNow() between CheckSoon() and now. Skip this
    // check.
    return;
  }
  CheckNow(CheckParams());
}

ExtensionDownloaderTask ExtensionUpdater::ToDownloaderTask(
    const Extension& extension,
    int request_id,
    DownloadFetchPriority fetch_priority,
    bool is_corrupt_reinstall) {
  // Note: webstore will ignore the UpdateURLData (and there's no reason for
  // a developer to set it for a store-installed extension, but it's safe if
  // they do).
  return ExtensionDownloaderTask(
      extension.id(), GetEffectiveUpdateURL(extension), extension.location(),
      is_corrupt_reinstall, request_id, fetch_priority, extension.version(),
      extension.GetType(), GetUpdateURLData(extension_prefs_, extension.id()));
}

ExtensionDownloaderTask ExtensionUpdater::ToDownloaderTask(
    const ExtensionId& id,
    const PendingExtensionInfo& info,
    int request_id,
    DownloadFetchPriority fetch_priority,
    bool is_corrupt_reinstall) {
  return ExtensionDownloaderTask(id, info.update_url(), info.install_source(),
                                 is_corrupt_reinstall, request_id,
                                 fetch_priority);
}

void ExtensionUpdater::EraseUnupdatableIds(std::set<ExtensionId>& ids) const {
  // In Kiosk mode extensions are downloaded and updated by the ExternalCache.
  bool kiosk_crx_manifest_update_url_ignored = false;
#if BUILDFLAG(IS_CHROMEOS)
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager && user_manager->IsLoggedInAsKioskChromeApp()) {
    ash::CrosSettings::Get()->GetBoolean(
        ash::kKioskCRXManifestUpdateURLIgnored,
        &kiosk_crx_manifest_update_url_ignored);
  }
#endif
  std::erase_if(ids, [&](const ExtensionId& id) {
    // Don't update blocked extensions.
    if (registry_->blocked_extensions().Contains(id)) {
      return true;
    }
    const Extension* extension = registry_->GetInstalledExtension(id);
    if (extension) {
      // Remove extensions converted from user scripts with empty update
      // URLs.
      if (extension->converted_from_user_script() &&
          GetEffectiveUpdateURL(*extension).is_empty()) {
        return true;
      }
      // Remove external-policy installed extensions if in kiosk mode.
      if (kiosk_crx_manifest_update_url_ignored &&
          extension->location() == mojom::ManifestLocation::kExternalPolicy) {
        return true;
      }
      // Remove extensions that aren't in an autoupdatable location.
      if (!Manifest::IsAutoUpdateableLocation(extension->location())) {
        return true;
      }
    } else {
      // Remove pending extensions that aren't from an autoupdatable
      // source.
      const PendingExtensionInfo* info =
          pending_extension_manager_->GetById(id);
      if (info && !Manifest::IsAutoUpdateableLocation(info->install_source())) {
        return true;
      }
    }
    return false;
  });
}

void ExtensionUpdater::CheckNow(CheckParams params) {
  std::unique_ptr<ScopedProfileKeepAlive> keep_alive =
      ScopedProfileKeepAlive::TryAcquire(
          profile_, ProfileKeepAliveOrigin::kExtensionUpdater);
  if (!keep_alive) {
    // Profile will be destroyed soon, don't start an update check.
    return;
  }

  CHECK(enabled_);
  CHECK(alive_);
  CHECK(pending_extension_manager_);

  int request_id = next_request_id_++;
  VLOG(2) << "Starting update check " << request_id;

  EnsureDownloaderCreated();

  // Gather the set of extension IDs to update.
  std::set<ExtensionId> ids_to_update;
  if (!params.ids.empty()) {
    // Check just the provided set of extensions.
    ids_to_update.insert(params.ids.begin(), params.ids.end());
  } else {
    // Check pending and installed extensions.

    // Cancel pending DoCheckSoon() call if there's
    // one, as it would be redundant.
    will_check_soon_ = false;
    NotifyStarted();

    // Pending extensions are not yet installed. They come from group policy
    // and external install sources.
    for (const ExtensionId& id :
         pending_extension_manager_->GetPendingIdsForUpdateCheck()) {
      ids_to_update.insert(id);
    }

    // Include all installed extensions.
    for (const auto& e : registry_->GenerateInstalledExtensionsSet()) {
      ids_to_update.insert(e->id());
    }
  }

  // Filter out extensions that can't or shouldn't be updated.
  EraseUnupdatableIds(ids_to_update);

  // If any pending extensions are high priority, the entire fetch should be
  // done at high priority.
  const DownloadFetchPriority pending_fetch_priority =
      pending_extension_manager_->HasHighPriorityPendingExtension()
          ? DownloadFetchPriority::kForeground
          : params.fetch_priority;

  // The extension updater is a chimera of two update stacks:
  // ExtensionDownloader is used for XML v2 extensions-dialect updates, while
  // update_client is used for JSON v4 updates. Each extension being updated
  // will be allocated to at most one of these stacks. `update_check_params`
  // contains the update_client set, while `downloader_->AddPendingExtension`
  // and `request.in_progress_ids` are used for the ExtensionDownloader set.
  ExtensionUpdateCheckParams update_check_params;
  update_check_params.priority =
      pending_fetch_priority == DownloadFetchPriority::kBackground
          ? ExtensionUpdateCheckParams::UpdateCheckPriority::BACKGROUND
          : ExtensionUpdateCheckParams::UpdateCheckPriority::FOREGROUND;
  update_check_params.install_immediately = params.install_immediately;

  InProgressCheck& request = requests_in_progress_[request_id];
  request.update_found_callback = params.update_found_callback;
  request.callback = std::move(params.callback);
  request.install_immediately = params.install_immediately;
  request.profile_keep_alive = std::move(keep_alive);

  // Allocate each extension to a download stack.
  for (const ExtensionId& id : ids_to_update) {
    const PendingExtensionInfo* pending_info =
        pending_extension_manager_->GetById(id);
    const Extension* extension = registry_->GetInstalledExtension(id);
    const bool is_corrupt_reinstall =
        corrupted_extension_reinstaller_->IsReinstallForCorruptionExpected(id);

    // Note: If an extension is both installed and pending, but can't use
    // update_service, pending_info takes priority.
    if (CanUseUpdateService(extension, pending_info)) {
      update_check_params.update_info[id] = GetExtensionUpdateData(id);
      update_check_params.update_info[id].is_corrupt_reinstall =
          is_corrupt_reinstall;
    } else if (pending_info &&
               downloader_->AddPendingExtension(ToDownloaderTask(
                   id, *pending_info, request_id, pending_fetch_priority,
                   is_corrupt_reinstall))) {
      request.in_progress_ids.insert(id);
      InstallStageTrackerFactory::GetForBrowserContext(profile_)
          ->ReportInstallationStage(id,
                                    InstallStageTracker::Stage::DOWNLOADING);
    } else if (pending_info) {
      InstallStageTrackerFactory::GetForBrowserContext(profile_)->ReportFailure(
          id, InstallStageTracker::FailureReason::DOWNLOADER_ADD_FAILED);
    } else if (extension && downloader_->AddPendingExtension(ToDownloaderTask(
                                *extension, request_id, params.fetch_priority,
                                is_corrupt_reinstall))) {
      request.in_progress_ids.insert(id);
    } else {
      // Adding the extension to ExtensionDownloader failed.
      LOG(WARNING) << "Extension " << id << " can not be updated.";
    }
  }

  bool awaiting_downloader = !request.in_progress_ids.empty();
  bool awaiting_update_service = !update_check_params.update_info.empty();

  if (!awaiting_downloader && !awaiting_update_service) {
    // All extensions were filtered out or failed to be added to
    // ExtensionDownloader (and weren't eligible for UpdateService).
    NotifyIfFinished(request_id);
    return;
  }

  request.awaiting_update_service = awaiting_update_service;

  // StartAllPending() will call OnExtensionDownloadFailed or
  // OnExtensionDownloadFinished for each extension that was checked.
  downloader_->StartAllPending(extension_cache_);

  if (awaiting_update_service) {
    update_service_->StartUpdateCheck(
        update_check_params, params.update_found_callback,
        base::BindOnce(&ExtensionUpdater::OnUpdateServiceFinished,
                       weak_ptr_factory_.GetWeakPtr(), request_id));
  }
}

// Only used for ExtensionDownloader callbacks.
void ExtensionUpdater::OnExtensionDownloadStageChanged(const ExtensionId& id,
                                                       Stage stage) {
  InstallStageTrackerFactory::GetForBrowserContext(profile_)
      ->ReportDownloadingStage(id, stage);
}

// Only used for ExtensionDownloader callbacks.
void ExtensionUpdater::OnExtensionUpdateFound(const ExtensionId& id,
                                              const std::set<int>& request_ids,
                                              const base::Version& version) {
  for (const int request_id : request_ids) {
    InProgressCheck& request = requests_in_progress_[request_id];
    if (request.update_found_callback) {
      request.update_found_callback.Run(id, version);
    }
  }
}

// Only used for ExtensionDownloader callbacks.
void ExtensionUpdater::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    CacheStatus cache_status) {
  InstallStageTrackerFactory::GetForBrowserContext(profile_)
      ->ReportDownloadingCacheStatus(id, cache_status);
}

// Only used for ExtensionDownloader callbacks.
void ExtensionUpdater::OnExtensionDownloadFailed(
    const ExtensionId& id,
    Error error,
    const PingResult& ping,
    const std::set<int>& request_ids,
    const FailureData& data) {
  DCHECK(alive_);
  InstallStageTracker* install_stage_tracker =
      InstallStageTrackerFactory::GetForBrowserContext(profile_);

  switch (error) {
    case Error::CRX_FETCH_FAILED:
      install_stage_tracker->ReportFetchError(
          id, InstallStageTracker::FailureReason::CRX_FETCH_FAILED, data);
      break;
    case Error::CRX_FETCH_URL_EMPTY:
      DCHECK(data.additional_info);
      install_stage_tracker->ReportInfoOnNoUpdatesFailure(
          id, data.additional_info.value());
      install_stage_tracker->ReportFailure(
          id, InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
      break;
    case Error::CRX_FETCH_URL_INVALID:
      install_stage_tracker->ReportFailure(
          id, InstallStageTracker::FailureReason::CRX_FETCH_URL_INVALID);
      break;
    case Error::MANIFEST_FETCH_FAILED:
      install_stage_tracker->ReportFetchError(
          id, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED, data);
      break;
    case Error::MANIFEST_INVALID:
      DCHECK(data.manifest_invalid_error);
      install_stage_tracker->ReportManifestInvalidFailure(id, data);
      break;
    case Error::NO_UPDATE_AVAILABLE:
      install_stage_tracker->ReportFailure(
          id, InstallStageTracker::FailureReason::NO_UPDATE);
      break;
    case Error::DISABLED:
      // Error::DISABLED corresponds to the browser having disabled extension
      // updates, the extension updater does not actually run when this error
      // code is emitted.
      break;
  }

  UpdatePingData(id, ping);
  bool install_immediately = false;
  for (const int request_id : request_ids) {
    InProgressCheck& request = requests_in_progress_[request_id];
    install_immediately |= request.install_immediately;
    request.in_progress_ids.erase(id);
    NotifyIfFinished(request_id);
  }

  // This method is called if no updates were found. However a previous update
  // check might have queued an update for this extension already. If a
  // current update check has |install_immediately| set the previously
  // queued update should be installed now.
  if (install_immediately &&
      delayed_install_manager_->GetPendingExtensionUpdate(id)) {
    delayed_install_manager_->FinishDelayedInstallationIfReady(
        id, install_immediately);
  }
}

// Only used for ExtensionDownloader callbacks.
void ExtensionUpdater::OnExtensionDownloadRetry(const ExtensionId& id,
                                                const FailureData& data) {
  InstallStageTrackerFactory::GetForBrowserContext(profile_)
      ->ReportFetchErrorCodes(id, data);
}

// Only used for ExtensionDownloader callbacks.
void ExtensionUpdater::OnExtensionDownloadFinished(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const PingResult& ping,
    const std::set<int>& request_ids,
    InstallCallback callback) {
  DCHECK(alive_);
  InstallStageTrackerFactory::GetForBrowserContext(profile_)
      ->ReportInstallationStage(file.extension_id,
                                InstallStageTracker::Stage::INSTALLING);
  UpdatePingData(file.extension_id, ping);

  VLOG(2) << download_url << " written to " << file.path.value();

  FetchedCRXFile fetched(file, file_ownership_passed, request_ids,
                         std::move(callback));
  // InstallCRXFile() removes extensions from |in_progress_ids| after starting
  // the crx installer.
  InstallCRXFile(std::move(fetched));
}

bool ExtensionUpdater::GetPingDataForExtension(const ExtensionId& id,
                                               DownloadPingData* ping_data) {
  DCHECK(alive_);
  ping_data->rollcall_days =
      CalculatePingDaysForExtension(extension_prefs_->LastPingDay(id));
  ping_data->is_enabled = registrar_->IsExtensionEnabled(id);
  if (!ping_data->is_enabled) {
    ping_data->disable_reasons = extension_prefs_->GetDisableReasons(id);
  }
  ping_data->active_days =
      CalculateActivePingDays(extension_prefs_->LastActivePingDay(id),
                              extension_prefs_->GetActiveBit(id));
  return true;
}

bool ExtensionUpdater::IsExtensionPending(const ExtensionId& id) {
  DCHECK(alive_);
  return PendingExtensionManager::Get(profile_)->IsIdPending(id);
}

bool ExtensionUpdater::GetExtensionExistingVersion(const ExtensionId& id,
                                                   std::string* version) {
  DCHECK(alive_);
  const Extension* extension =
      registry_->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return false;
  }
  const Extension* update =
      delayed_install_manager_->GetPendingExtensionUpdate(id);
  if (update) {
    *version = update->VersionString();
  } else {
    *version = extension->VersionString();
  }
  return true;
}

ExtensionUpdateData ExtensionUpdater::GetExtensionUpdateData(
    const ExtensionId& id) {
  ExtensionUpdateData result;

  const Extension* update =
      delayed_install_manager_->GetPendingExtensionUpdate(id);

  if (update) {
    result.pending_version = update->VersionString();
  }

  return result;
}

void ExtensionUpdater::UpdatePingData(const ExtensionId& id,
                                      const PingResult& ping_result) {
  DCHECK(alive_);
  if (ping_result.did_ping) {
    extension_prefs_->SetLastPingDay(id, ping_result.day_start);
  }
  if (extension_prefs_->GetActiveBit(id)) {
    extension_prefs_->SetActiveBit(id, false);
    extension_prefs_->SetLastActivePingDay(id, ping_result.day_start);
  }
}

void ExtensionUpdater::PutExtensionInCache(const CRXFileInfo& crx_info) {
  if (extension_cache_) {
    const ExtensionId& extension_id = crx_info.extension_id;
    const base::Version& expected_version = crx_info.expected_version;
    const std::string& expected_hash = crx_info.expected_hash;
    const base::FilePath& crx_path = crx_info.path;
    DCHECK(expected_version.IsValid());
    extension_cache_->PutExtension(
        extension_id, expected_hash, crx_path, expected_version.GetString(),
        base::BindRepeating(&ExtensionUpdater::CleanUpCrxFileIfNeeded,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    CleanUpCrxFileIfNeeded(crx_info.path, true);
  }
}

void ExtensionUpdater::CleanUpCrxFileIfNeeded(const base::FilePath& crx_path,
                                              bool file_ownership_passed) {
  if (file_ownership_passed &&
      !GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE, base::GetDeleteFileCallback(crx_path))) {
    NOTREACHED();
  }
}

bool ExtensionUpdater::CanUseUpdateService(
    const Extension* extension,
    const PendingExtensionInfo* info) const {
  if (g_force_use_update_service_for_tests) {
    return true;
  }

  // UpdateService can only update extensions from the store. If `update_url`
  // is empty, we default to checking from the store.
  if (info) {
    const GURL& update_url = info->update_url();
    return update_url.is_empty() ||
           extension_urls::IsWebstoreUpdateUrl(update_url);
  }
  if (extension) {
    const GURL& update_url = GetEffectiveUpdateURL(*extension);
    return update_url.is_empty() ||
           extension_urls::IsWebstoreUpdateUrl(update_url);
  }
  // TODO(crbug.com/482088398): This could become NOTREACHED, or the function
  // could take a std::variant.
  return false;
}

void ExtensionUpdater::InstallCRXFile(FetchedCRXFile crx_file) {
  std::set<int> request_ids;

  VLOG(2) << "updating " << crx_file.info.extension_id << " with "
          << crx_file.info.path.value();

  // The delegate is now responsible for cleaning up the temp file at
  // `crx_file.info.path`.
  scoped_refptr<CrxInstaller> installer =
      CreateUpdateInstaller(crx_file.info, crx_file.file_ownership_passed);
  if (installer) {
    // If the crx file passes the expectations from the update manifest, this
    // callback inserts an entry in the extension cache and deletes it, if
    // required.
    installer->set_expectations_verified_callback(
        base::BindOnce(&ExtensionUpdater::PutExtensionInCache,
                       weak_ptr_factory_.GetWeakPtr(), crx_file.info));

    auto token = UnguessableToken::Create();
    installer->AddInstallerCallback(
        base::BindOnce(&ExtensionUpdater::OnInstallerDone,
                       weak_ptr_factory_.GetWeakPtr(), token));

    for (const int request_id : crx_file.request_ids) {
      InProgressCheck& request = requests_in_progress_[request_id];
      if (request.install_immediately) {
        installer->set_install_immediately(true);
        break;
      }
    }

    crx_file.installer = installer;
    CRXFileInfo crx_info = crx_file.info;
    running_crx_installs_[token] = std::move(crx_file);
    installer->InstallCrxFile(crx_info);
  } else {
    for (const int request_id : crx_file.request_ids) {
      InProgressCheck& request = requests_in_progress_[request_id];
      request.in_progress_ids.erase(crx_file.info.extension_id);
    }
    request_ids.insert(crx_file.request_ids.begin(),
                       crx_file.request_ids.end());
  }

  for (const int request_id : request_ids) {
    NotifyIfFinished(request_id);
  }
}

scoped_refptr<CrxInstaller> ExtensionUpdater::CreateUpdateInstaller(
    const CRXFileInfo& file,
    bool file_ownership_passed) {
  // Allow tests to override the factory to supply fake CrxInstallers.
  if (crx_installer_factory_for_test_) {
    return crx_installer_factory_for_test_->CreateUpdateInstaller(
        file, file_ownership_passed);
  }
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
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(profile_));
  installer->set_is_update(true);
  installer->set_expected_id(id);
  installer->set_expected_hash(file.expected_hash);
  int creation_flags = Extension::NO_FLAGS;
  if (pending_extension_info) {
    installer->set_install_source(pending_extension_info->install_source());
    installer->set_allow_silent_install(true);
    // If the extension came in disabled due to a permission increase, then
    // don't grant it all the permissions. crbug.com/40416721
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
    // `external_install_manager_` may be null in tests.
    if (external_install_manager_ &&
        pending_extension_info->mark_acknowledged()) {
      external_install_manager_->AcknowledgeExternalExtension(id);
    }
    // If the extension was installed from or has migrated to the webstore, or
    // its auto-update URL is from the webstore, treat it as a webstore
    // install. Note that we ignore some older extensions with blank
    // auto-update URLs because we are mostly concerned with restrictions on
    // NaCl extensions, which are newer.
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

  return installer;
}

void ExtensionUpdater::OnInstallerDone(
    const UnguessableToken& token,
    const std::optional<CrxInstallError>& error) {
  auto iter = running_crx_installs_.find(token);
  CHECK(iter != running_crx_installs_.end());
  FetchedCRXFile& crx_file = iter->second;

  bool extension_removed_from_cache = false;

  if (error && extension_cache_) {
    extension_removed_from_cache = extension_cache_->OnInstallFailed(
        crx_file.installer->expected_id(), crx_file.installer->expected_hash(),
        error.value());
  }

  const bool verification_or_expectation_failed =
      error ? error->IsCrxVerificationFailedError() ||
                  error->IsCrxExpectationsFailedError()
            : false;

  if (verification_or_expectation_failed && extension_removed_from_cache &&
      !crx_file.callback.is_null()) {
    // If extension downloader asked us to notify it about failed
    // installations, it will resume a pending download from the manifest data
    // it has already fetched and call us again with the same request_id's
    // (with either OnExtensionDownloadFailed or OnExtensionDownloadFinished).
    // For that reason we don't notify finished requests yet.
    std::move(crx_file.callback).Run(true);
  } else {
    for (const int request_id : crx_file.request_ids) {
      InProgressCheck& request = requests_in_progress_[request_id];
      request.in_progress_ids.erase(crx_file.info.extension_id);
      NotifyIfFinished(request_id);
    }
    if (!crx_file.callback.is_null()) {
      std::move(crx_file.callback).Run(false);
    }
  }

  running_crx_installs_.erase(iter);

  if (installer_result_callback_for_testing_) {
    std::move(installer_result_callback_for_testing_).Run(error);
  }
}

void ExtensionUpdater::NotifyStarted() {
  if (updating_started_callback_) {
    updating_started_callback_.Run();
  }
}

void ExtensionUpdater::OnUpdateServiceFinished(int request_id) {
  DCHECK(requests_in_progress_.contains(request_id));
  InProgressCheck& request = requests_in_progress_[request_id];
  DCHECK(request.awaiting_update_service);
  request.awaiting_update_service = false;
  NotifyIfFinished(request_id);
}

void ExtensionUpdater::NotifyIfFinished(int request_id) {
  DCHECK(requests_in_progress_.contains(request_id));
  InProgressCheck& request = requests_in_progress_[request_id];
  if (!request.in_progress_ids.empty() || request.awaiting_update_service) {
    return;  // This request is not done yet.
  }
  VLOG(2) << "Finished update check " << request_id;
  if (!request.callback.is_null()) {
    std::move(request.callback).Run();
  }
  requests_in_progress_.erase(request_id);
}

void ExtensionUpdater::OnAppTerminating() {
  // Shutdown has started. Don't start any more extension updates.
  browser_terminating_ = true;
}

std::set<ExtensionId> ExtensionUpdater::GetCorruptedExtensionIds() const {
  std::set<ExtensionId> ids;
  for (const auto& it :
       corrupted_extension_reinstaller_->GetExpectedReinstalls()) {
    ids.insert(it.first);
  }
  return ids;
}

GURL ExtensionUpdater::GetEffectiveUpdateURL(const Extension& extension) const {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  return extension_management->GetEffectiveUpdateURL(extension);
}

ExtensionUpdater::ScopedSkipScheduledCheckForTest::
    ScopedSkipScheduledCheckForTest() {
  DCHECK(!g_skip_scheduled_checks_for_tests);
  g_skip_scheduled_checks_for_tests = true;
}

ExtensionUpdater::ScopedSkipScheduledCheckForTest::
    ~ScopedSkipScheduledCheckForTest() {
  g_skip_scheduled_checks_for_tests = false;
}

}  // namespace extensions
