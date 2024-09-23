// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/download_item_utils.h"

using DeepScanTrigger = DownloadItemWarningData::DeepScanTrigger;

namespace safe_browsing {

namespace {

DownloadCheckResult GetHighestPrecedenceResult(DownloadCheckResult result_1,
                                               DownloadCheckResult result_2) {
  // Don't use the enum's int values to determine precedence since that
  // may introduce bugs for new actions later.
  //
  // The current precedence is
  //   blocking > bypassable warning > scan failure > safe
  // with malware verdicts having precedence over DLP verdicts.
  //
  // This matches the precedence established in ResponseToDownloadCheckResult.
  constexpr DownloadCheckResult kDownloadCheckResultPrecedence[] = {
      DownloadCheckResult::DANGEROUS,
      DownloadCheckResult::SENSITIVE_CONTENT_BLOCK,
      DownloadCheckResult::BLOCKED_TOO_LARGE,
      DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED,
      DownloadCheckResult::BLOCKED_SCAN_FAILED,
      DownloadCheckResult::POTENTIALLY_UNWANTED,
      DownloadCheckResult::SENSITIVE_CONTENT_WARNING,
      DownloadCheckResult::PROMPT_FOR_SCANNING,
      DownloadCheckResult::DEEP_SCANNED_FAILED,
      DownloadCheckResult::UNKNOWN,
      DownloadCheckResult::DEEP_SCANNED_SAFE};

  for (DownloadCheckResult result : kDownloadCheckResultPrecedence) {
    if (result_1 == result || result_2 == result) {
      return result;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return DownloadCheckResult::UNKNOWN;
}

void ResponseToDownloadCheckResult(
    const enterprise_connectors::ContentAnalysisResponse& response,
    DownloadCheckResult* download_result) {
  bool malware_scan_failure = false;
  bool dlp_scan_failure = false;
  auto malware_action =
      enterprise_connectors::TriggeredRule::ACTION_UNSPECIFIED;
  auto dlp_action = enterprise_connectors::TriggeredRule::ACTION_UNSPECIFIED;

  for (const auto& result : response.results()) {
    if (result.tag() == "malware") {
      if (result.status() !=
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
        malware_scan_failure = true;
        continue;
      }
      for (const auto& rule : result.triggered_rules()) {
        malware_action = enterprise_connectors::GetHighestPrecedenceAction(
            malware_action, rule.action());
      }
    }
    if (result.tag() == "dlp") {
      if (result.status() !=
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
        dlp_scan_failure = true;
        continue;
      }
      for (const auto& rule : result.triggered_rules()) {
        dlp_action = enterprise_connectors::GetHighestPrecedenceAction(
            dlp_action, rule.action());
      }
    }
  }

  if (malware_action == enterprise_connectors::GetHighestPrecedenceAction(
                            malware_action, dlp_action)) {
    switch (malware_action) {
      case enterprise_connectors::TriggeredRule::BLOCK:
        *download_result = DownloadCheckResult::DANGEROUS;
        return;
      case enterprise_connectors::TriggeredRule::WARN:
        *download_result = DownloadCheckResult::POTENTIALLY_UNWANTED;
        return;
      case enterprise_connectors::TriggeredRule::REPORT_ONLY:
      case enterprise_connectors::TriggeredRule::ACTION_UNSPECIFIED:
        break;
    }
  } else {
    switch (dlp_action) {
      case enterprise_connectors::TriggeredRule::BLOCK:
        *download_result = DownloadCheckResult::SENSITIVE_CONTENT_BLOCK;
        return;
      case enterprise_connectors::TriggeredRule::WARN:
        *download_result = DownloadCheckResult::SENSITIVE_CONTENT_WARNING;
        return;
      case enterprise_connectors::TriggeredRule::REPORT_ONLY:
      case enterprise_connectors::TriggeredRule::ACTION_UNSPECIFIED:
        break;
    }
  }

  if (dlp_scan_failure || malware_scan_failure) {
    *download_result = DownloadCheckResult::DEEP_SCANNED_FAILED;
    return;
  }

  *download_result = DownloadCheckResult::DEEP_SCANNED_SAFE;
}

EventResult GetEventResult(download::DownloadDangerType danger_type,
                           download::DownloadItem* item) {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));
  if (download_core_service) {
    ChromeDownloadManagerDelegate* delegate =
        download_core_service->GetDownloadManagerDelegate();
    if (delegate && delegate->ShouldBlockFile(item, danger_type)) {
      return EventResult::BLOCKED;
    }
  }

  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return EventResult::WARNED;

    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return EventResult::ALLOWED;

    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return EventResult::BYPASSED;

    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return EventResult::UNKNOWN;
  }
}

EventResult GetEventResult(DownloadCheckResult download_result,
                           Profile* profile) {
  auto download_restriction =
      profile
          ? static_cast<DownloadPrefs::DownloadRestriction>(
                profile->GetPrefs()->GetInteger(prefs::kDownloadRestrictions))
          : DownloadPrefs::DownloadRestriction::NONE;
  switch (download_result) {
    case DownloadCheckResult::UNKNOWN:
    case DownloadCheckResult::SAFE:
    case DownloadCheckResult::ALLOWLISTED_BY_POLICY:
    case DownloadCheckResult::DEEP_SCANNED_SAFE:
      return EventResult::ALLOWED;

    // The following results return WARNED or BLOCKED depending on
    // |download_restriction|.
    case DownloadCheckResult::DANGEROUS:
    case DownloadCheckResult::DANGEROUS_HOST:
    case DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
      switch (download_restriction) {
        case DownloadPrefs::DownloadRestriction::ALL_FILES:
        case DownloadPrefs::DownloadRestriction::POTENTIALLY_DANGEROUS_FILES:
        case DownloadPrefs::DownloadRestriction::DANGEROUS_FILES:
        case DownloadPrefs::DownloadRestriction::MALICIOUS_FILES:
          return EventResult::BLOCKED;
        case DownloadPrefs::DownloadRestriction::NONE:
          return EventResult::WARNED;
      }

    case DownloadCheckResult::UNCOMMON:
    case DownloadCheckResult::POTENTIALLY_UNWANTED:
    case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
      return EventResult::WARNED;

    case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
    case DownloadCheckResult::BLOCKED_TOO_LARGE:
    case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
    case DownloadCheckResult::BLOCKED_SCAN_FAILED:
      return EventResult::BLOCKED;

    default:
      NOTREACHED_IN_MIGRATION() << "Should never be final result";
      break;
  }
  return EventResult::UNKNOWN;
}

std::string GetTriggerName(DeepScanTrigger trigger) {
  switch (trigger) {
    case DeepScanTrigger::TRIGGER_UNKNOWN:
      return "Unknown";
    case DeepScanTrigger::TRIGGER_CONSUMER_PROMPT:
      return "ConsumerPrompt";
    case DeepScanTrigger::TRIGGER_POLICY:
      return "Policy";
    case DeepScanTrigger::TRIGGER_ENCRYPTED_CONSUMER_PROMPT:
      return "EncryptedConsumerPrompt";
    case DeepScanTrigger::TRIGGER_IMMEDIATE_DEEP_SCAN:
      return "ImmediateDeepScan";
  }
}

enterprise_connectors::ContentAnalysisAcknowledgement::FinalAction
GetFinalAction(EventResult event_result) {
  auto final_action =
      enterprise_connectors::ContentAnalysisAcknowledgement::ALLOW;
  switch (event_result) {
    case EventResult::UNKNOWN:
    case EventResult::ALLOWED:
    case EventResult::BYPASSED:
      break;
    case EventResult::WARNED:
      final_action =
          enterprise_connectors::ContentAnalysisAcknowledgement::WARN;
      break;
    case EventResult::BLOCKED:
      final_action =
          enterprise_connectors::ContentAnalysisAcknowledgement::BLOCK;
      break;
  }

  return final_action;
}

void PromptForPassword(download::DownloadItem* item) {
  if (DownloadBubbleUIController* controller =
          DownloadBubbleUIController::GetForDownload(item);
      controller) {
    controller->GetDownloadDisplayController()->OpenSecuritySubpage(
        OfflineItemUtils::GetContentIdForDownload(item));
  }
}

void LogDeepScanResult(DownloadCheckResult download_result,
                       DeepScanTrigger trigger,
                       bool is_encrypted_archive) {
  base::UmaHistogramEnumeration(
      "SBClientDownload.MalwareDeepScanResult2." + GetTriggerName(trigger),
      download_result);
  if (is_encrypted_archive) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.PasswordProtectedMalwareDeepScanResult2." +
            GetTriggerName(trigger),
        download_result);
  }
}

