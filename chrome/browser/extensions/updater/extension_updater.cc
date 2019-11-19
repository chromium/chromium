// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <stdint.h>

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/module/module.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/sha2.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

using base::RandDouble;
using base::RandInt;
using content::BrowserThread;

typedef extensions::ExtensionDownloaderDelegate::Error Error;
typedef extensions::ExtensionDownloaderDelegate::PingResult PingResult;

namespace {

bool g_should_immediately_update = false;

// For sanity checking on update frequency - enforced in release mode only.
#if defined(NDEBUG)
const int kMinUpdateFrequencySeconds = 30;
#endif
const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

bool g_skip_scheduled_checks_for_tests = false;

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

}  // namespace

namespace extensions {

ExtensionUpdater::CheckParams::CheckParams()
    : install_immediately(false),
      fetch_priority(ManifestFetchData::FetchPriority::BACKGROUND) {}

ExtensionUpdater::CheckParams::~CheckParams() = default;

ExtensionUpdater::CheckParams::CheckParams(
    ExtensionUpdater::CheckParams&& other) = default;
ExtensionUpdater::CheckParams& ExtensionUpdater::CheckParams::operator=(
    ExtensionUpdater::CheckParams&& other) = default;

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const std::set<int>& request_ids,
    const InstallCallback& callback)
    : info(file),
      file_ownership_passed(file_ownership_passed),
      request_ids(request_ids),
      callback(callback) {
}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile()
    : file_ownership_passed(true) {
}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(const FetchedCRXFile& other) =
    default;

ExtensionUpdater::FetchedCRXFile::~FetchedCRXFile() {}

ExtensionUpdater::InProgressCheck::InProgressCheck()
    : install_immediately(false), awaiting_update_service(false) {}

ExtensionUpdater::InProgressCheck::~InProgressCheck() = default;

