// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_file_system_access_write_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_item_metadata.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "chrome/browser/safe_browsing/download_protection/download_url_sb_client.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/url_constants.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "net/cert/x509_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#endif

using content::BrowserThread;
using ReportThreatDetailsResult =
    safe_browsing::PingManager::ReportThreatDetailsResult;

namespace safe_browsing {

namespace {

constexpr int kDownloadAttributionUserGestureLimit = 2;
constexpr int kDownloadAttributionUserGestureLimitForExtendedReporting = 5;

constexpr base::TimeDelta kDownloadRequestTimeoutMs = base::Milliseconds(7000);

bool IsDownloadSecuritySensitive(safe_browsing::DownloadCheckResult result) {
  using Result = safe_browsing::DownloadCheckResult;
  switch (result) {
    case Result::UNKNOWN:
    case Result::DANGEROUS:
    case Result::UNCOMMON:
    case Result::DANGEROUS_HOST:
    case Result::POTENTIALLY_UNWANTED:
    case Result::DANGEROUS_ACCOUNT_COMPROMISE:
    case Result::DEEP_SCANNED_FAILED:
      return true;
    case Result::SAFE:
    case Result::ALLOWLISTED_BY_POLICY:
    case Result::ASYNC_SCANNING:
    case Result::ASYNC_LOCAL_PASSWORD_SCANNING:
    case Result::BLOCKED_PASSWORD_PROTECTED:
    case Result::BLOCKED_TOO_LARGE:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
    case Result::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case Result::BLOCKED_SCAN_FAILED:
    case Result::IMMEDIATE_DEEP_SCAN:
    case Result::FORCE_SAVE_TO_GDRIVE:
      return false;
  }
  NOTREACHED();
}

void MaybeLogSecuritySensitiveDownloadEvent(
    content::BrowserContext* browser_context,
    DownloadCheckResult result) {
  if (browser_context) {
    SafeBrowsingMetricsCollector* metrics_collector =
        SafeBrowsingMetricsCollectorFactory::GetForProfile(
            Profile::FromBrowserContext(browser_context));
    if (metrics_collector && IsDownloadSecuritySensitive(result)) {
      metrics_collector->AddSafeBrowsingEventToPref(
          safe_browsing::SafeBrowsingMetricsCollector::EventType::
              SECURITY_SENSITIVE_DOWNLOAD);
    }
  }
}

}  // namespace

const void* const DownloadProtectionService::kDownloadProtectionDataKey =
    &kDownloadProtectionDataKey;

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingServiceImpl* sb_service,
    std::unique_ptr<DownloadProtectionDelegate> delegate)
    : sb_service_(sb_service),
      delegate_(std::move(delegate)),
      binary_feature_extractor_(base::MakeRefCounted<BinaryFeatureExtractor>()),
#if !BUILDFLAG(IS_ANDROID)
      feedback_service_(std::make_unique<DownloadFeedbackService>(
          this,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
              .get())),
#endif
      weak_ptr_factory_(this) {
  CHECK(delegate_);
  if (sb_service) {
    ui_manager_ = sb_service->ui_manager();
    database_manager_ = sb_service->database_manager();
    ParseManualBlocklistFlag();
  }
}

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingServiceImpl* sb_service)
    : DownloadProtectionService(
          sb_service,
          DownloadProtectionDelegate::CreateForPlatform()) {}

DownloadProtectionService::~DownloadProtectionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CancelPendingRequests();
}

void DownloadProtectionService::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (enabled == enabled_) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_) {
    CancelPendingRequests();
  }
}

void DownloadProtectionService::ParseManualBlocklistFlag() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          safe_browsing::switches::kSbManualDownloadBlocklist))
    return;

  std::string flag_val = command_line->GetSwitchValueASCII(
      safe_browsing::switches::kSbManualDownloadBlocklist);
  for (const std::string& hash_hex : base::SplitString(
           flag_val, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::string bytes;
    if (base::HexStringToString(hash_hex, &bytes) && bytes.size() == 32) {
      manual_blocklist_hashes_.insert(std::move(bytes));
    } else {
      LOG(FATAL) << "Bad sha256 hex value '" << hash_hex << "' found in --"
                 << safe_browsing::switches::kSbManualDownloadBlocklist;
    }
  }
}