bool HasDecryptionFailedResult(
    enterprise_connectors::ContentAnalysisResponse response) {
  for (const auto& result : response.results()) {
    if (result.tag() != "malware") {
      continue;
    }

    if (result.status_error_message() ==
        enterprise_connectors::ContentAnalysisResponse::Result::
            DECRYPTION_FAILED) {
      return true;
    }
  }

  return false;
}

bool EnterpriseResultIsFailure(BinaryUploadService::Result result,
                               bool block_large_files,
                               bool block_password_protected_files) {
  return enterprise_connectors::IsResumableUploadEnabled()
             ? enterprise_connectors::CloudResumableResultIsFailure(
                   result, block_large_files, block_password_protected_files)
             : enterprise_connectors::CloudMultipartResultIsFailure(result);
}

void RecordEnterpriseScan(std::unique_ptr<FileAnalysisRequest> request,
                          BinaryUploadService::Result result) {
  const std::string result_info =
      safe_browsing::BinaryUploadService::ResultToString(result);
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), /*access_token*/ "",
      /*upload_info*/ base::StrCat({"Skipped - ", result_info}),
      /*upload_url*/ "", request->content_analysis_request());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      /*token=*/"", result_info,
      enterprise_connectors::ContentAnalysisResponse());
}

}  // namespace

