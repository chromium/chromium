// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_file_system_access_write_request.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "chrome/browser/safe_browsing/download_protection/download_url_sb_client.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/url_constants.h"
#include "components/download/public/common/download_item.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/safe_browsing/core/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "net/cert/x509_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using content::BrowserThread;
namespace safe_browsing {

namespace {

const int64_t kDownloadRequestTimeoutMs = 7000;
// We sample 1% of allowlisted downloads to still send out download pings.
const double kAllowlistDownloadSampleRate = 0.01;

// The number of user gestures we trace back for download attribution.
const int kDownloadAttributionUserGestureLimit = 2;
const int kDownloadAttributionUserGestureLimitForExtendedReporting = 5;

void AddEventUrlToReferrerChain(const download::DownloadItem& item,
                                ReferrerChain* out_referrer_chain) {
  ReferrerChainEntry* event_url_entry = out_referrer_chain->Add();
  event_url_entry->set_url(item.GetURL().spec());
  event_url_entry->set_type(ReferrerChainEntry::EVENT_URL);
  event_url_entry->set_referrer_url(
      content::DownloadItemUtils::GetWebContents(
          const_cast<download::DownloadItem*>(&item))
          ->GetLastCommittedURL()
          .spec());
  event_url_entry->set_is_retargeting(false);
  event_url_entry->set_navigation_time_msec(base::Time::Now().ToJavaTime());
  for (const GURL& url : item.GetUrlChain())
    event_url_entry->add_server_redirect_chain()->set_url(url.spec());
}

int GetDownloadAttributionUserGestureLimit(const download::DownloadItem& item) {
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(
          const_cast<download::DownloadItem*>(&item));
  if (!web_contents)
    return kDownloadAttributionUserGestureLimit;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return kDownloadAttributionUserGestureLimit;

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return kDownloadAttributionUserGestureLimit;
  if (!IsExtendedReportingEnabled(*prefs))
    return kDownloadAttributionUserGestureLimit;
  return kDownloadAttributionUserGestureLimitForExtendedReporting;
}

}  // namespace

const void* const DownloadProtectionService::kDownloadPingTokenKey =
    &kDownloadPingTokenKey;

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingService* sb_service)
    : sb_service_(sb_service),
      navigation_observer_manager_(nullptr),
      enabled_(false),
      binary_feature_extractor_(new BinaryFeatureExtractor()),
      download_request_timeout_ms_(kDownloadRequestTimeoutMs),
      feedback_service_(new DownloadFeedbackService(
          this,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
              .get())),
      allowlist_sample_rate_(kAllowlistDownloadSampleRate),
      weak_ptr_factory_(this) {
  if (sb_service) {
    ui_manager_ = sb_service->ui_manager();
    database_manager_ = sb_service->database_manager();
    navigation_observer_manager_ = sb_service->navigation_observer_manager();
    ParseManualBlocklistFlag();
  }
}

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
    CheckDownloadRepeatingCallback callback) {
  ClientDownloadRequest::DownloadType file_download_type =
      download_type_util::GetDownloadType(item->GetTargetFilePath());
  if (file_download_type == ClientDownloadRequest::DOCUMENT) {
    UMA_HISTOGRAM_MEMORY_KB("SafeBrowsing.Macros.DocumentSize",
                            item->GetReceivedBytes() / 1024);
  }
  auto request = std::make_unique<CheckClientDownloadRequest>(
      item, std::move(callback), this, database_manager_,
      binary_feature_extractor_);
  CheckClientDownloadRequest* request_copy = request.get();
  download_requests_[request_copy] = std::move(request);
  request_copy->Start();
}

bool DownloadProtectionService::MaybeCheckClientDownload(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  bool safe_browsing_enabled =
      profile && IsSafeBrowsingEnabled(*profile->GetPrefs());
  auto settings = DeepScanningRequest::ShouldUploadBinary(item);

  if (base::FeatureList::IsEnabled(kSafeBrowsingEnterpriseCsd) &&
      base::FeatureList::IsEnabled(
          kSafeBrowsingDisableConsumerCsdForEnterprise) &&
      settings.has_value()) {
    UploadForDeepScanning(item, std::move(callback),
                          DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
                          std::move(settings.value()));
    return true;
  }

  if (safe_browsing_enabled) {
    CheckClientDownload(item, std::move(callback));
    return true;
  }

  // TODO(https://crbug.com/1165815): Remove this check once the enterpise CSD
  // check has fully launched.
  if (settings.has_value()) {
    UploadForDeepScanning(item, std::move(callback),
                          DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
                          std::move(settings.value()));
    return true;
  }

  return false;
}

