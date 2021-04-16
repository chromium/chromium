// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_metrics.h"

#include <set>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/browser/updater/extension_downloader.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

using ExtensionStatus = ForceInstalledTracker::ExtensionStatus;
using FailureReason = InstallStageTracker::FailureReason;

namespace {
// Timeout to report UMA if not all force-installed extension were loaded.
constexpr base::TimeDelta kInstallationTimeout =
    base::TimeDelta::FromMinutes(5);

constexpr char kManifestFetchFailedNetworkErrorCode[] =
    "Extensions.ForceInstalledManifestFetchFailedNetworkErrorCode";
constexpr char kManifestFetchFailedFetchTries[] =
    "Extensions.ForceInstalledManifestFetchFailedFetchTries";
constexpr char kCrxFetchFailedNetworkErrorCode[] =
    "Extensions.ForceInstalledNetworkErrorCode";
constexpr char kCrxFetchFailedFetchTries[] =
    "Extensions.ForceInstalledFetchTries";

// This is used to construct histograms for the form
// `Extensions.*ForceInstalledManifestFetchFailedHttpErrorCode2`.
constexpr char kManifestFetchFailedHttpErrorCode[] =
    "ForceInstalledManifestFetchFailedHttpErrorCode2";
// This is used to construct histograms for the form
// `Extensions.*ForceInstalledHttpErrorCode2`.
constexpr char kCrxFetchFailedHttpErrorCode[] = "ForceInstalledHttpErrorCode2";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Helper method to convert user_manager::UserType to
// InstallStageTracker::UserType for histogram purposes.
ForceInstalledMetrics::UserType ConvertUserType(
    InstallStageTracker::UserInfo user_info) {
  switch (user_info.user_type) {
    case user_manager::USER_TYPE_REGULAR: {
      if (user_info.is_new_user)
        return ForceInstalledMetrics::UserType::USER_TYPE_REGULAR_NEW;
      return ForceInstalledMetrics::UserType::USER_TYPE_REGULAR_EXISTING;
    }
    case user_manager::USER_TYPE_GUEST:
      return ForceInstalledMetrics::UserType::USER_TYPE_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return ForceInstalledMetrics::UserType::USER_TYPE_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_KIOSK_APP:
      return ForceInstalledMetrics::UserType::USER_TYPE_KIOSK_APP;
    case user_manager::USER_TYPE_CHILD:
      return ForceInstalledMetrics::UserType::USER_TYPE_CHILD;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return ForceInstalledMetrics::UserType::USER_TYPE_ARC_KIOSK_APP;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return ForceInstalledMetrics::UserType::USER_TYPE_ACTIVE_DIRECTORY;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return ForceInstalledMetrics::UserType::USER_TYPE_WEB_KIOSK_APP;
    default:
      NOTREACHED();
  }
  return ForceInstalledMetrics::UserType::kMaxValue;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Reports time taken for force installed extension during different
// installation stages.
void ReportInstallationStageTimes(
    const ExtensionId& extension_id,
    const InstallStageTracker::InstallationData& installation) {
  if (installation.download_manifest_finish_time &&
      installation.download_manifest_started_time) {
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.DownloadingStartTo."
        "ManifestDownloadComplete",
        installation.download_manifest_finish_time.value() -
            installation.download_manifest_started_time.value());
  }
  // Report the download time for CRX only when
  // installation.download_CRX_started_time is set because in other case CRX
  // is fetched from cache and the download was not started.
  if (installation.download_CRX_finish_time &&
      installation.download_CRX_started_time) {
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.ManifestDownloadCompleteTo."
        "CRXDownloadComplete",
        installation.download_CRX_finish_time.value() -
            installation.download_CRX_started_time.value());
  }
  if (installation.copying_started_time) {
    DCHECK(installation.verification_started_time);
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.VerificationStartTo.CopyingStart",
        installation.copying_started_time.value() -
            installation.verification_started_time.value());
  }
  if (installation.unpacking_started_time &&
      installation.copying_started_time) {
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.CopyingStartTo.UnpackingStart",
        installation.unpacking_started_time.value() -
            installation.copying_started_time.value());
  }
  if (installation.checking_expectations_started_time &&
      installation.unpacking_started_time) {
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.UnpackingStartTo."
        "CheckingExpectationsStart",
        installation.checking_expectations_started_time.value() -
            installation.unpacking_started_time.value());
  }
  if (installation.finalizing_started_time &&
      installation.checking_expectations_started_time) {
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.CheckingExpectationsStartTo."
        "FinalizingStart",
        installation.finalizing_started_time.value() -
            installation.checking_expectations_started_time.value());
  }
  if (installation.installation_complete_time &&
      installation.finalizing_started_time) {
    base::UmaHistogramLongTimes(
        "Extensions.ForceInstalledTime.FinalizingStartTo."
        "CRXInstallComplete",
        installation.installation_complete_time.value() -
            installation.finalizing_started_time.value());
  }
}