bool DownloadProtectionService::IsHashManuallyBlocklisted(
    const std::string& sha256_hash) const {
  return manual_blocklist_hashes_.count(sha256_hash) > 0;
}

void DownloadProtectionService::CheckClientDownload(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback,
    base::optional_ref<const std::string> password) {
  auto request = std::make_unique<CheckClientDownloadRequest>(
      item, std::move(callback), this, database_manager_,
      binary_feature_extractor_, password);
  CheckClientDownloadRequest* request_copy = request.get();
  context_download_requests_[content::DownloadItemUtils::GetBrowserContext(
      item)][request_copy] = std::move(request);
  request_copy->Start();
}

bool DownloadProtectionService::MaybeCheckClientDownload(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback) {
  auto settings = ShouldUploadBinaryForDeepScanning(item);

#if !BUILDFLAG(IS_ANDROID)
  bool report_only_scan =
      settings.has_value() &&
      settings.value().block_until_verdict ==
          enterprise_connectors::BlockUntilVerdict::kNoBlock;

  if (settings.has_value() && !report_only_scan) {
    // Since this branch implies that the CSD check is done through the deep
    // scanning request and not with a consumer check, the pre-deep scanning
    // DownloadCheckResult is considered UNKNOWN. This shouldn't trigger on
    // report-only scans to avoid skipping the consumer check.
    UploadForDeepScanning(
        std::make_unique<DownloadItemMetadata>(item),
        base::BindRepeating(
            &DownloadProtectionService::MaybeCheckMetadataAfterDeepScanning,
            weak_ptr_factory_.GetWeakPtr(), item, std::move(callback)),
        DownloadItemWarningData::DeepScanTrigger::TRIGGER_POLICY,
        DownloadCheckResult::UNKNOWN, std::move(settings.value()),
        /*password=*/std::nullopt);
    return true;
  }
#else
  CHECK(!settings.has_value());
#endif

  if (delegate_->MayCheckClientDownload(item)) {
    CheckClientDownload(item, std::move(callback), /*password=*/std::nullopt);
    return true;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (settings.has_value()) {
    DCHECK(report_only_scan);
    // Since this branch implies that CheckClientDownload was not called, the
    // pre-deep scanning DownloadCheckResult is considered UNKNOWN.
    UploadForDeepScanning(
        std::make_unique<DownloadItemMetadata>(item), std::move(callback),
        DownloadItemWarningData::DeepScanTrigger::TRIGGER_POLICY,
        DownloadCheckResult::UNKNOWN, std::move(settings.value()),
        /*password=*/std::nullopt);
    return true;
  }
#endif

  return false;
}

void DownloadProtectionService::CancelChecksForDownload(
    download::DownloadItem* item) {
  if (!item) {
    return;
  }

  content::BrowserContext* context =
      content::DownloadItemUtils::GetBrowserContext(item);
  for (auto it = context_download_requests_[context].begin();
       it != context_download_requests_[context].end(); ++it) {
    if (it->first->item() == item) {
      context_download_requests_[context].erase(it);
      break;
    }
  }
}

bool DownloadProtectionService::ShouldCheckDownloadUrl(
    download::DownloadItem* item) {
  return delegate_->ShouldCheckDownloadUrl(item);
}

void DownloadProtectionService::CheckDownloadUrl(
    download::DownloadItem* item,
    CheckDownloadCallback callback) {
  DCHECK(!item->GetUrlChain().empty());
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  // |web_contents| can be null in tests.
  // Checks if this download is allowlisted by enterprise policy.
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (profile &&
        MatchesEnterpriseAllowlist(*profile->GetPrefs(), item->GetUrlChain())) {
      // We don't return ALLOWLISTED_BY_POLICY yet, because future deep scanning
      // operations may indicate the file is unsafe.
      std::move(callback).Run(DownloadCheckResult::SAFE);
      return;
    }
  }

  auto client = base::MakeRefCounted<DownloadUrlSBClient>(
      item, this, std::move(callback), ui_manager_, database_manager_);
  // The client will release itself once it is done.
  client->StartCheck();
}