ExtensionUpdater::ExtensionUpdater(
    ExtensionServiceInterface* service,
    ExtensionPrefs* extension_prefs,
    PrefService* prefs,
    Profile* profile,
    int frequency_seconds,
    ExtensionCache* cache,
    const ExtensionDownloader::Factory& downloader_factory)
    : alive_(false),
      service_(service),
      downloader_factory_(downloader_factory),
      update_service_(nullptr),
      frequency_(base::TimeDelta::FromSeconds(frequency_seconds)),
      will_check_soon_(false),
      extension_prefs_(extension_prefs),
      prefs_(prefs),
      profile_(profile),
      registry_(ExtensionRegistry::Get(profile)),
      next_request_id_(0),
      crx_install_is_running_(false),
      extension_cache_(cache) {
  DCHECK_LE(frequency_seconds, kMaxUpdateFrequencySeconds);
#if defined(NDEBUG)
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds = std::max(frequency_seconds, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds = std::min(frequency_seconds, kMaxUpdateFrequencySeconds);
  frequency_ = base::TimeDelta::FromSeconds(frequency_seconds);
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
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
      static_cast<int64_t>(frequency_.InMilliseconds() * jitter_factor));
  base::PostDelayedTask(FROM_HERE,
                        {base::TaskPriority::BEST_EFFORT, BrowserThread::UI},
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
  if (base::PostTask(FROM_HERE,
                     {base::TaskPriority::BEST_EFFORT, BrowserThread::UI},
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

void ExtensionUpdater::DoCheckSoon() {
  DCHECK(will_check_soon_);
  CheckNow(CheckParams());
  will_check_soon_ = false;
}

void ExtensionUpdater::AddToDownloader(
    const ExtensionSet* extensions,
    const std::list<std::string>& pending_ids,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority,
    ExtensionUpdateCheckParams* update_check_params) {
  DCHECK(update_service_);
  InProgressCheck& request = requests_in_progress_[request_id];
  for (ExtensionSet::const_iterator extension_iter = extensions->begin();
       extension_iter != extensions->end(); ++extension_iter) {
    const Extension& extension = **extension_iter;
    const std::string& extension_id = extension.id();
    if (!Manifest::IsAutoUpdateableLocation(extension.location())) {
      VLOG(2) << "Extension " << extension_id << " is not auto updateable";
      continue;
    }
    // An extension might be overwritten by policy, and have its update url
    // changed. Make sure existing extensions aren't fetched again, if a
    // pending fetch for an extension with the same id already exists.
    if (!base::Contains(pending_ids, extension_id)) {
      if (update_service_->CanUpdate(extension_id)) {
        update_check_params->update_info[extension_id] = ExtensionUpdateData();
      } else if (downloader_->AddExtension(extension, request_id,
                                           fetch_priority)) {
        request.in_progress_ids_.insert(extension_id);
      }
    }
  }
}

void ExtensionUpdater::CheckNow(CheckParams params) {
  int request_id = next_request_id_++;

  VLOG(2) << "Starting update check " << request_id;
  if (params.ids.empty())
    NotifyStarted();

  DCHECK(alive_);

  InProgressCheck& request = requests_in_progress_[request_id];
  request.callback = std::move(params.callback);
  request.install_immediately = params.install_immediately;

  EnsureDownloaderCreated();

  // Add fetch records for extensions that should be fetched by an update URL.
  // These extensions are not yet installed. They come from group policy
  // and external install sources.
  const PendingExtensionManager* pending_extension_manager =
      service_->pending_extension_manager();

  std::list<std::string> pending_ids;
  ExtensionUpdateCheckParams update_check_params;

  if (params.ids.empty()) {
    // If no extension ids are specified, check for updates for all extensions.
    pending_extension_manager->GetPendingIdsForUpdateCheck(&pending_ids);

    for (const std::string& pending_id : pending_ids) {
      const PendingExtensionInfo* info =
          pending_extension_manager->GetById(pending_id);
      if (!Manifest::IsAutoUpdateableLocation(info->install_source())) {
        VLOG(2) << "Extension " << pending_id << " is not auto updateable";
        continue;
      }

      const bool is_corrupt_reinstall =
          pending_extension_manager->IsPolicyReinstallForCorruptionExpected(
              pending_id);
      if (update_service_->CanUpdate(pending_id)) {
        update_check_params.update_info[pending_id].is_corrupt_reinstall =
            is_corrupt_reinstall;
      } else if (downloader_->AddPendingExtension(
                     pending_id, info->update_url(), info->install_source(),
                     is_corrupt_reinstall, request_id, params.fetch_priority)) {
        request.in_progress_ids_.insert(pending_id);
        InstallationReporter::Get(profile_)->ReportInstallationStage(
            pending_id, InstallationReporter::Stage::DOWNLOADING);
      } else {
        InstallationReporter::Get(profile_)->ReportFailure(
            pending_id,
            InstallationReporter::FailureReason::DOWNLOADER_ADD_FAILED);
      }
    }

    AddToDownloader(&registry_->enabled_extensions(), pending_ids, request_id,
                    params.fetch_priority, &update_check_params);
    AddToDownloader(&registry_->disabled_extensions(), pending_ids, request_id,
                    params.fetch_priority, &update_check_params);
  } else {
    for (const std::string& id : params.ids) {
      const Extension* extension = registry_->GetExtensionById(
          id, extensions::ExtensionRegistry::EVERYTHING);
      if (extension) {
        if (update_service_->CanUpdate(id)) {
          update_check_params.update_info[id] = ExtensionUpdateData();
        } else if (downloader_->AddExtension(*extension, request_id,
                                             params.fetch_priority)) {
          request.in_progress_ids_.insert(extension->id());
        }
      }
    }
  }

  // StartAllPending() might call OnExtensionDownloadFailed/Finished before
  // it returns, which would cause NotifyIfFinished to incorrectly try to
  // send out a notification. So check before we call StartAllPending if any
  // extensions are going to be updated, and use that to figure out if
  // NotifyIfFinished should be called.
  bool empty_downloader = request.in_progress_ids_.empty();
  bool awaiting_update_service = !update_check_params.update_info.empty();

  request.awaiting_update_service = awaiting_update_service;

  // StartAllPending() will call OnExtensionDownloadFailed or
  // OnExtensionDownloadFinished for each extension that was checked.
  downloader_->StartAllPending(extension_cache_);

  if (awaiting_update_service) {
    update_check_params.priority =
        params.fetch_priority == ManifestFetchData::FetchPriority::BACKGROUND
            ? ExtensionUpdateCheckParams::UpdateCheckPriority::BACKGROUND
            : ExtensionUpdateCheckParams::UpdateCheckPriority::FOREGROUND;
    update_check_params.install_immediately = params.install_immediately;
    update_service_->StartUpdateCheck(
        update_check_params,
        base::BindOnce(&ExtensionUpdater::OnUpdateServiceFinished,
                       base::Unretained(this), request_id));
  } else if (empty_downloader) {
    NotifyIfFinished(request_id);
  }
}

void ExtensionUpdater::OnExtensionDownloadStageChanged(const ExtensionId& id,
                                                       Stage stage) {
  InstallationReporter::Get(profile_)->ReportDownloadingStage(id, stage);
}

void ExtensionUpdater::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    CacheStatus cache_status) {
  InstallationReporter::Get(profile_)->ReportDownloadingCacheStatus(
      id, cache_status);
}

void ExtensionUpdater::OnExtensionDownloadFailed(
    const ExtensionId& id,
    Error error,
    const PingResult& ping,
    const std::set<int>& request_ids) {
  DCHECK(alive_);
  InstallationReporter* installation_reporter =
      InstallationReporter::Get(profile_);

  switch (error) {
    case Error::CRX_FETCH_FAILED:
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.ExtensionUpdaterUpdateResults",
          ExtensionUpdaterUpdateResult::UPDATE_DOWNLOAD_ERROR,
          ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);
      installation_reporter->ReportFailure(
          id, InstallationReporter::FailureReason::CRX_FETCH_FAILED);
      break;
    case Error::MANIFEST_FETCH_FAILED:
      installation_reporter->ReportFailure(
          id, InstallationReporter::FailureReason::MANIFEST_FETCH_FAILED);
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.ExtensionUpdaterUpdateResults",
          ExtensionUpdaterUpdateResult::UPDATE_CHECK_ERROR,
          ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);
      break;
    case Error::MANIFEST_INVALID:
      installation_reporter->ReportFailure(
          id, InstallationReporter::FailureReason::MANIFEST_INVALID);
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.ExtensionUpdaterUpdateResults",
          ExtensionUpdaterUpdateResult::UPDATE_CHECK_ERROR,
          ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);
      break;
    case Error::NO_UPDATE_AVAILABLE:
      installation_reporter->ReportFailure(
          id, InstallationReporter::FailureReason::NO_UPDATE);
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.ExtensionUpdaterUpdateResults",
          ExtensionUpdaterUpdateResult::NO_UPDATE,
          ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);
      break;
    case Error::DISABLED:
      // Error::DISABLED corresponds to the browser having disabled extension
      // updates, the extension updater does not actually run when this error
      // code is emitted. For this reason, Error::DISABLED is not included in
      // Extensions.ExtensionUpdaterUpdateResults UMA; we are only interested
      // in the update results when the extension updater runs.
      break;
  }

  UpdatePingData(id, ping);
  bool install_immediately = false;
  for (const int request_id : request_ids) {
    InProgressCheck& request = requests_in_progress_[request_id];
    install_immediately |= request.install_immediately;
    request.in_progress_ids_.erase(id);
    NotifyIfFinished(request_id);
  }

  // This method is called if no updates were found. However a previous update
  // check might have queued an update for this extension already. If a
  // current update check has |install_immediately| set the previously
  // queued update should be installed now.
  if (install_immediately && service_->GetPendingExtensionUpdate(id))
    service_->FinishDelayedInstallationIfReady(id, install_immediately);
}