/* static */
std::optional<enterprise_connectors::AnalysisSettings>
DeepScanningRequest::ShouldUploadBinary(download::DownloadItem* item) {
  // Files already on the disk shouldn't be uploaded for scanning.
  if (item->GetURL().SchemeIsFile()) {
    return std::nullopt;
  }

  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));

  // If the download Connector is not enabled, don't scan.
  if (!service ||
      !service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED)) {
    return std::nullopt;
  }

  // Check that item->GetURL() matches the appropriate URL patterns by getting
  // settings. No settings means no matches were found and that the downloaded
  // file shouldn't be uploaded.
  return service->GetAnalysisSettings(
      item->GetURL(),
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
}

DeepScanningRequest::DeepScanningRequest(
    download::DownloadItem* item,
    DeepScanTrigger trigger,
    DownloadCheckResult pre_scan_download_check_result,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* download_service,
    enterprise_connectors::AnalysisSettings settings,
    base::optional_ref<const std::string> password)
    : item_(item),
      trigger_(trigger),
      callback_(callback),
      download_service_(download_service),
      analysis_settings_(std::move(settings)),
      pending_scan_requests_(1),
      pre_scan_download_check_result_(pre_scan_download_check_result),
      password_(password.CopyAsOptional()),
      reason_(enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD),
      weak_ptr_factory_(this) {
  base::UmaHistogramEnumeration("SBClientDownload.DeepScanType",
                                DeepScanType::NORMAL);
  item_->AddObserver(this);
}

DeepScanningRequest::DeepScanningRequest(
    download::DownloadItem* item,
    DownloadCheckResult pre_scan_download_check_result,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* download_service,
    enterprise_connectors::AnalysisSettings settings,
    base::flat_map<base::FilePath, base::FilePath> save_package_files)
    : item_(item),
      trigger_(DeepScanTrigger::TRIGGER_POLICY),
      callback_(callback),
      download_service_(download_service),
      analysis_settings_(std::move(settings)),
      save_package_files_(std::move(save_package_files)),
      pending_scan_requests_(save_package_files_.size()),
      pre_scan_download_check_result_(pre_scan_download_check_result),
      reason_(enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD),
      weak_ptr_factory_(this) {
  base::UmaHistogramEnumeration("SBClientDownload.DeepScanType",
                                DeepScanType::SAVE_PACKAGE);
  base::UmaHistogramCounts10000("SBClientDownload.SavePackageFileCount",
                                save_package_files_.size());
  item_->AddObserver(this);
}

DeepScanningRequest::~DeepScanningRequest() {
  item_->RemoveObserver(this);
}