bool DownloadProtectionService::ShouldCheckDownloadUrl(
    download::DownloadItem* item) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  return profile && IsSafeBrowsingEnabled(*profile->GetPrefs());
}

void DownloadProtectionService::CheckDownloadUrl(
    download::DownloadItem* item,
    CheckDownloadCallback callback) {
  DCHECK(!item->GetUrlChain().empty());
  DCHECK(ShouldCheckDownloadUrl(item));
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

  scoped_refptr<DownloadUrlSBClient> client(new DownloadUrlSBClient(
      item, this, std::move(callback), ui_manager_, database_manager_));
  // The client will release itself once it is done.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DownloadUrlSBClient::StartCheck, client));
}

bool DownloadProtectionService::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path) const {
  DownloadCheckResultReason reason = REASON_MAX;
  // TODO(nparker): Remove the CRX check here once can support
  // UNKNOWN types properly.  http://crbug.com/581044
  return (CheckClientDownloadRequest::IsSupportedDownload(item, target_path,
                                                          &reason) &&
          download_type_util::GetDownloadType(target_path) !=
              ClientDownloadRequest::CHROME_EXTENSION);
}

void DownloadProtectionService::CheckPPAPIDownloadRequest(
    const GURL& requestor_url,
    const GURL& initiating_frame_url,
    content::WebContents* web_contents,
    const base::FilePath& default_file_path,
    const std::vector<base::FilePath::StringType>& alternate_extensions,
    Profile* profile,
    CheckDownloadCallback callback) {
  DVLOG(1) << __func__ << " url:" << requestor_url
           << " default_file_path:" << default_file_path.value();
  if (profile &&
      MatchesEnterpriseAllowlist(*profile->GetPrefs(),
                                 {requestor_url, initiating_frame_url})) {
    std::move(callback).Run(DownloadCheckResult::ALLOWLISTED_BY_POLICY);
    return;
  }
  std::unique_ptr<PPAPIDownloadRequest> request(new PPAPIDownloadRequest(
      requestor_url, initiating_frame_url, web_contents, default_file_path,
      alternate_extensions, profile, std::move(callback), this,
      database_manager_));
  PPAPIDownloadRequest* request_copy = request.get();
  auto insertion_result = ppapi_download_requests_.insert(
      std::make_pair(request_copy, std::move(request)));
  DCHECK(insertion_result.second);
  insertion_result.first->second->Start();
}

void DownloadProtectionService::CheckFileSystemAccessWrite(
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    CheckDownloadCallback callback) {
  auto request = std::make_unique<CheckFileSystemAccessWriteRequest>(
      std::move(item), std::move(callback), this, database_manager_,
      binary_feature_extractor_);
  CheckClientDownloadRequestBase* request_copy = request.get();
  download_requests_[request_copy] = std::move(request);
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

base::CallbackListSubscription
DownloadProtectionService::RegisterPPAPIDownloadRequestCallback(
    const PPAPIDownloadRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ppapi_download_request_callbacks_.Add(callback);
}

void DownloadProtectionService::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // It is sufficient to delete the list of CheckClientDownloadRequests.
  download_requests_.clear();

  // It is sufficient to delete the list of PPAPI download requests.
  ppapi_download_requests_.clear();
}

void DownloadProtectionService::RequestFinished(
    CheckClientDownloadRequestBase* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = download_requests_.find(request);
  DCHECK(it != download_requests_.end());
  download_requests_.erase(it);
}

void DownloadProtectionService::PPAPIDownloadCheckRequestFinished(
    PPAPIDownloadRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = ppapi_download_requests_.find(request);
  DCHECK(it != ppapi_download_requests_.end());
  ppapi_download_requests_.erase(it);
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
                             ui::PAGE_TRANSITION_LINK, false));
}

