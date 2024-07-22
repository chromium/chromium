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

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using base::RandDouble;
using base::UnguessableToken;
using Error = extensions::ExtensionDownloaderDelegate::Error;
using PingResult = extensions::ExtensionDownloaderDelegate::PingResult;

namespace {

bool g_should_immediately_update = false;

// For sanity checking on update frequency - enforced in release mode only.
#if defined(NDEBUG)
const int kMinUpdateFrequencySeconds = 30;
#endif
const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

bool g_skip_scheduled_checks_for_tests = false;

bool g_force_use_update_service_for_tests = false;

// When we've computed a days value, we want to make sure we don't send a
// negative value (due to the system clock being set backwards, etc.), since -1
// is a special sentinel value that means "never pinged", and other negative
// values don't make sense.
int SanitizeDays(int days) {
  if (days < 0)
    return 0;
  return days;
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
  if (!hasActiveBit)
    return 0;
  if (last_active_ping_day.is_null())
    return extensions::ManifestFetchData::kNeverPinged;
  return SanitizeDays((base::Time::Now() - last_active_ping_day).InDays());
}

std::string GetUpdateURLData(const extensions::ExtensionPrefs* prefs,
                             const std::string& extension_id) {
  std::string data;
  prefs->ReadPrefAsString(extension_id, extensions::kUpdateURLData, &data);
  return data;
}

}  // namespace