bool DownloadProtectionService::IsSupportedDownload(
    download::DownloadItem& item,
    const base::FilePath& target_path) const {
  return delegate_->IsSupportedDownload(item, target_path) !=
         MayCheckDownloadResult::kMayNotCheckDownload;
}

void DownloadProtectionService::CheckFileSystemAccessWrite(
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    CheckDownloadCallback callback) {
  if (!delegate_->MayCheckFileSystemAccessWrite(item.get())) {
    std::move(callback).Run(DownloadCheckResult::UNKNOWN);
    return;
  }
  content::BrowserContext* browser_context = item->browser_context;
  auto request = std::make_unique<CheckFileSystemAccessWriteRequest>(
      std::move(item), std::move(callback), this, database_manager_,
      binary_feature_extractor_);
  CheckClientDownloadRequestBase* request_copy = request.get();
  context_download_requests_[browser_context][request_copy] =
      std::move(request);
  request_copy->Start();
}

base::CallbackListSubscription
DownloadProtectionService::RegisterClientDownloadRequestCallback(
    const ClientDownloadRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return client_download_request_callbacks_.Add(callback);
}

base::CallbackListSubscription
DownloadProtectionService::RegisterFileSystemAccessWriteRequestCallback(
    const FileSystemAccessWriteRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return file_system_access_write_request_callbacks_.Add(callback);
}

void DownloadProtectionService::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // It is sufficient to delete the list of CheckClientDownloadRequests.
  context_download_requests_.clear();
}

void DownloadProtectionService::RequestFinished(
    CheckClientDownloadRequestBase* request,
    content::BrowserContext* browser_context,
    DownloadCheckResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MaybeLogSecuritySensitiveDownloadEvent(browser_context, result);
  DCHECK(context_download_requests_.contains(browser_context));
  DCHECK(context_download_requests_[browser_context].contains(request));
  context_download_requests_[browser_context].erase(request);
}