void ExtensionUpdater::OnExtensionDownloadFinished(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const std::string& version,
    const PingResult& ping,
    const std::set<int>& request_ids,
    const InstallCallback& callback) {
  DCHECK(alive_);
  InstallationReporter::Get(profile_)->ReportInstallationStage(
      file.extension_id, InstallationReporter::Stage::INSTALLING);
  UpdatePingData(file.extension_id, ping);

  VLOG(2) << download_url << " written to " << file.path.value();

  FetchedCRXFile fetched(file, file_ownership_passed, request_ids, callback);
  fetched_crx_files_.push(fetched);

  // MaybeInstallCRXFile() removes extensions from |in_progress_ids_| after
  // starting the crx installer.
  MaybeInstallCRXFile();
}

bool ExtensionUpdater::GetPingDataForExtension(
    const ExtensionId& id,
    ManifestFetchData::PingData* ping_data) {
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

std::string ExtensionUpdater::GetUpdateUrlData(const ExtensionId& id) {
  DCHECK(alive_);
  return extension::GetUpdateURLData(extension_prefs_, id);
}

bool ExtensionUpdater::IsExtensionPending(const ExtensionId& id) {
  DCHECK(alive_);
  return service_->pending_extension_manager()->IsIdPending(id);
}

bool ExtensionUpdater::GetExtensionExistingVersion(const ExtensionId& id,
                                                   std::string* version) {
  DCHECK(alive_);
  const Extension* extension = registry_->GetExtensionById(
      id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return false;
  const Extension* update = service_->GetPendingExtensionUpdate(id);
  if (update)
    *version = update->VersionString();
  else
    *version = extension->VersionString();
  return true;
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

void ExtensionUpdater::MaybeInstallCRXFile() {
  if (crx_install_is_running_ || fetched_crx_files_.empty())
    return;

  std::set<int> request_ids;

  while (!fetched_crx_files_.empty() && !crx_install_is_running_) {
    const FetchedCRXFile& crx_file = fetched_crx_files_.front();

    VLOG(2) << "updating " << crx_file.info.extension_id
            << " with " << crx_file.info.path.value();

    // The ExtensionService is now responsible for cleaning up the temp file
    // at |crx_file.info.path|.
    CrxInstaller* installer = NULL;
    if (service_->UpdateExtension(crx_file.info,
                                  crx_file.file_ownership_passed,
                                  &installer)) {
      crx_install_is_running_ = true;
      current_crx_file_ = crx_file;

      for (const int request_id : crx_file.request_ids) {
        InProgressCheck& request = requests_in_progress_[request_id];
        if (request.install_immediately) {
          installer->set_install_immediately(true);
          break;
        }
      }

      // Source parameter ensures that we only see the completion event for the
      // the installer we started.
      registrar_.Add(this, NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(installer));
    } else {
      for (const int request_id : crx_file.request_ids) {
        InProgressCheck& request = requests_in_progress_[request_id];
        request.in_progress_ids_.erase(crx_file.info.extension_id);
      }
      request_ids.insert(crx_file.request_ids.begin(),
                         crx_file.request_ids.end());
    }
    fetched_crx_files_.pop();
  }

  for (const int request_id : request_ids)
    NotifyIfFinished(request_id);
}

void ExtensionUpdater::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(NOTIFICATION_CRX_INSTALLER_DONE, type);

  registrar_.Remove(this, NOTIFICATION_CRX_INSTALLER_DONE, source);
  crx_install_is_running_ = false;

  // If installing this file didn't succeed, we may need to re-download it.
  const Extension* extension = content::Details<const Extension>(details).ptr();

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.ExtensionUpdaterUpdateResults",
      extension ? ExtensionUpdaterUpdateResult::UPDATE_SUCCESS
                : ExtensionUpdaterUpdateResult::UPDATE_INSTALL_ERROR,
      ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);

  CrxInstaller* installer = content::Source<CrxInstaller>(source).ptr();
  const FetchedCRXFile& crx_file = current_crx_file_;
  if (!extension && installer->hash_check_failed() &&
      !crx_file.callback.is_null()) {
    // If extension downloader asked us to notify it about failed installations,
    // it will resume a pending download from the manifest data it has already
    // fetched and call us again with the same request_id's (with either
    // OnExtensionDownloadFailed or OnExtensionDownloadFinished). For that
    // reason we don't notify finished requests yet.
    crx_file.callback.Run(true);
  } else {
    for (const int request_id : crx_file.request_ids) {
      InProgressCheck& request = requests_in_progress_[request_id];
      request.in_progress_ids_.erase(crx_file.info.extension_id);
      NotifyIfFinished(request_id);
    }
    if (!crx_file.callback.is_null()) {
      crx_file.callback.Run(false);
    }
  }

  // If any files are available to update, start one.
  MaybeInstallCRXFile();
}

void ExtensionUpdater::NotifyStarted() {
  content::NotificationService::current()->Notify(
      NOTIFICATION_EXTENSION_UPDATING_STARTED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
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
  if (!request.in_progress_ids_.empty() || request.awaiting_update_service)
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