namespace extensions {

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

ExtensionUpdater::ExtensionUpdater(
    ExtensionServiceInterface* service,
    ExtensionPrefs* extension_prefs,
    PrefService* prefs,
    Profile* profile,
    int frequency_seconds,
    ExtensionCache* cache,
    const ExtensionDownloader::Factory& downloader_factory)
    : service_(service),
      downloader_factory_(downloader_factory),
      frequency_(base::Seconds(frequency_seconds)),
      extension_prefs_(extension_prefs),
      prefs_(prefs),
      profile_(profile),
      registry_(ExtensionRegistry::Get(profile)),
      extension_cache_(cache) {
  DCHECK_LE(frequency_seconds, kMaxUpdateFrequencySeconds);
#if defined(NDEBUG)
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds = std::max(frequency_seconds, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds = std::min(frequency_seconds, kMaxUpdateFrequencySeconds);
  frequency_ = base::Seconds(frequency_seconds);
}

ExtensionUpdater::~ExtensionUpdater() {
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
  DCHECK(!alive_);
  // If these are NULL, then that means we've been called after Stop()
  // has been called.
  DCHECK(service_);
  DCHECK(extension_prefs_);
  DCHECK(prefs_);
  DCHECK(profile_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  DCHECK(registry_);
  alive_ = true;
  // Check soon, and set up the first delayed check.
  if (!g_skip_scheduled_checks_for_tests) {
    if (g_should_immediately_update)
      CheckNow({});
    else
      CheckSoon();
    ScheduleNextCheck();
  }
}

void ExtensionUpdater::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  alive_ = false;
  service_ = nullptr;
  extension_prefs_ = nullptr;
  prefs_ = nullptr;
  profile_ = nullptr;
  will_check_soon_ = false;
  downloader_.reset();
  update_service_ = nullptr;
  registry_ = nullptr;
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
  if (!alive_)
    return;
  CheckNow(CheckParams());
  ScheduleNextCheck();
}

void ExtensionUpdater::CheckSoon() {
  DCHECK(alive_);
  if (will_check_soon_)
    return;
  if (content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&ExtensionUpdater::DoCheckSoon,
                                    weak_ptr_factory_.GetWeakPtr()))) {
    will_check_soon_ = true;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

bool ExtensionUpdater::WillCheckSoon() const {
  return will_check_soon_;
}

void ExtensionUpdater::SetExtensionCacheForTesting(
    ExtensionCache* extension_cache) {
  extension_cache_ = extension_cache;
}

void ExtensionUpdater::SetExtensionDownloaderForTesting(
    std::unique_ptr<ExtensionDownloader> downloader) {
  downloader_.swap(downloader);
}

// static
void ExtensionUpdater::UpdateImmediatelyForFirstRun() {
  g_should_immediately_update = true;
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

void ExtensionUpdater::AddToDownloader(
    const ExtensionSet* extensions,
    const std::set<ExtensionId>& pending_ids,
    int request_id,
    DownloadFetchPriority fetch_priority,
    ExtensionUpdateCheckParams* update_check_params) {
  DCHECK(update_service_);

  // In Kiosk mode extensions are downloaded and updated by the ExternalCache.
  // Therefore we skip updates here to avoid conflicts.
  bool kiosk_crx_manifest_update_url_ignored = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager && user_manager->IsLoggedInAsKioskApp()) {
    ash::CrosSettings::Get()->GetBoolean(
        ash::kKioskCRXManifestUpdateURLIgnored,
        &kiosk_crx_manifest_update_url_ignored);
  }
#endif

  InProgressCheck& request = requests_in_progress_[request_id];
  for (ExtensionSet::const_iterator extension_iter = extensions->begin();
       extension_iter != extensions->end(); ++extension_iter) {
    const Extension& extension = **extension_iter;
    const ExtensionId& extension_id = extension.id();
    if (!Manifest::IsAutoUpdateableLocation(extension.location())) {
      VLOG(2) << "Extension " << extension_id << " is not auto updateable";
      continue;
    }
    // An extension might be overwritten by policy, and have its update url
    // changed. Make sure existing extensions aren't fetched again, if a
    // pending fetch for an extension with the same id already exists.
    if (base::Contains(pending_ids, extension_id))
      continue;

    if (extension.location() == mojom::ManifestLocation::kExternalPolicy &&
        kiosk_crx_manifest_update_url_ignored) {
      continue;
    }

    if (CanUseUpdateService(extension_id)) {
      update_check_params->update_info[extension_id] =
          GetExtensionUpdateData(extension_id);
    } else if (AddExtensionToDownloader(extension, request_id,
                                        fetch_priority)) {
      request.in_progress_ids.insert(extension_id);
    }
  }
}

bool ExtensionUpdater::AddExtensionToDownloader(
    const Extension& extension,
    int request_id,
    DownloadFetchPriority fetch_priority) {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  GURL update_url = extension_management->GetEffectiveUpdateURL(extension);
  // Skip extensions with empty update URLs converted from user
  // scripts.
  if (extension.converted_from_user_script() && update_url.is_empty()) {
    return false;
  }

  DCHECK(alive_);

  // If the extension updates itself from the gallery, ignore any update URL
  // data.  At the moment there is no extra data that an extension can
  // communicate to the gallery update servers.
  std::string update_url_data;
  if (!ManifestURL::UpdatesFromGallery(&extension))
    update_url_data = GetUpdateURLData(extension_prefs_, extension.id());

  return downloader_->AddPendingExtension(ExtensionDownloaderTask(
      extension.id(), update_url, extension.location(),
      false /*is_corrupt_reinstall*/, request_id, fetch_priority,
      extension.version(), extension.GetType(), update_url_data));
}

void ExtensionUpdater::CheckNow(CheckParams params) {
  if (params.ids.empty()) {
    // Checking all extensions. Cancel pending DoCheckSoon() call if there's
    // one, as it would be redundant.
    will_check_soon_ = false;
  }

  int request_id = next_request_id_++;

  VLOG(2) << "Starting update check " << request_id;
  if (params.ids.empty())
    NotifyStarted();

  DCHECK(alive_);

  InProgressCheck& request = requests_in_progress_[request_id];
  request.update_found_callback = params.update_found_callback;
  request.callback = std::move(params.callback);
  request.install_immediately = params.install_immediately;
  request.profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kExtensionUpdater);

  EnsureDownloaderCreated();

  // Add fetch records for extensions that should be fetched by an update URL.
  // These extensions are not yet installed. They come from group policy
  // and external install sources.
  const PendingExtensionManager* pending_extension_manager =
      service_->pending_extension_manager();
  const CorruptedExtensionReinstaller* corrupted_extension_reinstaller =
      service_->corrupted_extension_reinstaller();

  ExtensionUpdateCheckParams update_check_params;

  if (params.ids.empty()) {
    // If no extension ids are specified, then:
    //   * install all pending extensions from the pending extension manager,
    //   * reinstall corrupted extension to repair them,
    //   * check for updates for all installed extensions.

    // Use a set so extension IDs will be deduplicated automatically.
    std::set<ExtensionId> pending_ids;
    for (const ExtensionId& id :
         pending_extension_manager->GetPendingIdsForUpdateCheck()) {
      pending_ids.insert(id);
    }
    // Include corrupted extensions that should be repaired.
    for (const auto& it :
         corrupted_extension_reinstaller->GetExpectedReinstalls()) {
      pending_ids.insert(it.first);
    }

    for (const ExtensionId& pending_id : pending_ids) {
      const PendingExtensionInfo* info =
          pending_extension_manager->GetById(pending_id);

      const bool is_corrupt_reinstall =
          corrupted_extension_reinstaller->IsReinstallForCorruptionExpected(
              pending_id);

      // Extensions from the webstore that are corrupted do not have
      // PendingExtensionInfo but are still available in the extension registry.
      // They should be disabled because they are corrupted and require to be
      // repaired.
      if (!info) {
        const Extension* extension = registry_->GetExtensionById(
            pending_id, ExtensionRegistry::EVERYTHING);

        // It is possible that the user deletes the extension between the time
        // it was detected as corrupted and now. In that case, `extension` will
        // be null and we should just skip it.
        if (!extension)
          continue;
        // Policy installed extensions are not necessarily from the webstore,
        // but should have an `info` and never hit this path.
        DCHECK(extension->from_webstore()) << "Extension with id " << pending_id
                                           << " is not from the webstore";
        DCHECK(is_corrupt_reinstall) << "Extension with id " << pending_id
                                     << " is not a corrupt reinstall";
        update_check_params.update_info[pending_id] =
            GetExtensionUpdateData(pending_id);
      } else if (!Manifest::IsAutoUpdateableLocation(info->install_source())) {
        VLOG(2) << "Extension " << pending_id << " is not auto updateable";
        continue;
      }
      // We have to mark high-priority extensions (such as policy-forced
      // extensions or external component extensions) with foreground fetch
      // priority; otherwise their installation may be throttled by bandwidth
      // limits.
      // See https://crbug.com/904600 and https://crbug.com/965686.
      const bool is_high_priority_extension_pending =
          pending_extension_manager->HasHighPriorityPendingExtension();
      if (CanUseUpdateService(pending_id)) {
        update_check_params.update_info[pending_id] =
            GetExtensionUpdateData(pending_id);
        update_check_params.update_info[pending_id].is_corrupt_reinstall =
            is_corrupt_reinstall;
        if (is_corrupt_reinstall) {
          LOG(WARNING) << "Corrupt extension with id " << pending_id
                       << " will be reinstalled with UpdateService.";
        }
      } else if (info &&
                 downloader_->AddPendingExtension(ExtensionDownloaderTask(
                     pending_id, info->update_url(), info->install_source(),
                     is_corrupt_reinstall, request_id,
                     is_high_priority_extension_pending
                         ? DownloadFetchPriority::kForeground
                         : params.fetch_priority))) {
        request.in_progress_ids.insert(pending_id);
        InstallStageTracker::Get(profile_)->ReportInstallationStage(
            pending_id, InstallStageTracker::Stage::DOWNLOADING);
        if (is_corrupt_reinstall) {
          LOG(WARNING) << "Corrupt extension with id " << pending_id
                       << " will be reinstalled with ExtensionDownloader.";
        }
      } else {
        InstallStageTracker::Get(profile_)->ReportFailure(
            pending_id,
            InstallStageTracker::FailureReason::DOWNLOADER_ADD_FAILED);
      }
    }

    AddToDownloader(&registry_->enabled_extensions(), pending_ids, request_id,
                    params.fetch_priority, &update_check_params);
    AddToDownloader(&registry_->disabled_extensions(), pending_ids, request_id,
                    params.fetch_priority, &update_check_params);
    ExtensionSet remotely_disabled_extensions;
    for (auto extension : registry_->blocklisted_extensions()) {
      if (blocklist_prefs::HasOmahaBlocklistState(
              extension->id(), BitMapBlocklistState::BLOCKLISTED_MALWARE,
              extension_prefs_)) {
        remotely_disabled_extensions.Insert(extension);
      }
    }
    AddToDownloader(&remotely_disabled_extensions, pending_ids, request_id,
                    params.fetch_priority, &update_check_params);
  } else {
    for (const ExtensionId& id : params.ids) {
      const Extension* extension =
          registry_->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
      if (extension) {
        if (CanUseUpdateService(id)) {
          update_check_params.update_info[id] = GetExtensionUpdateData(id);
        } else if (AddExtensionToDownloader(*extension, request_id,
                                            params.fetch_priority)) {
          request.in_progress_ids.insert(extension->id());
        }
      }
    }
  }

  // StartAllPending() might call OnExtensionDownloadFailed/Finished before
  // it returns, which would cause NotifyIfFinished to incorrectly try to
  // send out a notification. So check before we call StartAllPending if any
  // extensions are going to be updated, and use that to figure out if
  // NotifyIfFinished should be called.
  bool empty_downloader = request.in_progress_ids.empty();
  bool awaiting_update_service = !update_check_params.update_info.empty();

  request.awaiting_update_service = awaiting_update_service;

  // StartAllPending() will call OnExtensionDownloadFailed or
  // OnExtensionDownloadFinished for each extension that was checked.
  downloader_->StartAllPending(extension_cache_);

  if (awaiting_update_service) {
    update_check_params.priority =
        params.fetch_priority == DownloadFetchPriority::kBackground
            ? ExtensionUpdateCheckParams::UpdateCheckPriority::BACKGROUND
            : ExtensionUpdateCheckParams::UpdateCheckPriority::FOREGROUND;
    update_check_params.install_immediately = params.install_immediately;
    update_service_->StartUpdateCheck(
        update_check_params, params.update_found_callback,
        base::BindOnce(&ExtensionUpdater::OnUpdateServiceFinished,
                       base::Unretained(this), request_id));
  } else if (empty_downloader) {
    NotifyIfFinished(request_id);
  }
}

void ExtensionUpdater::OnExtensionDownloadStageChanged(const ExtensionId& id,
                                                       Stage stage) {
  InstallStageTracker::Get(profile_)->ReportDownloadingStage(id, stage);
}

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

void ExtensionUpdater::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    CacheStatus cache_status) {
  InstallStageTracker::Get(profile_)->ReportDownloadingCacheStatus(
      id, cache_status);
}