// Reports the network error code, HTTP error code and number of fetch tries
// made when extension fails to install with MANIFEST_FETCH_FAILED or
// CRX_FETCH_FAILED.
void ReportErrorCodes(const InstallStageTracker::InstallationData& installation,
                      const std::string& network_error_code_histogram,
                      const std::string& http_error_code_histogram_suffix,
                      const std::string& fetch_tries_histogram,
                      bool is_from_store) {
  base::UmaHistogramSparse(network_error_code_histogram,
                           installation.network_error_code.value());

  if (installation.response_code) {
    if (is_from_store) {
      base::UmaHistogramSparse(
          "Extensions.WebStore_" + http_error_code_histogram_suffix,
          installation.response_code.value());
    } else {
      base::UmaHistogramSparse(
          "Extensions.OffStore_" + http_error_code_histogram_suffix,
          installation.response_code.value());
    }
    base::UmaHistogramSparse("Extensions." + http_error_code_histogram_suffix,
                             installation.response_code.value());
  }
  base::UmaHistogramExactLinear(fetch_tries_histogram,
                                installation.fetch_tries.value(),
                                ExtensionDownloader::kMaxRetries);
}

// Reports installation stage and downloading stage for extensions which are
// currently in progress of the installation.
void ReportCurrentStage(
    const InstallStageTracker::InstallationData& installation) {
  InstallStageTracker::Stage install_stage = installation.install_stage.value();
  base::UmaHistogramEnumeration("Extensions.ForceInstalledStage2",
                                install_stage);
  if (install_stage == InstallStageTracker::Stage::CREATED) {
    DCHECK(installation.install_creation_stage);
    InstallStageTracker::InstallCreationStage install_creation_stage =
        installation.install_creation_stage.value();
    base::UmaHistogramEnumeration("Extensions.ForceInstalledCreationStage",
                                  install_creation_stage);
  }
  if (install_stage == InstallStageTracker::Stage::DOWNLOADING) {
    DCHECK(installation.downloading_stage);
    ExtensionDownloaderDelegate::Stage downloading_stage =
        installation.downloading_stage.value();
    base::UmaHistogramEnumeration("Extensions.ForceInstalledDownloadingStage",
                                  downloading_stage);
  }
}