void DeepScanningRequest::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeepScanningRequest::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeepScanningRequest::Start() {
  // Indicate we're now scanning the file.
  pre_scan_danger_type_ = item_->GetDangerType();

  if (ReportOnlyScan()) {
    // In non-blocking mode, run `callback_` immediately so the download
    // completes, then start the scanning process only after the downloaded
    // files have been renamed. `callback_` is also reset so that the download
    // is no longer updated after scanning finishes.
    callback_.Run(pre_scan_download_check_result_);
    callback_.Reset();
    return;
  }

  DownloadItemWarningData::SetDeepScanTrigger(item_, trigger_);
  callback_.Run(DownloadCheckResult::ASYNC_SCANNING);
  if (save_package_files_.empty()) {
    StartSingleFileScan();
  } else {
    StartSavePackageScan();
  }
}

void DeepScanningRequest::StartSingleFileScan() {
  DCHECK(!scanning_started_);
  scanning_started_ = true;
  IncrementCrashKey(ScanningCrashKey::PENDING_FILE_DOWNLOADS);
  IncrementCrashKey(ScanningCrashKey::TOTAL_FILE_DOWNLOADS);

  enterprise_obfuscation::DownloadObfuscationData* obfuscation_data =
      static_cast<enterprise_obfuscation::DownloadObfuscationData*>(
          item_->GetUserData(
              enterprise_obfuscation::DownloadObfuscationData::kUserDataKey));

  auto request = std::make_unique<FileAnalysisRequest>(
      analysis_settings_, item_->GetFullPath(),
      item_->GetTargetFilePath().BaseName(), item_->GetMimeType(),
      /* delay_opening_file */ false,
      base::BindOnce(&DeepScanningRequest::OnScanComplete,
                     weak_ptr_factory_.GetWeakPtr(), item_->GetFullPath()),
      base::DoNothing(),
      obfuscation_data ? obfuscation_data->is_obfuscated : false);

  request->set_filename(item_->GetTargetFilePath().AsUTF8Unsafe());

  std::string sha256 = base::HexEncode(item_->GetHash());
  request->set_digest(sha256);

  if (password_) {
    request->set_password(*password_);
  }

  file_metadata_.insert({item_->GetFullPath(),
                         enterprise_connectors::FileMetadata(
                             item_->GetTargetFilePath().AsUTF8Unsafe(), sha256,
                             item_->GetMimeType(), item_->GetTotalBytes())});

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));

  base::UmaHistogramEnumeration("SBClientDownload.DeepScanTrigger", trigger_);

  FileAnalysisRequest* request_raw = request.get();
  PopulateRequest(request_raw, profile, item_->GetFullPath());
  request_raw->GetRequestData(
      base::BindOnce(&DeepScanningRequest::OnGetFileRequestData,
                     weak_ptr_factory_.GetWeakPtr(), item_->GetFullPath(),
                     std::move(request)));
}

void DeepScanningRequest::StartSavePackageScan() {
  DCHECK(!scanning_started_);
  scanning_started_ = true;
  IncrementCrashKey(ScanningCrashKey::PENDING_FILE_DOWNLOADS,
                    pending_scan_requests_);
  IncrementCrashKey(ScanningCrashKey::TOTAL_FILE_DOWNLOADS,
                    pending_scan_requests_);

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  std::vector<FileOpeningJob::FileOpeningTask> tasks(pending_scan_requests_);
  size_t i = 0;
  for (const auto& tmp_path_and_final_path : save_package_files_) {
    base::FilePath file_location = ReportOnlyScan()
                                       ? tmp_path_and_final_path.second
                                       : tmp_path_and_final_path.first;
    auto request = std::make_unique<FileAnalysisRequest>(
        analysis_settings_, file_location,
        tmp_path_and_final_path.second.BaseName(), /*mimetype*/ "",
        /* delay_opening_file */ true,
        base::BindOnce(&DeepScanningRequest::OnScanComplete,
                       weak_ptr_factory_.GetWeakPtr(), file_location));
    request->set_filename(tmp_path_and_final_path.second.AsUTF8Unsafe());

    FileAnalysisRequest* request_raw = request.get();
    PopulateRequest(request_raw, profile, file_location);
    request_raw->GetRequestData(base::BindOnce(
        &DeepScanningRequest::OnGetPackageFileRequestData,
        weak_ptr_factory_.GetWeakPtr(), tmp_path_and_final_path.second,
        file_location, std::move(request)));
    DCHECK_LT(i, tasks.size());
    tasks[i++].request = request_raw;
  }
  DCHECK_EQ(i, tasks.size());

  file_opening_job_ = std::make_unique<FileOpeningJob>(std::move(tasks));
}