void ExtensionUpdater::OnExtensionDownloadFailed(
    const ExtensionId& id,
    Error error,
    const PingResult& ping,
    const std::set<int>& request_ids,
    const FailureData& data) {
  DCHECK(alive_);
  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);

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
  if (install_immediately && service_->GetPendingExtensionUpdate(id))
    service_->FinishDelayedInstallationIfReady(id, install_immediately);
}

void ExtensionUpdater::OnExtensionDownloadRetry(const ExtensionId& id,
                                                const FailureData& data) {
  InstallStageTracker::Get(profile_)->ReportFetchErrorCodes(id, data);
}

void ExtensionUpdater::OnExtensionDownloadFinished(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const PingResult& ping,
    const std::set<int>& request_ids,
    InstallCallback callback) {
  DCHECK(alive_);
  InstallStageTracker::Get(profile_)->ReportInstallationStage(
      file.extension_id, InstallStageTracker::Stage::INSTALLING);
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
  ping_data->is_enabled = service_->IsExtensionEnabled(id);
  if (!ping_data->is_enabled)
    ping_data->disable_reasons = extension_prefs_->GetDisableReasons(id);
  ping_data->active_days =
      CalculateActivePingDays(extension_prefs_->LastActivePingDay(id),
                              extension_prefs_->GetActiveBit(id));
  return true;
}