void DownloadProtectionService::SetDownloadPingToken(
    download::DownloadItem* item,
    const std::string& token) {
  if (item) {
    item->SetUserData(kDownloadPingTokenKey,
                      std::make_unique<DownloadPingToken>(token));
  }
}

std::string DownloadProtectionService::GetDownloadPingToken(
    const download::DownloadItem* item) {
  base::SupportsUserData::Data* token_data =
      item->GetUserData(kDownloadPingTokenKey);
  if (token_data)
    return static_cast<DownloadPingToken*>(token_data)->token_string();
  else
    return std::string();
}

void DownloadProtectionService::MaybeSendDangerousDownloadOpenedReport(
    const download::DownloadItem* item,
    bool show_download_in_folder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string token = GetDownloadPingToken(item);
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || !IsSafeBrowsingEnabled(*profile->GetPrefs()))
    return;

  // When users are in incognito mode, no report will be sent and no
  // |onDangerousDownloadOpened| extension API will be called.
  if (browser_context->IsOffTheRecord())
    return;

  // Only report downloads that are known to be dangerous, or downloads that are
  // opened while scanning isn't done.
  if (!item->IsDangerous() &&
      item->GetDangerType() != download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
    return;
  }

  OnDangerousDownloadOpened(item, profile);
  if (sb_service_ &&
      !token.empty() &&  // Only dangerous downloads have token stored.
      profile && (IsExtendedReportingEnabled(*profile->GetPrefs()))) {
    safe_browsing::ClientSafeBrowsingReportRequest report;
    report.set_url(item->GetURL().spec());
    report.set_type(safe_browsing::ClientSafeBrowsingReportRequest::
                        DANGEROUS_DOWNLOAD_OPENED);
    report.set_token(token);
    report.set_show_download_in_folder(show_download_in_folder);
    std::string serialized_report;
    if (report.SerializeToString(&serialized_report)) {
      sb_service_->SendSerializedDownloadReport(profile, serialized_report);
    } else {
      DCHECK(false)
          << "Unable to serialize the dangerous download opened report.";
    }
  }
}

std::unique_ptr<ReferrerChainData>
DownloadProtectionService::IdentifyReferrerChain(
    const download::DownloadItem& item) {
  // If navigation_observer_manager_ is null, return immediately. This could
  // happen in tests.
  if (!navigation_observer_manager_)
    return nullptr;

  std::unique_ptr<ReferrerChain> referrer_chain =
      std::make_unique<ReferrerChain>();
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(
          const_cast<download::DownloadItem*>(&item));
  SessionID download_tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents);
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.ReferrerHasInvalidTabID.DownloadAttribution",
      !download_tab_id.is_valid());
  // We look for the referrer chain that leads to the download url first.
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByEventURL(
          item.GetURL(), download_tab_id,
          GetDownloadAttributionUserGestureLimit(item), referrer_chain.get());

  // If no navigation event is found, this download is not triggered by regular
  // navigation (e.g. html5 file apis, etc). We look for the referrer chain
  // based on relevant WebContents instead.
  if (result ==
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND &&
      web_contents && web_contents->GetLastCommittedURL().is_valid()) {
    AddEventUrlToReferrerChain(item, referrer_chain.get());
    result = navigation_observer_manager_->IdentifyReferrerChainByWebContents(
        web_contents, GetDownloadAttributionUserGestureLimit(item),
        referrer_chain.get());
  }

  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.DownloadAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  size_t referrer_chain_length = referrer_chain->size();

  // Determines how many recent navigation events to append to referrer chain
  // if any.
  size_t recent_navigations_to_collect =
      web_contents ? SafeBrowsingNavigationObserverManager::
                         CountOfRecentNavigationsToAppend(
                             *Profile::FromBrowserContext(
                                 web_contents->GetBrowserContext()),
                             result)
                   : 0u;
  navigation_observer_manager_->AppendRecentNavigations(
      recent_navigations_to_collect, referrer_chain.get());

  return std::make_unique<ReferrerChainData>(std::move(referrer_chain),
                                             referrer_chain_length,
                                             recent_navigations_to_collect);
}