void DeepScanningRequest::PopulateRequest(FileAnalysisRequest* request,
                                          Profile* profile,
                                          const base::FilePath& path) {
  if (IsEnterpriseTriggered()) {
    if (analysis_settings_.cloud_or_local_settings.is_cloud_analysis()) {
      request->set_device_token(
          analysis_settings_.cloud_or_local_settings.dm_token());
    }
    request->set_per_profile_request(analysis_settings_.per_profile);
    if (analysis_settings_.client_metadata) {
      request->set_client_metadata(*analysis_settings_.client_metadata);
    }
    request->set_reason(reason_);
  }

  request->set_analysis_connector(enterprise_connectors::FILE_DOWNLOADED);
  request->set_email(enterprise_connectors::GetProfileEmail(profile));

  if (item_->GetURL().is_valid()) {
    request->set_url(item_->GetURL().spec());
  }

  if (item_->GetTabUrl().is_valid()) {
    request->set_tab_url(item_->GetTabUrl());
  }

  if (file_metadata_.count(path) &&
      !file_metadata_.at(path).mime_type.empty()) {
    request->set_content_type(file_metadata_.at(path).mime_type);
  }

  for (const auto& tag : analysis_settings_.tags) {
    request->add_tag(tag.first);
  }

  request->set_blocking(analysis_settings_.block_until_verdict !=
                        enterprise_connectors::BlockUntilVerdict::kNoBlock);
}

void DeepScanningRequest::PrepareClientDownloadRequest(
    const base::FilePath& current_path,
    std::unique_ptr<FileAnalysisRequest> request) {
  if (IsEnterpriseTriggered()) {
    download_request_maker_ = DownloadRequestMaker::CreateFromDownloadItem(
        new BinaryFeatureExtractor(), item_);
    download_request_maker_->Start(base::BindOnce(
        &DeepScanningRequest::OnDownloadRequestReady,
        weak_ptr_factory_.GetWeakPtr(), current_path, std::move(request)));
  } else {
    OnDownloadRequestReady(current_path, std::move(request), nullptr);
  }
}