bool ExtensionUpdater::IsExtensionPending(const ExtensionId& id) {
  DCHECK(alive_);
  return service_->pending_extension_manager()->IsIdPending(id);
}

bool ExtensionUpdater::GetExtensionExistingVersion(const ExtensionId& id,
                                                   std::string* version) {
  DCHECK(alive_);
  const Extension* extension =
      registry_->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  if (!extension)
    return false;
  const Extension* update = service_->GetPendingExtensionUpdate(id);
  if (update)
    *version = update->VersionString();
  else
    *version = extension->VersionString();
  return true;
}

ExtensionUpdateData ExtensionUpdater::GetExtensionUpdateData(
    const ExtensionId& id) {
  ExtensionUpdateData result;

  const Extension* update = service_->GetPendingExtensionUpdate(id);

  if (update) {
    result.pending_version = update->VersionString();
    result.pending_fingerprint = update->DifferentialFingerprint();
  }

  return result;
}

void ExtensionUpdater::UpdatePingData(const ExtensionId& id,
                                      const PingResult& ping_result) {
  DCHECK(alive_);
  if (ping_result.did_ping)
    extension_prefs_->SetLastPingDay(id, ping_result.day_start);
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
    NOTREACHED_IN_MIGRATION();
  }
}

bool ExtensionUpdater::CanUseUpdateService(
    const ExtensionId& extension_id) const {
  if (g_force_use_update_service_for_tests)
    return true;

  // Won't update extensions with empty IDs.
  if (extension_id.empty())
    return false;

  // Update service can only update extensions that have been installed on the
  // system.
  const Extension* extension = registry_->GetInstalledExtension(extension_id);
  if (extension == nullptr)
    return false;

  // Furthermore, we can only update extensions that were installed from the
  // default webstore or extensions with empty update URLs not converted from
  // user scripts.
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  const GURL& update_url =
      extension_management->GetEffectiveUpdateURL(*extension);
  if (update_url.is_empty())
    return !extension->converted_from_user_script();
  return extension_urls::IsWebstoreUpdateUrl(update_url);
}