void DownloadProtectionService::ShowDetailsForDownload(
    const download::DownloadItem* item,
    content::PageNavigator* navigator) {
  GURL learn_more_url(chrome::kDownloadScanningLearnMoreURL);
  learn_more_url = google_util::AppendGoogleLocaleParam(
      learn_more_url, g_browser_process->GetApplicationLocale());
  learn_more_url = net::AppendQueryParameter(
      learn_more_url, "ctx",
      base::NumberToString(static_cast<int>(item->GetDangerType())));

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  if (profile &&
      AdvancedProtectionStatusManagerFactory::GetForProfile(profile)
          ->IsUnderAdvancedProtection() &&
      item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT) {
    learn_more_url = GURL(chrome::kAdvancedProtectionDownloadLearnMoreURL);
    learn_more_url = google_util::AppendGoogleLocaleParam(
        learn_more_url, g_browser_process->GetApplicationLocale());
  }

  navigator->OpenURL(
      content::OpenURLParams(learn_more_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

double DownloadProtectionService::allowlist_sample_rate() const {
  return allowlist_sample_rate_.value_or(
      delegate()->GetAllowlistedDownloadSampleRate());
}

// static
void DownloadProtectionService::SetDownloadProtectionData(
    download::DownloadItem* item,
    const std::string& token,
    const ClientDownloadResponse::Verdict& verdict,
    const ClientDownloadResponse::TailoredVerdict& tailored_verdict) {
  if (item) {
    item->SetUserData(kDownloadProtectionDataKey,
                      std::make_unique<DownloadProtectionData>(
                          token, verdict, tailored_verdict));
  }
}

// static
std::string DownloadProtectionService::GetDownloadPingToken(
    const download::DownloadItem* item) {
  base::SupportsUserData::Data* protection_data =
      item->GetUserData(kDownloadProtectionDataKey);
  if (protection_data)
    return static_cast<DownloadProtectionData*>(protection_data)
        ->token_string();
  else
    return std::string();
}

// static
bool DownloadProtectionService::HasDownloadProtectionVerdict(
    const download::DownloadItem* item) {
  return item->GetUserData(kDownloadProtectionDataKey) != nullptr;
}

// static
ClientDownloadResponse::Verdict
DownloadProtectionService::GetDownloadProtectionVerdict(
    const download::DownloadItem* item) {
  base::SupportsUserData::Data* protection_data =
      item->GetUserData(kDownloadProtectionDataKey);
  if (protection_data)
    return static_cast<DownloadProtectionData*>(protection_data)->verdict();
  else
    return ClientDownloadResponse::SAFE;
}

// static
ClientDownloadResponse::TailoredVerdict
DownloadProtectionService::GetDownloadProtectionTailoredVerdict(
    const download::DownloadItem* item) {
  base::SupportsUserData::Data* protection_data =
      item->GetUserData(kDownloadProtectionDataKey);
  if (protection_data)
    return static_cast<DownloadProtectionData*>(protection_data)
        ->tailored_verdict();
  else
    return ClientDownloadResponse::TailoredVerdict();
}

void DownloadProtectionService::MaybeSendDangerousDownloadOpenedReport(
    download::DownloadItem* item,
    bool show_download_in_folder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || !IsSafeBrowsingEnabled(*profile->GetPrefs()))
    return;

  // When users are in incognito mode, no report will be sent and no
  // |onDangerousDownloadOpened| extension API will be called.
  if (browser_context->IsOffTheRecord())
    return;

  // Only report downloads that are known to be dangerous or was dangerous but
  // was validated by the user.
  if (!item->IsDangerous() &&
      item->GetDangerType() != download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED)
    return;

  OnDangerousDownloadOpened(item, profile);

  if (sb_service_) {
    // If the download is opened, it indicates the user has bypassed the warning
    // and decided to proceed, so setting did_proceed to true.
    sb_service_->SendDownloadReport(
        item, ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED,
        /*did_proceed=*/true, show_download_in_folder);
  }
}

void DownloadProtectionService::ReportDelayedBypassEvent(
    download::DownloadItem* download,
    download::DownloadDangerType danger_type) {
  download_protection_observer_.ReportDelayedBypassEvent(download, danger_type);
}

void DownloadProtectionService::ReportSensitiveFileBypassEnterpriseEvent(
    download::DownloadItem* item,
    Profile* profile,
    const enterprise_connectors::FileMetadata& metadata,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const google::protobuf::RepeatedPtrField<ReferrerChainEntry>&
        referrer_chain,
    const google::protobuf::RepeatedPtrField<std::string>& frame_urls) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  auto* reporting_event_router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          profile);
  if (!reporting_event_router) {
    return;
  }
  enterprise_connectors::DownloadContentAreaUserProvider info(*item);

  reporting_event_router->OnSensitiveDataEvent(
      item->GetURL(), item->GetTabUrl(), /*source=*/"",
      /*destination=*/"", metadata.filename, metadata.sha256,
      metadata.mime_type,
      enterprise_connectors::kFileDownloadDataTransferEventTrigger,
      metadata.scan_response.request_token(),
      /*content_transfer_method=*/"", /*source_email=*/"",
      info.GetContentAreaAccountEmail(),
      /*user_justification=*/std::nullopt, result, metadata.size,
      referrer_chain, frame_urls, enterprise_connectors::EventResult::BYPASSED);
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void DownloadProtectionService::ReportDangerousDownloadOpenedEnterpriseEvent(
    download::DownloadItem* item,
    Profile* profile,
    const enterprise_connectors::FileMetadata& metadata,
    const google::protobuf::RepeatedPtrField<ReferrerChainEntry>&
        referrer_chain,
    const google::protobuf::RepeatedPtrField<std::string>& frame_urls) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  enterprise_connectors::ReportingEventRouter* reporting_event_router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          profile);
  if (!reporting_event_router) {
    return;
  }

  reporting_event_router->OnDangerousDownloadEvent(
      item->GetURL(), item->GetTabUrl(), metadata.filename, metadata.sha256,
      item->GetDangerType(), metadata.mime_type,
      enterprise_connectors::kFileDownloadDataTransferEventTrigger,
      metadata.scan_response.request_token(), metadata.size, referrer_chain,
      frame_urls, enterprise_connectors::EventResult::BYPASSED);
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void DownloadProtectionService::ReportDangerousDownloadOpenedEnterpriseEvent(
    download::DownloadItem* item,
    Profile* profile,
    const google::protobuf::RepeatedPtrField<ReferrerChainEntry>&
        referrer_chain,
    const google::protobuf::RepeatedPtrField<std::string>& frame_urls) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  enterprise_connectors::ReportingEventRouter* reporting_event_router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          profile);
  if (!reporting_event_router) {
    return;
  }

  reporting_event_router->OnDangerousDownloadEvent(
      item->GetURL(), item->GetTabUrl(),
      item->GetTargetFilePath().AsUTF8Unsafe(),
      base::HexEncode(item->GetHash()), item->GetDangerType(),
      item->GetMimeType(),
      enterprise_connectors::kFileDownloadDataTransferEventTrigger,
      /*scan_id*/ "", item->GetTotalBytes(), referrer_chain, frame_urls,
      enterprise_connectors::EventResult::BYPASSED);
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void DownloadProtectionService::ReportDangerousDownloadOpenedSafeBrowsingEvent(
    download::DownloadItem* item,
    Profile* profile,
    const enterprise_connectors::FileMetadata& metadata) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* safe_browsing_event_router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!safe_browsing_event_router) {
    return;
  }

  safe_browsing_event_router->OnDangerousDownloadOpened(
      item->GetURL(), item->GetTabUrl(), metadata.filename, metadata.sha256,
      metadata.mime_type, metadata.scan_response.request_token(),
      item->GetDangerType(), metadata.size);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void DownloadProtectionService::ReportDangerousDownloadOpenedSafeBrowsingEvent(
    download::DownloadItem* item,
    Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* safe_browsing_event_router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!safe_browsing_event_router) {
    return;
  }
  safe_browsing_event_router->OnDangerousDownloadOpened(
      item->GetURL(), item->GetTabUrl(),
      item->GetTargetFilePath().AsUTF8Unsafe(),
      base::HexEncode(item->GetHash()), item->GetMimeType(), /*scan_id*/ "",
      item->GetDangerType(), item->GetTotalBytes());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void DownloadProtectionService::OnDangerousDownloadOpened(
    download::DownloadItem* item,
    Profile* profile) {
  enterprise_connectors::ScanResult* scan_result =
      static_cast<enterprise_connectors::ScanResult*>(
          item->GetUserData(enterprise_connectors::ScanResult::kKey));

  google::protobuf::RepeatedPtrField<ReferrerChainEntry> referrer_chain;
  if (base::FeatureList::IsEnabled(kEnhancedFieldsForSecOps)) {
    referrer_chain = GetOrIdentifyReferrerChainForEnterprise(*item);
  }

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(content::DownloadItemUtils::GetWebContents(item),
                       enterprise_connectors::DeepScanAccessPoint::DOWNLOAD);

  // A download with a verdict of "sensitive data warning" can be opened and
  // |item->IsDangerous()| will return |true| for it but the reported event
  // should be a "sensitive file bypass" event rather than a "dangerous file
  // bypass" event.
  if (scan_result &&
      item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
    enterprise_connectors::DownloadContentAreaUserProvider info(*item);
    for (const auto& metadata : scan_result->file_metadata) {
      for (const auto& result : metadata.scan_response.results()) {
        if (result.tag() != "dlp") {
          continue;
        }

        ReportSensitiveFileBypassEnterpriseEvent(
            item, profile, metadata, result, referrer_chain, frame_urls);

        // There won't be multiple DLP verdicts in the same response, so no need
        // to keep iterating.
        break;
      }
    }
  } else if (scan_result) {
    for (const auto& metadata : scan_result->file_metadata) {
      ReportDangerousDownloadOpenedEnterpriseEvent(item, profile, metadata,
                                                   referrer_chain, frame_urls);
      ReportDangerousDownloadOpenedSafeBrowsingEvent(item, profile, metadata);
    }
  } else {
    ReportDangerousDownloadOpenedEnterpriseEvent(item, profile, referrer_chain,
                                                 frame_urls);
    ReportDangerousDownloadOpenedSafeBrowsingEvent(item, profile);
  }
}