void DeepScanningRequest::OnGetPackageFileRequestData(
    const base::FilePath& final_path,
    const base::FilePath& current_path,
    std::unique_ptr<FileAnalysisRequest> request,
    BinaryUploadService::Result result,
    BinaryUploadService::Request::Data data) {
  file_metadata_.insert({current_path, enterprise_connectors::FileMetadata(
                                           final_path.AsUTF8Unsafe(), data.hash,
                                           data.mime_type, data.size)});
  if (ShouldTerminateEarly(result)) {
    // We record the scan here because the request is terminated early and won't
    // be uploaded to CloudBinaryUploadService.
    if (IsEnterpriseTriggered()) {
      RecordEnterpriseScan(std::move(request), result);
    }
    OnScanComplete(current_path, result,
                   enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  OnDownloadRequestReady(current_path, std::move(request), nullptr);
}

void DeepScanningRequest::OnGetFileRequestData(
    const base::FilePath& file_path,
    std::unique_ptr<FileAnalysisRequest> request,
    BinaryUploadService::Result result,
    BinaryUploadService::Request::Data data) {
  if (ShouldTerminateEarly(result)) {
    // We record the scan here because the request is terminated early and won't
    // be uploaded to CloudBinaryUploadService.
    if (IsEnterpriseTriggered()) {
      RecordEnterpriseScan(std::move(request), result);
    }
    OnScanComplete(file_path, result,
                   enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  PrepareClientDownloadRequest(file_path, std::move(request));
}

void DeepScanningRequest::OnDownloadRequestReady(
    const base::FilePath& current_path,
    std::unique_ptr<FileAnalysisRequest> deep_scan_request,
    std::unique_ptr<ClientDownloadRequest> download_request) {
  if (download_request) {
    deep_scan_request->set_csd(*download_request);
  }

  upload_start_times_[current_path] = base::TimeTicks::Now();
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  BinaryUploadService* binary_upload_service =
      download_service_->GetBinaryUploadService(profile, analysis_settings_);
  if (binary_upload_service) {
    binary_upload_service->MaybeUploadForDeepScanning(
        std::move(deep_scan_request));
  } else {
    OnScanComplete(current_path, BinaryUploadService::Result::UNKNOWN,
                   enterprise_connectors::ContentAnalysisResponse());
  }
}

void DeepScanningRequest::OnScanComplete(
    const base::FilePath& current_path,
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordDeepScanMetrics(
      analysis_settings_.cloud_or_local_settings.is_cloud_analysis(),
      /*access_point=*/DeepScanAccessPoint::DOWNLOAD,
      /*duration=*/base::TimeTicks::Now() - upload_start_times_[current_path],
      /*total_size=*/item_->GetTotalBytes(), /*result=*/result,
      /*response=*/response);

  if (IsConsumerTriggered()) {
    OnConsumerScanComplete(current_path, result, response);
  } else if (IsEnterpriseTriggered()) {
    OnEnterpriseScanComplete(current_path, result, response);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void DeepScanningRequest::OnConsumerScanComplete(
    const base::FilePath& current_path,
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  bool is_invalid_password =
      result == BinaryUploadService::Result::FILE_ENCRYPTED ||
      (result == BinaryUploadService::Result::SUCCESS &&
       DownloadItemWarningData::IsTopLevelEncryptedArchive(item_) &&
       HasDecryptionFailedResult(response));
  bool is_success =
      result == BinaryUploadService::Result::SUCCESS && !is_invalid_password;
  CHECK(IsConsumerTriggered());
  DownloadCheckResult download_result = DownloadCheckResult::UNKNOWN;
  if (is_success) {
    request_tokens_.push_back(response.request_token());
    ResponseToDownloadCheckResult(response, &download_result);
    LogDeepScanEvent(item_, DeepScanEvent::kScanCompleted);
  } else if (is_invalid_password) {
    // Since we now prompt the user for a password, FILE_ENCRYPTED indicates
    // the password was not correct. Instead of failing, ask the user to
    // correct the issue.
    DownloadItemWarningData::SetHasIncorrectPassword(item_, true);
    PromptForPassword(item_);
    download_result = DownloadCheckResult::PROMPT_FOR_SCANNING;
    LogDeepScanEvent(item_, DeepScanEvent::kIncorrectPassword);
    base::UmaHistogramBoolean(
        "SBClientDownload.DeepScan.IncorrectPasswordVerdictIsLocal",
        result == BinaryUploadService::Result::FILE_ENCRYPTED);
  } else {
    download_result = DownloadCheckResult::DEEP_SCANNED_FAILED;
    LogDeepScanEvent(item_, DeepScanEvent::kScanFailed);
  }

  LogDeepScanResult(download_result, trigger_,
                    DownloadItemWarningData::IsTopLevelEncryptedArchive(item_));

  DCHECK(file_metadata_.count(current_path));
  file_metadata_.at(current_path).scan_response = std::move(response);
  MaybeFinishRequest(download_result);
}

void DeepScanningRequest::OnEnterpriseScanComplete(
    const base::FilePath& current_path,
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  CHECK(IsEnterpriseTriggered());
  DownloadCheckResult download_result = DownloadCheckResult::UNKNOWN;
  if (result == BinaryUploadService::Result::SUCCESS) {
    request_tokens_.push_back(response.request_token());
    ResponseToDownloadCheckResult(response, &download_result);
  } else if (result == BinaryUploadService::Result::FILE_TOO_LARGE &&
             analysis_settings_.block_large_files) {
    download_result = DownloadCheckResult::BLOCKED_TOO_LARGE;
  } else if (result == BinaryUploadService::Result::FILE_ENCRYPTED &&
             analysis_settings_.block_password_protected_files) {
    download_result = DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED;
  } else if (enterprise_connectors::ResultIsFailClosed(result) &&
             analysis_settings_.default_action ==
                 enterprise_connectors::DefaultAction::kBlock) {
    download_result = DownloadCheckResult::BLOCKED_SCAN_FAILED;
  }

  LogDeepScanResult(download_result, trigger_,
                    DownloadItemWarningData::IsTopLevelEncryptedArchive(item_));

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  DCHECK(file_metadata_.count(current_path));
  file_metadata_.at(current_path).scan_response = std::move(response);
  if (profile) {
    const auto& file_metadata = file_metadata_.at(current_path);
    report_callbacks_.AddUnsafe(base::BindOnce(
        &MaybeReportDeepScanningVerdict, profile, item_->GetURL(),
        item_->GetTabUrl(), "", "", file_metadata.filename,
        file_metadata.sha256, file_metadata.mime_type,
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload, "",
        DeepScanAccessPoint::DOWNLOAD, file_metadata.size, result,
        file_metadata.scan_response));

    enterprise_connectors::ScanResult* stored_result =
        static_cast<enterprise_connectors::ScanResult*>(
            item_->GetUserData(enterprise_connectors::ScanResult::kKey));
    if (stored_result) {
      stored_result->file_metadata.push_back(file_metadata);
    } else {
      auto scan_result =
          std::make_unique<enterprise_connectors::ScanResult>(file_metadata);
      item_->SetUserData(enterprise_connectors::ScanResult::kKey,
                         std::move(scan_result));
    }
  }

  MaybeFinishRequest(download_result);
}

void DeepScanningRequest::OnDownloadUpdated(download::DownloadItem* download) {
  DCHECK_EQ(download, item_);

  if (ReportOnlyScan() &&
      item_->GetState() == download::DownloadItem::COMPLETE &&
      !scanning_started_) {
    // Now that the download is complete in non-blocking mode, scanning can
    // start since the files have moved to their final destination.
    if (save_package_files_.empty()) {
      StartSingleFileScan();
    } else {
      StartSavePackageScan();
    }
  }
}

void DeepScanningRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  DCHECK_EQ(download, item_);

  if (download->IsSavePackageDownload()) {
    enterprise_connectors::RunSavePackageScanningCallback(download, false);
  }

  // We can't safely return a verdict for this download because it's already
  // been destroyed, so reset the callback here. We still need to run
  // `FinishRequest` to notify the DownloadProtectionService that this deep scan
  // has finished.
  callback_.Reset();

  FinishRequest(DownloadCheckResult::UNKNOWN);
}

void DeepScanningRequest::MaybeFinishRequest(DownloadCheckResult result) {
  download_check_result_ =
      GetHighestPrecedenceResult(download_check_result_, result);
  DecrementCrashKey(ScanningCrashKey::PENDING_FILE_DOWNLOADS);

  if ((--pending_scan_requests_) == 0) {
    FinishRequest(download_check_result_);
  }
}

void DeepScanningRequest::FinishRequest(DownloadCheckResult result) {
  EventResult event_result = EventResult::UNKNOWN;

  if (!report_callbacks_.empty()) {
    DCHECK(IsEnterpriseTriggered());

    if (ReportOnlyScan()) {
      // The event result in report-only will always match whatever danger type
      // known before deep scanning since the UI will never be updated based on
      // `result`.
      event_result = GetEventResult(pre_scan_danger_type_, item_);
    } else {
      Profile* profile = Profile::FromBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item_));
      // If FinishRequest is reached with an unknown `result` or an explicit
      // failure, then it means no scanning request ever completed successfully,
      // so `event_result` needs to reflect whatever danger type was known
      // pre-deep scanning.
      event_result = (result == DownloadCheckResult::DEEP_SCANNED_FAILED ||
                      result == DownloadCheckResult::UNKNOWN)
                         ? GetEventResult(pre_scan_danger_type_, item_)
                         : GetEventResult(result, profile);
    }

    report_callbacks_.Notify(event_result);
  }

  // If the deep-scanning result is unknown for whatever reason, `callback_`
  // should be called with whatever SB result was known prior to deep scanning.
  if ((result == DownloadCheckResult::UNKNOWN ||
       result == DownloadCheckResult::DEEP_SCANNED_FAILED) &&
      IsEnterpriseTriggered()) {
    result = pre_scan_download_check_result_;
  }

  for (auto& observer : observers_) {
    observer.OnFinish(this);
  }

  AcknowledgeRequest(event_result);

  // For obfuscated download files, deobfuscate it if the scan returns a safe
  // verdict.
  enterprise_obfuscation::DownloadObfuscationData* obfuscation_data =
      static_cast<enterprise_obfuscation::DownloadObfuscationData*>(
          item_->GetUserData(
              enterprise_obfuscation::DownloadObfuscationData::kUserDataKey));

  // Bypassed verdicts are given when a user continues a download after being
  // warned by WP, so it is considered safe here.
  if ((event_result == EventResult::ALLOWED ||
       event_result == EventResult::BYPASSED) &&
      obfuscation_data && obfuscation_data->is_obfuscated) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&enterprise_obfuscation::DeobfuscateFileInPlace,
                       item_->GetFullPath()),
        base::BindOnce(&DeepScanningRequest::OnDeobfuscationComplete,
                       weak_ptr_factory_.GetWeakPtr(), result));
    return;
  }

  CallbackAndCleanup(result);
}