std::unique_ptr<ReferrerChainData>
DownloadProtectionService::IdentifyReferrerChain(
    const content::FileSystemAccessWriteItem& item) {
  // If navigation_observer_manager_ is null, return immediately. This could
  // happen in tests.
  if (!navigation_observer_manager_)
    return nullptr;

  std::unique_ptr<ReferrerChain> referrer_chain =
      std::make_unique<ReferrerChain>();

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(item.web_contents);
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.ReferrerHasInvalidTabID.NativeFileSystemWriteAttribution",
      !tab_id.is_valid());

  GURL tab_url =
      item.web_contents ? item.web_contents->GetVisibleURL() : GURL();

  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByHostingPage(
          item.frame_url, tab_url, tab_id, item.has_user_gesture,
          kDownloadAttributionUserGestureLimit, referrer_chain.get());

  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.NativeFileSystemWriteAttribution",
      result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  size_t referrer_chain_length = referrer_chain->size();

  // Determines how many recent navigation events to append to referrer chain
  // if any.
  size_t recent_navigations_to_collect =
      item.browser_context
          ? SafeBrowsingNavigationObserverManager::
                CountOfRecentNavigationsToAppend(
                    *Profile::FromBrowserContext(item.browser_context), result)
          : 0u;
  navigation_observer_manager_->AppendRecentNavigations(
      recent_navigations_to_collect, referrer_chain.get());

  return std::make_unique<ReferrerChainData>(std::move(referrer_chain),
                                             referrer_chain_length,
                                             recent_navigations_to_collect);
}

void DownloadProtectionService::AddReferrerChainToPPAPIClientDownloadRequest(
    const GURL& initiating_frame_url,
    const GURL& initiating_main_frame_url,
    SessionID tab_id,
    bool has_user_gesture,
    ClientDownloadRequest* out_request) {
  if (!navigation_observer_manager_)
    return;

  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.ReferrerHasInvalidTabID.DownloadAttribution",
      !tab_id.is_valid());
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByHostingPage(
          initiating_frame_url, initiating_main_frame_url, tab_id,
          has_user_gesture, kDownloadAttributionUserGestureLimit,
          out_request->mutable_referrer_chain());
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.PPAPIDownloadAttribution",
      out_request->referrer_chain_size());
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.PPAPIDownloadAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);
}

void DownloadProtectionService::OnDangerousDownloadOpened(
    const download::DownloadItem* item,
    Profile* profile) {
  std::string raw_digest_sha256 = item->GetHash();
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (router) {
    router->OnDangerousDownloadOpened(
        item->GetURL(), item->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        item->GetMimeType(), item->GetDangerType(), item->GetTotalBytes());
  }
}

bool DownloadProtectionService::MaybeBeginFeedbackForDownload(
    Profile* profile,
    download::DownloadItem* download,
    DownloadCommands::Command download_command) {
  PrefService* prefs = profile->GetPrefs();
  bool is_extended_reporting =
      ExtendedReportingPrefExists(*prefs) && IsExtendedReportingEnabled(*prefs);
  if (!profile->IsOffTheRecord() && is_extended_reporting) {
    feedback_service_->BeginFeedbackForDownload(profile, download,
                                                download_command);
    return true;
  }
  return false;
}

void DownloadProtectionService::UploadForDeepScanning(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback,
    DeepScanningRequest::DeepScanTrigger trigger,
    enterprise_connectors::AnalysisSettings analysis_settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request = std::make_unique<DeepScanningRequest>(
      item, trigger, callback, this, std::move(analysis_settings));
  DeepScanningRequest* request_raw = request.get();
  auto insertion_result = deep_scanning_requests_.insert(
      std::make_pair(request_raw, std::move(request)));
  DCHECK(insertion_result.second);
  insertion_result.first->second->Start();
}

scoped_refptr<network::SharedURLLoaderFactory>
DownloadProtectionService::GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  return sb_service_->GetURLLoaderFactory(
      Profile::FromBrowserContext(browser_context));
}

void DownloadProtectionService::RequestFinished(DeepScanningRequest* request) {
  auto it = deep_scanning_requests_.find(request);
  DCHECK(it != deep_scanning_requests_.end());
  deep_scanning_requests_.erase(it);
}

BinaryUploadService* DownloadProtectionService::GetBinaryUploadService(
    Profile* profile) {
  return BinaryUploadServiceFactory::GetForProfile(profile);
}

}  // namespace safe_browsing