const GURL& DownloadProtectionService::GetDownloadRequestUrl() const {
  return delegate_->GetDownloadRequestUrl();
}

base::TimeDelta DownloadProtectionService::GetDownloadRequestTimeout() const {
  return kDownloadRequestTimeoutMs;
}

bool DownloadProtectionService::MaybeBeginFeedbackForDownload(
    Profile* profile,
    download::DownloadItem* download,
    const std::string& ping_request,
    const std::string& ping_response) {
#if !BUILDFLAG(IS_ANDROID)
  PrefService* prefs = profile->GetPrefs();
  bool is_extended_reporting = IsExtendedReportingEnabled(*prefs);
  if (!profile->IsOffTheRecord() && is_extended_reporting) {
    feedback_service_->BeginFeedbackForDownload(profile, download, ping_request,
                                                ping_response);
    return true;
  }
#endif
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
void DownloadProtectionService::UploadForDeepScanning(
    std::unique_ptr<DeepScanningMetadata> metadata,
    CheckDownloadRepeatingCallback callback,
    DownloadItemWarningData::DeepScanTrigger trigger,
    DownloadCheckResult download_check_result,
    enterprise_connectors::AnalysisSettings analysis_settings,
    base::optional_ref<const std::string> password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context = metadata->GetBrowserContext();
  auto request = std::make_unique<DeepScanningRequest>(
      std::move(metadata), trigger, download_check_result, callback, this,
      std::move(analysis_settings), password);
  auto insertion_result = deep_scanning_requests_.insert(std::move(request));
  DCHECK(insertion_result.second);
  insertion_result.first->get()->Start();

  Profile* profile = Profile::FromBrowserContext(browser_context);
  SafeBrowsingMetricsCollector* metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile);
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            DOWNLOAD_DEEP_SCAN);
  }
}