// Reports detailed failure reason for the extensions which failed to install
// after 5 minutes.
void ReportDetailedFailureReasons(
    Profile* profile,
    const InstallStageTracker::InstallationData& installation,
    const bool is_from_store) {
  FailureReason failure_reason =
      installation.failure_reason.value_or(FailureReason::UNKNOWN);

  // In case of CRX_FETCH_FAILURE, report the network error code, HTTP
  // error code and number of fetch tries made.
  if (failure_reason == FailureReason::CRX_FETCH_FAILED)
    ReportErrorCodes(installation, kCrxFetchFailedNetworkErrorCode,
                     kCrxFetchFailedHttpErrorCode, kCrxFetchFailedFetchTries,
                     is_from_store);

  // In case of MANIFEST_FETCH_FAILURE, report the network error code,
  // HTTP error code and number of fetch tries made.
  if (failure_reason == FailureReason::MANIFEST_FETCH_FAILED)
    ReportErrorCodes(installation, kManifestFetchFailedNetworkErrorCode,
                     kManifestFetchFailedHttpErrorCode,
                     kManifestFetchFailedFetchTries, is_from_store);

  if (installation.install_error_detail) {
    CrxInstallErrorDetail detail = installation.install_error_detail.value();
    base::UmaHistogramEnumeration(
        "Extensions.ForceInstalledFailureCrxInstallError", detail);
  }
  if (installation.unpacker_failure_reason) {
    base::UmaHistogramEnumeration(
        "Extensions.ForceInstalledFailureSandboxUnpackFailureReason2",
        installation.unpacker_failure_reason.value(),
        SandboxedUnpackerFailureReason::NUM_FAILURE_REASONS);
  }
  if (failure_reason == FailureReason::CRX_FETCH_URL_EMPTY) {
    DCHECK(installation.no_updates_info);
    base::UmaHistogramEnumeration(
        "Extensions."
        "ForceInstalledFailureNoUpdatesInfo",
        installation.no_updates_info.value());
  }
  if (installation.manifest_invalid_error) {
    DCHECK_EQ(failure_reason, FailureReason::MANIFEST_INVALID);
    base::UmaHistogramEnumeration(
        "Extensions.ForceInstalledFailureManifestInvalidErrorDetail2",
        installation.manifest_invalid_error.value());
    if (installation.app_status_error) {
      base::UmaHistogramEnumeration(
          "Extensions.ForceInstalledFailureManifestInvalidAppStatusError",
          installation.app_status_error.value());
    }
  }
  if (installation.unpacker_failure_reason &&
      installation.unpacker_failure_reason.value() ==
          SandboxedUnpackerFailureReason::CRX_HEADER_INVALID) {
    base::UmaHistogramBoolean(
        "Extensions.ForceInstalledFailureWithCrxHeaderInvalidIsCWS",
        is_from_store);
    base::UmaHistogramBoolean(
        "Extensions.ForceInstalledFailureWithCrxHeaderInvalidIsFromCache",
        ForceInstalledTracker::IsExtensionFetchedFromCache(
            installation.downloading_cache_status));
  }

  if (installation.failure_reason == FailureReason::IN_PROGRESS &&
      installation.install_creation_stage ==
          InstallStageTracker::InstallCreationStage::
              NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED) {
    base::UmaHistogramBoolean(
        "Extensions."
        "ForceInstalledFailureStuckInInitialCreationStageAreExtensionsEnabled",
        ExtensionSystem::Get(profile)
            ->extension_service()
            ->extensions_enabled());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Report type of user in case Force Installed Extensions fail to
// install only if there is a user corresponding to given profile.
void ReportUserType(Profile* profile, bool is_stuck_in_initial_creation_stage) {
  InstallStageTracker::UserInfo user_info =
      InstallStageTracker::GetUserInfo(profile);
  // There can be extensions on the login screen. There is no user on the login
  // screen and thus we would not report in that case.
  if (!user_info.is_user_present)
    return;

  ForceInstalledMetrics::UserType user_type = ConvertUserType(user_info);
  base::UmaHistogramEnumeration("Extensions.ForceInstalledFailureSessionType",
                                user_type);
  if (is_stuck_in_initial_creation_stage) {
    base::UmaHistogramEnumeration(
        "Extensions.ForceInstalledFailureSessionType."
        "ExtensionStuckInInitialCreationStage",
        user_type);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ForceInstalledMetrics::ForceInstalledMetrics(
    ExtensionRegistry* registry,
    Profile* profile,
    ForceInstalledTracker* tracker,
    std::unique_ptr<base::OneShotTimer> timer)
    : registry_(registry),
      profile_(profile),
      tracker_(tracker),
      start_time_(base::Time::Now()),
      timer_(std::move(timer)) {
  timer_->Start(
      FROM_HERE, kInstallationTimeout,
      base::BindOnce(&ForceInstalledMetrics::OnForceInstalledExtensionsLoaded,
                     base::Unretained(this)));
  if (tracker_->IsDoneLoading())
    OnForceInstalledExtensionsLoaded();
  else
    tracker_observation_.Observe(tracker_);
}

ForceInstalledMetrics::~ForceInstalledMetrics() = default;

bool ForceInstalledMetrics::IsStatusGood(ExtensionStatus status) {
  switch (status) {
    case ExtensionStatus::kPending:
      return false;
    case ExtensionStatus::kLoaded:
      return true;
    case ExtensionStatus::kReady:
      return true;
    case ExtensionStatus::kFailed:
      return false;
    default:
      NOTREACHED();
  }
  return false;
}

void ForceInstalledMetrics::ReportDisableReason(
    const ExtensionId& extension_id) {
  int disable_reasons =
      ExtensionPrefs::Get(profile_)->GetDisableReasons(extension_id);
  // Choose any disable reason among the disable reasons for this extension.
  disable_reasons = disable_reasons & ~(disable_reasons - 1);
  base::UmaHistogramSparse("Extensions.ForceInstalledNotLoadedDisableReason",
                           disable_reasons);
}

void ForceInstalledMetrics::ReportMetricsOnExtensionsReady() {
  for (const auto& extension : tracker_->extensions()) {
    if (extension.second.status != ExtensionStatus::kReady)
      return;
  }
  base::UmaHistogramLongTimes("Extensions.ForceInstalledReadyTime",
                              base::Time::Now() - start_time_);
}

void ForceInstalledMetrics::ReportMetrics() {
  base::UmaHistogramCounts100("Extensions.ForceInstalledTotalCandidateCount",
                              tracker_->extensions().size());
  std::set<ExtensionId> missing_forced_extensions;
  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);
  for (const auto& extension : tracker_->extensions()) {
    if (!IsStatusGood(extension.second.status)) {
      missing_forced_extensions.insert(extension.first);
    } else {
      InstallStageTracker::InstallationData installation =
          install_stage_tracker->Get(extension.first);
      ReportInstallationStageTimes(extension.first, installation);
    }
  }
  if (missing_forced_extensions.empty()) {
    base::UmaHistogramLongTimes("Extensions.ForceInstalledLoadTime",
                                base::Time::Now() - start_time_);
    // TODO(burunduk): Remove VLOGs after resolving crbug/917700 and
    // crbug/904600.
    VLOG(2) << "All forced extensions seem to be installed";
    return;
  }
  size_t enabled_missing_count = missing_forced_extensions.size();
  size_t blocklisted_count = 0;
  auto installed_extensions = registry_->GenerateInstalledExtensionsSet();
  auto blocklisted_extensions = registry_->GenerateInstalledExtensionsSet(
      ExtensionRegistry::IncludeFlag::BLOCKLISTED);
  for (const auto& entry : *installed_extensions) {
    if (missing_forced_extensions.count(entry->id())) {
      missing_forced_extensions.erase(entry->id());
      ReportDisableReason(entry->id());
      if (blocklisted_extensions->Contains(entry->id()))
        blocklisted_count++;
    }
  }
  size_t misconfigured_extensions = 0;
  size_t installed_missing_count = missing_forced_extensions.size();

  base::UmaHistogramCounts100("Extensions.ForceInstalledTimedOutCount",
                              enabled_missing_count);
  base::UmaHistogramCounts100(
      "Extensions.ForceInstalledTimedOutAndNotInstalledCount",
      installed_missing_count);
  base::UmaHistogramCounts100("Extensions.ForceInstalledAndBlackListed",
                              blocklisted_count);
  VLOG(2) << "Failed to install " << installed_missing_count
          << " forced extensions.";
  for (const auto& extension_id : missing_forced_extensions) {
    InstallStageTracker::InstallationData installation =
        install_stage_tracker->Get(extension_id);
    base::UmaHistogramEnumeration(
        "Extensions.ForceInstalledFailureCacheStatus",
        installation.downloading_cache_status.value_or(
            ExtensionDownloaderDelegate::CacheStatus::CACHE_UNKNOWN));
    if (!installation.failure_reason && installation.install_stage) {
      installation.failure_reason = FailureReason::IN_PROGRESS;
      ReportCurrentStage(installation);
    }
    if (tracker_->IsMisconfiguration(installation, extension_id))
      misconfigured_extensions++;
    FailureReason failure_reason =
        installation.failure_reason.value_or(FailureReason::UNKNOWN);
    base::UmaHistogramEnumeration("Extensions.ForceInstalledFailureReason3",
                                  failure_reason);
    bool is_from_store = tracker_->extensions().at(extension_id).is_from_store;
    if (is_from_store) {
      base::UmaHistogramEnumeration(
          "Extensions.WebStore_ForceInstalledFailureReason3", failure_reason);
    } else {
      base::UmaHistogramEnumeration(
          "Extensions.OffStore_ForceInstalledFailureReason3", failure_reason);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    bool is_stuck_in_initial_creation_stage =
        failure_reason == FailureReason::IN_PROGRESS &&
        installation.install_stage == InstallStageTracker::Stage::CREATED &&
        installation.install_creation_stage ==
            InstallStageTracker::InstallCreationStage::
                NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED;
    ReportUserType(profile_, is_stuck_in_initial_creation_stage);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    VLOG(2) << "Forced extension " << extension_id
            << " failed to install with data="
            << InstallStageTracker::GetFormattedInstallationData(installation);
    ReportDetailedFailureReasons(profile_, installation, is_from_store);
  }
  bool non_misconfigured_failure_occurred =
      misconfigured_extensions != missing_forced_extensions.size();
  base::UmaHistogramBoolean(
      "Extensions."
      "ForceInstalledSessionsWithNonMisconfigurationFailureOccured",
      non_misconfigured_failure_occurred);
}

void ForceInstalledMetrics::OnForceInstalledExtensionsLoaded() {
  if (load_reported_)
    return;
  // Report only if there was non-empty list of force-installed extensions.
  if (!tracker_->extensions().empty())
    ReportMetrics();
  load_reported_ = true;
  timer_->Stop();
}

void ForceInstalledMetrics::OnForceInstalledExtensionsReady() {
  if (ready_reported_)
    return;
  // Report only if there was non-empty list of force-installed extensions.
  if (!tracker_->extensions().empty())
    ReportMetricsOnExtensionsReady();
  ready_reported_ = true;
}

void ForceInstalledMetrics::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::CacheStatus cache_status) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.ForceInstalledCacheStatus",
                            cache_status);
}

}  //  namespace extensions
