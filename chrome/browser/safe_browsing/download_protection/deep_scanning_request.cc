// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

namespace {

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
    *download_result = DownloadCheckResult::UNKNOWN;
    return;
  }

  *download_result = DownloadCheckResult::DEEP_SCANNED_SAFE;
}

EventResult GetEventResult(DownloadCheckResult download_result,
                           Profile* profile) {
  auto download_restriction = static_cast<DownloadPrefs::DownloadRestriction>(
      profile->GetPrefs()->GetInteger(prefs::kDownloadRestrictions));
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
    case DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE:
      return EventResult::BLOCKED;

    default:
      NOTREACHED() << "Should never be final result";
      break;
  }
  return EventResult::UNKNOWN;
}

std::string GetTriggerName(DeepScanningRequest::DeepScanTrigger trigger) {
  switch (trigger) {
    case DeepScanningRequest::DeepScanTrigger::TRIGGER_UNKNOWN:
      return "Unknown";
    case DeepScanningRequest::DeepScanTrigger::TRIGGER_APP_PROMPT:
      return "AdvancedProtectionPrompt";
    case DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY:
      return "Policy";
  }
}

}  // namespace

/* static */
base::Optional<enterprise_connectors::AnalysisSettings>
DeepScanningRequest::ShouldUploadBinary(download::DownloadItem* item) {
  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));

  // If the download Connector is not enabled, don't scan.
  if (!service ||
      !service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED)) {
    return base::nullopt;
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
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* download_service,
    enterprise_connectors::AnalysisSettings settings)
    : item_(item),
      trigger_(trigger),
      callback_(callback),
      download_service_(download_service),
      analysis_settings_(std::move(settings)),
      weak_ptr_factory_(this) {
  item_->AddObserver(this);
}

DeepScanningRequest::~DeepScanningRequest() {
  item_->RemoveObserver(this);
}

void DeepScanningRequest::Start() {
  // Indicate we're now scanning the file.
  callback_.Run(DownloadCheckResult::ASYNC_SCANNING);

  auto request = std::make_unique<FileAnalysisRequest>(
      analysis_settings_, item_->GetFullPath(),
      item_->GetTargetFilePath().BaseName(),
      base::BindOnce(&DeepScanningRequest::OnScanComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  request->set_filename(item_->GetTargetFilePath().BaseName().AsUTF8Unsafe());

  std::string raw_digest_sha256 = item_->GetHash();
  request->set_digest(
      base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()));

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));

  base::UmaHistogramEnumeration("SBClientDownload.DeepScanTrigger", trigger_);

  PrepareRequest(std::move(request), profile);
}

void DeepScanningRequest::PrepareRequest(
    std::unique_ptr<FileAnalysisRequest> request,
    Profile* profile) {
  if (trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    request->set_device_token(analysis_settings_.dm_token);
    request->set_per_profile_request(analysis_settings_.per_profile);
    if (analysis_settings_.client_metadata)
      request->set_client_metadata(*analysis_settings_.client_metadata);
  }

  request->set_analysis_connector(enterprise_connectors::FILE_DOWNLOADED);
  request->set_email(GetProfileEmail(profile));

  if (item_->GetURL().is_valid())
    request->set_url(item_->GetURL().spec());

  if (item_->GetTabUrl().is_valid())
    request->set_tab_url(item_->GetTabUrl());

  for (const std::string& tag : analysis_settings_.tags)
    request->add_tag(tag);

  if (base::FeatureList::IsEnabled(kSafeBrowsingEnterpriseCsd) &&
      trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    download_request_maker_ = DownloadRequestMaker::CreateFromDownloadItem(
        new BinaryFeatureExtractor(), item_);
    download_request_maker_->Start(
        base::BindOnce(&DeepScanningRequest::OnDownloadRequestReady,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request)));
  } else {
    OnDownloadRequestReady(std::move(request), nullptr);
  }
}

void DeepScanningRequest::OnDownloadRequestReady(
    std::unique_ptr<FileAnalysisRequest> deep_scan_request,
    std::unique_ptr<ClientDownloadRequest> download_request) {
  if (download_request) {
    deep_scan_request->set_csd(*download_request);
  }

  upload_start_time_ = base::TimeTicks::Now();
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  BinaryUploadService* binary_upload_service =
      download_service_->GetBinaryUploadService(profile);
  if (binary_upload_service) {
    binary_upload_service->MaybeUploadForDeepScanning(
        std::move(deep_scan_request));
  } else {
    OnScanComplete(BinaryUploadService::Result::UNKNOWN,
                   enterprise_connectors::ContentAnalysisResponse());
  }
}

void DeepScanningRequest::OnScanComplete(
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordDeepScanMetrics(
      /*access_point=*/DeepScanAccessPoint::DOWNLOAD,
      /*duration=*/base::TimeTicks::Now() - upload_start_time_,
      /*total_size=*/item_->GetTotalBytes(), /*result=*/result,
      /*response=*/response);
  DownloadCheckResult download_result = DownloadCheckResult::UNKNOWN;
  if (result == BinaryUploadService::Result::SUCCESS) {
    ResponseToDownloadCheckResult(response, &download_result);
    base::UmaHistogramEnumeration(
        "SBClientDownload.MalwareDeepScanResult." + GetTriggerName(trigger_),
        download_result);
  } else if (trigger_ == DeepScanTrigger::TRIGGER_APP_PROMPT &&
             MaybeShowDeepScanFailureModalDialog(
                 base::BindOnce(&DeepScanningRequest::Start,
                                weak_ptr_factory_.GetWeakPtr()),
                 base::BindOnce(&DeepScanningRequest::FinishRequest,
                                weak_ptr_factory_.GetWeakPtr(),
                                DownloadCheckResult::UNKNOWN),
                 base::BindOnce(&DeepScanningRequest::OpenDownload,
                                weak_ptr_factory_.GetWeakPtr()))) {
    return;
  } else if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
    if (analysis_settings_.block_large_files)
      download_result = DownloadCheckResult::BLOCKED_TOO_LARGE;
  } else if (result == BinaryUploadService::Result::FILE_ENCRYPTED) {
    if (analysis_settings_.block_password_protected_files)
      download_result = DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED;
  } else if (result ==
             BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE) {
    if (analysis_settings_.block_unsupported_file_types)
      download_result = DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE;
  }

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (profile && trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    std::string raw_digest_sha256 = item_->GetHash();
    MaybeReportDeepScanningVerdict(
        profile, item_->GetURL(), item_->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        item_->GetMimeType(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
        DeepScanAccessPoint::DOWNLOAD, item_->GetTotalBytes(), result, response,
        GetEventResult(download_result, profile));

    item_->SetUserData(
        enterprise_connectors::ScanResult::kKey,
        std::make_unique<enterprise_connectors::ScanResult>(response));
  }

  FinishRequest(download_result);
}

void DeepScanningRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

void DeepScanningRequest::FinishRequest(DownloadCheckResult result) {
  callback_.Run(result);
  weak_ptr_factory_.InvalidateWeakPtrs();
  item_->RemoveObserver(this);
  download_service_->RequestFinished(this);
}

bool DeepScanningRequest::MaybeShowDeepScanFailureModalDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure open_now_callback) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (!profile)
    return false;

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser)
    return false;

  DeepScanningFailureModalDialog::ShowForWebContents(
      browser->tab_strip_model()->GetActiveWebContents(),
      std::move(accept_callback), std::move(cancel_callback),
      std::move(open_now_callback));
  return true;
}

void DeepScanningRequest::OpenDownload() {
  item_->OpenDownload();
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

}  // namespace safe_browsing