// static
void DownloadProtectionService::UploadForConsumerDeepScanning(
    download::DownloadItem* item,
    DownloadItemWarningData::DeepScanTrigger trigger,
    base::optional_ref<const std::string> password) {
  if (!item) {
    return;
  }
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service) {
    return;
  }
  safe_browsing::DownloadProtectionService* protection_service =
      sb_service->download_protection_service();
  if (!protection_service) {
    return;
  }
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));
  DCHECK(download_core_service);
  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  DCHECK(delegate);

  // Create an analysis settings object for UploadForDeepScanning().
  // Make sure it specifies a cloud analysis is required and does not
  // specify a DM token, which is what triggers an APP scan.
  enterprise_connectors::AnalysisSettings settings;
  settings.cloud_or_local_settings =
      enterprise_connectors::CloudOrLocalAnalysisSettings(
          enterprise_connectors::CloudAnalysisSettings());
  settings.tags = {{"malware", enterprise_connectors::TagSettings()}};
  protection_service->UploadForDeepScanning(
      std::make_unique<DownloadItemMetadata>(item),
      base::BindRepeating(
          &ChromeDownloadManagerDelegate::CheckClientDownloadDone,
          delegate->GetWeakPtr(), item->GetId()),
      trigger, safe_browsing::DownloadCheckResult::UNKNOWN, std::move(settings),
      password);
}

