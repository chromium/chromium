// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"

#include "base/check_op.h"
#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"          // nogncheck
#include "components/user_manager/user_manager.h"  // nogncheck
#include "components/user_manager/user_type.h"     // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {
// Returns true if the `current_stage` should be overridden by the
// `new_stage`.
bool ShouldOverrideCurrentStage(
    std::optional<InstallStageTracker::Stage> current_stage,
    InstallStageTracker::Stage new_stage) {
  if (!current_stage)
    return true;
  // If CRX was from the cache and was damaged as a file, we would try to
  // download the CRX after reporting the INSTALLING stage.
  if (current_stage == InstallStageTracker::Stage::INSTALLING &&
      new_stage == InstallStageTracker::Stage::DOWNLOADING)
    return true;
  return new_stage > current_stage;
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
InstallStageTracker::UserInfo::UserInfo() = default;
InstallStageTracker::UserInfo::UserInfo(const UserInfo&) = default;
InstallStageTracker::UserInfo::UserInfo(user_manager::UserType user_type,
                                        bool is_new_user,
                                        bool is_user_present)
    : user_type(user_type),
      is_new_user(is_new_user),
      is_user_present(is_user_present) {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// InstallStageTracker::InstallationData implementation.

InstallStageTracker::InstallationData::InstallationData() = default;
InstallStageTracker::InstallationData::~InstallationData() = default;

InstallStageTracker::InstallationData::InstallationData(
    const InstallationData&) = default;

std::string InstallStageTracker::GetFormattedInstallationData(
    const InstallationData& data) {
  std::ostringstream str;
  str << "failure_reason: "
      << static_cast<int>(data.failure_reason.value_or(FailureReason::UNKNOWN));
  if (data.install_error_detail) {
    str << "; install_error_detail: "
        << static_cast<int>(data.install_error_detail.value());
    if (data.install_error_detail.value() ==
        CrxInstallErrorDetail::DISALLOWED_BY_POLICY) {
      str << "; extension_type: "
          << static_cast<int>(data.extension_type.value());
    }
  }
  if (data.install_stage) {
    str << "; install_stage: " << static_cast<int>(data.install_stage.value());
  }
  if (data.install_stage && data.install_stage.value() == Stage::DOWNLOADING &&
      data.downloading_stage) {
    str << "; downloading_stage: "
        << static_cast<int>(data.downloading_stage.value());
  }
  // No extra check for stage: we may be interested in cache status even in case
  // of successfull extension install.
  if (data.downloading_cache_status) {
    str << "; downloading_cache_status: "
        << static_cast<int>(data.downloading_cache_status.value());
  }
  if (data.failure_reason == FailureReason::MANIFEST_FETCH_FAILED ||
      data.failure_reason == FailureReason::CRX_FETCH_FAILED) {
    str << "; network_error_code: "
        << static_cast<int>(data.network_error_code.value());
    if (data.network_error_code == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE) {
      str << "; response_code: "
          << static_cast<int>(data.response_code.value());
    }
    str << "; fetch_tries: " << static_cast<int>(data.fetch_tries.value());
  }
  if (data.failure_reason ==
      FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE) {
    str << "; unpacker_failure_reason: "
        << static_cast<int>(data.unpacker_failure_reason.value());
  }
  if (data.manifest_invalid_error) {
    str << "; manifest_invalid_error: "
        << static_cast<int>(data.manifest_invalid_error.value());
  }
  if (data.no_updates_info) {
    str << "; no_update_info: "
        << static_cast<int>(data.no_updates_info.value());
  }
  if (data.app_status_error) {
    str << "; app_status_error: "
        << static_cast<int>(data.app_status_error.value());
  }

  return str.str();
}

// InstallStageTracker::Observer implementation.

InstallStageTracker::Observer::~Observer() = default;

// InstallStageTracker implementation.

InstallStageTracker::InstallStageTracker(const content::BrowserContext* context)
    : browser_context_(context) {}

InstallStageTracker::~InstallStageTracker() = default;

// static
InstallStageTracker* InstallStageTracker::Get(
    content::BrowserContext* context) {
  return InstallStageTrackerFactory::GetForBrowserContext(context);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
InstallStageTracker::UserInfo InstallStageTracker::GetUserInfo(
    Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return UserInfo();

  bool is_new_user = user_manager::UserManager::Get()->IsCurrentUserNew() ||
                     profile->IsNewProfile();
  UserInfo current_user(user->GetType(), is_new_user, /*is_user_present=*/true);
  return current_user;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void InstallStageTracker::ReportInfoOnNoUpdatesFailure(
    const ExtensionId& id,
    const std::string& info) {
  InstallationData& data = installation_data_map_[id];

  // Map the no updates info to NoUpdatesInfo enum.
  if (info.empty())
    data.no_updates_info = NoUpdatesInfo::kEmpty;
  else if (info == "rate limit")
    data.no_updates_info = NoUpdatesInfo::kRateLimit;
  else if (info == "disabled by client")
    data.no_updates_info = NoUpdatesInfo::kDisabledByClient;
  else if (info == "bandwidth limit")
    data.no_updates_info = NoUpdatesInfo::kBandwidthLimit;
  else
    data.no_updates_info = NoUpdatesInfo::kUnknown;
}

void InstallStageTracker::ReportManifestInvalidFailure(
    const ExtensionId& id,
    const ExtensionDownloaderDelegate::FailureData& failure_data) {
  DCHECK(failure_data.manifest_invalid_error);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = FailureReason::MANIFEST_INVALID;
  data.manifest_invalid_error = failure_data.manifest_invalid_error.value();
  if (failure_data.app_status_error) {
    data.app_status_error =
        GetManifestInvalidAppStatusError(failure_data.app_status_error.value());
  }
  NotifyObserversOfFailure(id, data.failure_reason.value(), data);
}

void InstallStageTracker::ReportInstallCreationStage(
    const ExtensionId& id,
    InstallCreationStage stage) {
  InstallationData& data = installation_data_map_[id];
  data.install_creation_stage = stage;
  for (auto& observer : observers_) {
    observer.OnExtensionInstallCreationStageChanged(id, stage);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportInstallationStage(const ExtensionId& id,
                                                  Stage stage) {
  InstallationData& data = installation_data_map_[id];
  if (!ShouldOverrideCurrentStage(data.install_stage, stage))
    return;
  data.install_stage = stage;
  for (auto& observer : observers_) {
    observer.OnExtensionInstallationStageChanged(id, stage);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportDownloadingStage(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::Stage stage) {
  InstallationData& data = installation_data_map_[id];
  data.downloading_stage = stage;
  const base::TimeTicks current_time = base::TimeTicks::Now();
  if (stage == ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST)
    data.download_manifest_started_time = current_time;
  else if (stage == ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED)
    data.download_manifest_finish_time = current_time;
  else if (stage == ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX)
    data.download_CRX_started_time = current_time;
  else if (stage == ExtensionDownloaderDelegate::Stage::FINISHED)
    data.download_CRX_finish_time = current_time;

  for (auto& observer : observers_) {
    observer.OnExtensionDownloadingStageChanged(id, stage);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportCRXInstallationStage(const ExtensionId& id,
                                                     InstallationStage stage) {
  DCHECK(!id.empty());
  InstallationData& data = installation_data_map_[id];
  data.installation_stage = stage;
  const base::TimeTicks current_time = base::TimeTicks::Now();
  if (stage == InstallationStage::kVerification)
    data.verification_started_time = current_time;
  else if (stage == InstallationStage::kCopying)
    data.copying_started_time = current_time;
  else if (stage == InstallationStage::kUnpacking)
    data.unpacking_started_time = current_time;
  else if (stage == InstallationStage::kCheckingExpectations)
    data.checking_expectations_started_time = current_time;
  else if (stage == InstallationStage::kFinalizing)
    data.finalizing_started_time = current_time;
  else if (stage == InstallationStage::kComplete)
    data.installation_complete_time = current_time;

  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportDownloadingCacheStatus(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::CacheStatus cache_status) {
  DCHECK_NE(cache_status,
            ExtensionDownloaderDelegate::CacheStatus::CACHE_UNKNOWN);
  InstallationData& data = installation_data_map_[id];
  data.downloading_cache_status = cache_status;
  for (auto& observer : observers_) {
    observer.OnExtensionDownloadCacheStatusRetrieved(id, cache_status);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

InstallStageTracker::AppStatusError
InstallStageTracker::GetManifestInvalidAppStatusError(
    const std::string& status) {
  if (status == "error-unknownApplication")
    return AppStatusError::kErrorUnknownApplication;
  else if (status == "error-invalidAppId")
    return AppStatusError::kErrorInvalidAppId;
  else if (status == "error-restricted" || status == "restricted")
    return AppStatusError::kErrorRestricted;
  return AppStatusError::kUnknown;
}

void InstallStageTracker::ReportFetchErrorCodes(
    const ExtensionId& id,
    const ExtensionDownloaderDelegate::FailureData& failure_data) {
  InstallationData& data = installation_data_map_[id];
  data.network_error_code = failure_data.network_error_code;
  data.response_code = failure_data.response_code;
  data.fetch_tries = failure_data.fetch_tries;
}

void InstallStageTracker::ReportFetchError(
    const ExtensionId& id,
    FailureReason reason,
    const ExtensionDownloaderDelegate::FailureData& failure_data) {
  DCHECK(reason == FailureReason::MANIFEST_FETCH_FAILED ||
         reason == FailureReason::CRX_FETCH_FAILED);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  ReportFetchErrorCodes(id, failure_data);
  NotifyObserversOfFailure(id, reason, data);
}

void InstallStageTracker::ReportFailure(const ExtensionId& id,
                                        FailureReason reason) {
  DCHECK_NE(reason, FailureReason::UNKNOWN);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  NotifyObserversOfFailure(id, reason, data);
}

void InstallStageTracker::ReportExtensionType(const ExtensionId& id,
                                              Manifest::Type extension_type) {
  InstallationData& data = installation_data_map_[id];
  data.extension_type = extension_type;
}

void InstallStageTracker::ReportCrxInstallError(
    const ExtensionId& id,
    FailureReason reason,
    CrxInstallErrorDetail crx_install_error) {
  DCHECK(reason == FailureReason::CRX_INSTALL_ERROR_DECLINED ||
         reason == FailureReason::CRX_INSTALL_ERROR_OTHER);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  data.install_error_detail = crx_install_error;
  NotifyObserversOfFailure(id, reason, data);
}

void InstallStageTracker::ReportSandboxedUnpackerFailureReason(
    const ExtensionId& id,
    const CrxInstallError& crx_install_error) {
  std::optional<SandboxedUnpackerFailureReason> unpacker_failure_reason =
      crx_install_error.sandbox_failure_detail();
  DCHECK(unpacker_failure_reason);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason =
      FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE;
  data.unpacker_failure_reason = unpacker_failure_reason;
  if (data.unpacker_failure_reason ==
      SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED) {
    data.unpacker_client_failed_error = crx_install_error.message();
  }
  NotifyObserversOfFailure(
      id, FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE, data);
}

InstallStageTracker::InstallationData InstallStageTracker::Get(
    const ExtensionId& id) {
  auto it = installation_data_map_.find(id);
  return it == installation_data_map_.end() ? InstallationData() : it->second;
}

void InstallStageTracker::Clear() {
  installation_data_map_.clear();
}

void InstallStageTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InstallStageTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstallStageTracker::NotifyObserversOfFailure(
    const ExtensionId& id,
    FailureReason reason,
    const InstallationData& data) {
  for (auto& observer : observers_)
    observer.OnExtensionInstallationFailed(id, reason);
  // Observer::OnExtensionInstallationFailed may change |observers_|, run the
  // loop again to call the other methods for |observers_|.
  for (auto& observer : observers_)
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
}

}  //  namespace extensions