void DeepScanningRequest::OnDeobfuscationComplete(
    DownloadCheckResult result,
    base::expected<void, enterprise_obfuscation::Error> deobfuscation_result) {
  if (!deobfuscation_result.has_value()) {
    // TODO(b/367259664): Add better error handling for deobfuscation.
    DVLOG(1) << "Failed to deobfuscate downloaded file.";
  }

  CallbackAndCleanup(result);
}

void DeepScanningRequest::CallbackAndCleanup(DownloadCheckResult result) {
  if (!callback_.is_null()) {
    callback_.Run(result);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  item_->RemoveObserver(this);
  download_service_->RequestFinished(this);
}

bool DeepScanningRequest::ReportOnlyScan() {
  if (IsConsumerTriggered()) {
    return false;
  }

  return analysis_settings_.block_until_verdict ==
         enterprise_connectors::BlockUntilVerdict::kNoBlock;
}

void DeepScanningRequest::AcknowledgeRequest(EventResult event_result) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  BinaryUploadService* binary_upload_service =
      download_service_->GetBinaryUploadService(profile, analysis_settings_);
  if (!binary_upload_service) {
    return;
  }

  // Calculate final action applied to all requests.
  auto final_action = GetFinalAction(event_result);

  for (auto& token : request_tokens_) {
    auto ack = std::make_unique<BinaryUploadService::Ack>(
        analysis_settings_.cloud_or_local_settings);
    ack->set_request_token(token);
    ack->set_status(
        enterprise_connectors::ContentAnalysisAcknowledgement::SUCCESS);
    ack->set_final_action(final_action);
    binary_upload_service->MaybeAcknowledge(std::move(ack));
  }
}