// static
void DownloadProtectionService::CheckDownloadWithLocalDecryption(
    download::DownloadItem* item,
    base::optional_ref<const std::string> password) {
  if (!item) {
    return;
  }
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service) {
    return;
  }
  safe_browsing::DownloadProtectionService* protection_service =
      sb_service->download_protection_service();
  if (!protection_service) {
    return;
  }
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));
  DCHECK(download_core_service);
  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  DCHECK(delegate);

  DownloadItemWarningData::SetHasShownLocalDecryptionPrompt(item, true);

  delegate->CheckClientDownloadDone(
      item->GetId(),
      safe_browsing::DownloadCheckResult::ASYNC_LOCAL_PASSWORD_SCANNING);
  protection_service->CheckClientDownload(
      item,
      base::BindRepeating(
          &ChromeDownloadManagerDelegate::CheckClientDownloadDone,
          delegate->GetWeakPtr(), item->GetId()),
      password);
}

void DownloadProtectionService::UploadSavePackageForDeepScanning(
    download::DownloadItem* item,
    base::flat_map<base::FilePath, base::FilePath> save_package_files,
    CheckDownloadRepeatingCallback callback,
    enterprise_connectors::AnalysisSettings analysis_settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request = std::make_unique<DeepScanningRequest>(
      std::make_unique<DownloadItemMetadata>(item),
      DownloadCheckResult::UNKNOWN, callback, this,
      std::move(analysis_settings), std::move(save_package_files));
  auto insertion_result = deep_scanning_requests_.insert(std::move(request));
  DCHECK(insertion_result.second);
  insertion_result.first->get()->Start();
}

std::vector<DeepScanningRequest*>
DownloadProtectionService::GetDeepScanningRequests() {
  std::vector<DeepScanningRequest*> requests;
  for (const auto& request : deep_scanning_requests_) {
    requests.push_back(request.get());
  }
  return requests;
}
#endif  // !BUILDFLAG(IS_ANDROID)

scoped_refptr<network::SharedURLLoaderFactory>
DownloadProtectionService::GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  return sb_service_->GetURLLoaderFactory(
      Profile::FromBrowserContext(browser_context));
}

void DownloadProtectionService::RemovePendingDownloadRequests(
    content::BrowserContext* browser_context) {
  context_download_requests_.erase(browser_context);
}

// static
int DownloadProtectionService::GetDownloadAttributionUserGestureLimit(
    download::DownloadItem* item) {
  if (!item) {
    return kDownloadAttributionUserGestureLimit;
  }

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents) {
    return kDownloadAttributionUserGestureLimit;
  }

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      profile && profile->GetPrefs() &&
      IsExtendedReportingEnabled(*profile->GetPrefs())) {
    return kDownloadAttributionUserGestureLimitForExtendedReporting;
  }

  return kDownloadAttributionUserGestureLimit;
}

#if !BUILDFLAG(IS_ANDROID)
void DownloadProtectionService::RequestFinished(DeepScanningRequest* request) {
  auto it = std::ranges::find_if(deep_scanning_requests_,
                                 base::MatchesUniquePtr(request));
  CHECK(it != deep_scanning_requests_.end());
  deep_scanning_requests_.erase(it);
}

BinaryUploadService* DownloadProtectionService::GetBinaryUploadService(
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& settings) {
  return BinaryUploadService::GetForProfile(profile, settings);
}
#endif

void DownloadProtectionService::MaybeCheckMetadataAfterDeepScanning(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback,
    DownloadCheckResult result) {
  if (result == DownloadCheckResult::UNKNOWN) {
    CheckClientDownload(item, callback, /*password=*/std::nullopt);
  } else {
    std::move(callback).Run(result);
  }
}

}  // namespace safe_browsing