void ExtensionUpdater::InstallCRXFile(FetchedCRXFile crx_file) {
  std::set<int> request_ids;

  VLOG(2) << "updating " << crx_file.info.extension_id << " with "
          << crx_file.info.path.value();

  // The ExtensionService is now responsible for cleaning up the temp file
  // at |crx_file.info.path|.
  scoped_refptr<CrxInstaller> installer = service_->CreateUpdateInstaller(
      crx_file.info, crx_file.file_ownership_passed);
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

  for (const int request_id : request_ids)
    NotifyIfFinished(request_id);
}

void ExtensionUpdater::OnInstallerDone(
    const UnguessableToken& token,
    const std::optional<CrxInstallError>& error) {
  auto iter = running_crx_installs_.find(token);
  CHECK(iter != running_crx_installs_.end(), base::NotFatalUntil::M130);
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
    // If extension downloader asked us to notify it about failed installations,
    // it will resume a pending download from the manifest data it has already
    // fetched and call us again with the same request_id's (with either
    // OnExtensionDownloadFailed or OnExtensionDownloadFinished). For that
    // reason we don't notify finished requests yet.
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
  if (updating_started_callback_)
    updating_started_callback_.Run();
}

void ExtensionUpdater::OnUpdateServiceFinished(int request_id) {
  DCHECK(base::Contains(requests_in_progress_, request_id));
  InProgressCheck& request = requests_in_progress_[request_id];
  DCHECK(request.awaiting_update_service);
  request.awaiting_update_service = false;
  NotifyIfFinished(request_id);
}

void ExtensionUpdater::NotifyIfFinished(int request_id) {
  DCHECK(base::Contains(requests_in_progress_, request_id));
  InProgressCheck& request = requests_in_progress_[request_id];
  if (!request.in_progress_ids.empty() || request.awaiting_update_service)
    return;  // This request is not done yet.
  VLOG(2) << "Finished update check " << request_id;
  if (!request.callback.is_null())
    std::move(request.callback).Run();
  requests_in_progress_.erase(request_id);
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