bool DeepScanningRequest::IsConsumerTriggered() const {
  switch (trigger_) {
    case DeepScanTrigger::TRIGGER_UNKNOWN:
    case DeepScanTrigger::TRIGGER_POLICY:
      return false;
    case DeepScanTrigger::TRIGGER_CONSUMER_PROMPT:
    case DeepScanTrigger::TRIGGER_ENCRYPTED_CONSUMER_PROMPT:
    case DeepScanTrigger::TRIGGER_IMMEDIATE_DEEP_SCAN:
      return true;
  }
}

bool DeepScanningRequest::IsEnterpriseTriggered() const {
  switch (trigger_) {
    case DeepScanTrigger::TRIGGER_UNKNOWN:
    case DeepScanTrigger::TRIGGER_CONSUMER_PROMPT:
    case DeepScanTrigger::TRIGGER_ENCRYPTED_CONSUMER_PROMPT:
    case DeepScanTrigger::TRIGGER_IMMEDIATE_DEEP_SCAN:
      return false;
    case DeepScanTrigger::TRIGGER_POLICY:
      return true;
  }
}

bool DeepScanningRequest::MaybeShowDeepScanFailureModalDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure close_callback,
    base::OnceClosure open_now_callback) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (!profile) {
    return false;
  }

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser) {
    return false;
  }

  DeepScanningFailureModalDialog::ShowForWebContents(
      browser->tab_strip_model()->GetActiveWebContents(),
      std::move(accept_callback), std::move(cancel_callback),
      std::move(close_callback), std::move(open_now_callback));
  return true;
}

bool DeepScanningRequest::ShouldTerminateEarly(
    BinaryUploadService::Result result) {
  CHECK(analysis_settings_.cloud_or_local_settings.is_cloud_analysis());

  return IsEnterpriseTriggered()
             ? EnterpriseResultIsFailure(
                   result, analysis_settings_.block_large_files,
                   analysis_settings_.block_password_protected_files)
             : result != BinaryUploadService::Result::SUCCESS;
}

void DeepScanningRequest::OpenDownload() {
  item_->OpenDownload();
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

}  // namespace safe_browsing
