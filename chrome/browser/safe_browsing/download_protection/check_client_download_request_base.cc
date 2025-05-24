// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"

#include <algorithm>

#include "base/cancelable_callback.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#endif

namespace safe_browsing {

using content::BrowserThread;

namespace {

const char kDownloadExtensionUmaName[] = "SBClientDownload.DownloadExtensions";

void RecordFileExtensionType(const std::string& metric_name,
                             const base::FilePath& file) {
  base::UmaHistogramSparse(
      metric_name, FileTypePolicies::GetInstance()->UmaValueForFile(file));
}

std::string SanitizeUrl(const std::string& url) {
  return GURL(url).DeprecatedGetOriginAsURL().spec();
}

}  // namespace

CheckClientDownloadRequestBase::CheckClientDownloadRequestBase(
    GURL source_url,
    base::FilePath target_file_path,
    content::BrowserContext* browser_context,
    CheckDownloadCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    std::unique_ptr<DownloadRequestMaker> download_request_maker)
    : source_url_(std::move(source_url)),
      target_file_path_(std::move(target_file_path)),
      callback_(std::move(callback)),
      service_(service),
      database_manager_(std::move(database_manager)),
      pingback_enabled_(service_->enabled()),
      download_request_maker_(std::move(download_request_maker)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    is_extended_reporting_ =
        profile && IsExtendedReportingEnabled(*profile->GetPrefs());
    is_incognito_ = browser_context->IsOffTheRecord();
    is_enhanced_protection_ =
        profile && IsEnhancedProtectionEnabled(*profile->GetPrefs());
#if !BUILDFLAG(IS_ANDROID)
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    if (!profile->IsOffTheRecord() && identity_manager &&
        safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(identity_manager)) {
      token_fetcher_ = std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          identity_manager);
    }
#endif
  }
}

CheckClientDownloadRequestBase::~CheckClientDownloadRequestBase() = default;

void CheckClientDownloadRequestBase::Start() {
  DVLOG(2) << "Starting SafeBrowsing download check for: " << source_url_;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CheckClientDownloadRequestBase::StartTimeout,
                                GetWeakPtr()));

  if (IsAllowlistedByPolicy()) {
    FinishRequest(DownloadCheckResult::ALLOWLISTED_BY_POLICY,
                  REASON_ALLOWLISTED_URL);
    return;
  }

  if (!database_manager_ || !source_url_.is_valid()) {
    OnUrlAllowlistCheckDone(false);
    return;
  }

  // If allowlist check passes, FinishRequest() will be called to avoid
  // analyzing file. Otherwise, AnalyzeFile() will be called to continue with
  // analysis.
  auto callback = base::BindOnce(
      &CheckClientDownloadRequestBase::OnUrlAllowlistCheckDone, GetWeakPtr());
  database_manager_->MatchDownloadAllowlistUrl(source_url_,
                                               std::move(callback));
}

void CheckClientDownloadRequestBase::FinishRequest(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto settings = ShouldUploadBinary(reason);
  if (settings.has_value()) {
    UploadBinary(result, reason, std::move(settings.value()));
  } else {
    // Post a task to avoid reentrance issue. http://crbug.com//1152451.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), result));
  }

  base::UmaHistogramEnumeration("SBClientDownload.CheckDownloadStats", reason,
                                REASON_MAX);

  NotifyRequestFinished(result, reason);
  service()->RequestFinished(this, GetBrowserContext(), result);
  // DownloadProtectionService::RequestFinished may delete us.
}

bool CheckClientDownloadRequestBase::ShouldSampleAllowlistedDownload() {
  // We currently sample 1% allowlisted downloads from users who opted
  // in extended reporting and are not in incognito mode.
  return service_ && is_extended_reporting_ && !is_incognito_ &&
         base::RandDouble() < service_->allowlist_sample_rate();
}

bool CheckClientDownloadRequestBase::ShouldSampleUnsupportedFile(
    const base::FilePath& filename) {
  // If this extension is specifically marked as SAMPLED_PING (as are
  // all "unknown" extensions), we may want to sample it. Sampling it means
  // we'll send a "light ping" with private info removed, and we won't
  // use the verdict.
  return service_ && is_extended_reporting_ && !is_incognito_ &&
         base::RandDouble() <
             service_->delegate()->GetUnsupportedFileSampleRate(filename);
}

// If the hash of either the original file or any executables within an
// archive matches the blocklist flag, return true.
bool CheckClientDownloadRequestBase::IsDownloadManuallyBlocklisted(
    const ClientDownloadRequest& request) {
  if (service_->IsHashManuallyBlocklisted(request.digests().sha256()))
    return true;

  for (const auto& bin_itr : request.archived_binary()) {
    if (service_->IsHashManuallyBlocklisted(bin_itr.digests().sha256()))
      return true;
  }
  return false;
}

void CheckClientDownloadRequestBase::OnUrlAllowlistCheckDone(
    bool is_allowlisted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the download URL is allowlisted on the allowlist, file feature
  // extraction and download ping are skipped.
  if (is_allowlisted) {
    DVLOG(2) << source_url_ << " is on the download allowlist.";
    if (ShouldSampleAllowlistedDownload()) {
      skipped_url_allowlist_ = true;
    } else {
      // TODO(grt): Continue processing without uploading so that
      // ClientDownloadRequest callbacks can be run even for this type of safe
      // download.
      FinishRequest(DownloadCheckResult::SAFE, REASON_ALLOWLISTED_URL);
      return;
    }
  }

  DownloadCheckResultReason reason = REASON_MAX;
  MayCheckDownloadResult may_check_download_result =
      IsSupportedDownload(&reason);

  if (may_check_download_result ==
      MayCheckDownloadResult::kMayNotCheckDownload) {
    CHECK(reason == REASON_EMPTY_URL_CHAIN || reason == REASON_INVALID_URL ||
          reason == REASON_LOCAL_FILE || reason == REASON_REMOTE_FILE ||
          reason == REASON_UNSUPPORTED_URL_SCHEME ||
          reason == REASON_DOWNLOAD_DESTROYED);
    FinishRequest(DownloadCheckResult::UNKNOWN, reason);
    return;
  }

  RecordFileExtensionType(kDownloadExtensionUmaName, target_file_path_);

  if (may_check_download_result ==
      MayCheckDownloadResult::kMaySendSampledPingOnly) {
    CHECK(reason == REASON_NOT_BINARY_FILE);
    // Send a "light ping" and don't use the verdict.
    sampled_unsupported_file_ = ShouldSampleUnsupportedFile(target_file_path_);
    if (!sampled_unsupported_file_) {
      FinishRequest(DownloadCheckResult::UNKNOWN, reason);
      return;
    }
  }

  CHECK(may_check_download_result ==
            MayCheckDownloadResult::kMayCheckDownload ||
        sampled_unsupported_file_);
  download_request_maker_->Start(base::BindOnce(
      &CheckClientDownloadRequestBase::OnRequestBuilt, GetWeakPtr()));
}

void CheckClientDownloadRequestBase::SanitizeRequest() {
  if (!sampled_unsupported_file_)
    return;

  client_download_request_->set_download_type(
      ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE);
  if (client_download_request_->referrer_chain_size() > 0) {
    SafeBrowsingNavigationObserverManager::SanitizeReferrerChain(
        client_download_request_->mutable_referrer_chain());
  }

  client_download_request_->set_url(
      SanitizeUrl(client_download_request_->url()));
  for (ClientDownloadRequest::Resource& resource :
       *client_download_request_->mutable_resources()) {
    resource.set_url(SanitizeUrl(resource.url()));
    resource.set_referrer(SanitizeUrl(resource.referrer()));
  }
}

void CheckClientDownloadRequestBase::GetAdditionalPromptResult(
    const ClientDownloadResponse& response,
    DownloadCheckResult* result,
    DownloadCheckResultReason* reason,
    std::string* token) const {
  bool local_decryption_prompt = ShouldPromptForLocalDecryption(
      response.is_suspicious_encrypted_archive());
  if (local_decryption_prompt) {
    LogLocalDecryptionEvent(safe_browsing::DeepScanEvent::kPromptShown);

    *result = DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING;
    *reason = DownloadCheckResultReason::REASON_LOCAL_DECRYPTION_PROMPT;
    *token = response.token();
  }

  if (ShouldPromptForLocalDecryption(/*server_requests_prompt=*/true)) {
    base::UmaHistogramBoolean(
        "SBClientDownload.ServerRequestsLocalDecryptionPrompt",
        local_decryption_prompt);
  }

  bool deep_scanning_prompt =
      ShouldPromptForDeepScanning(response.request_deep_scan());
  if (deep_scanning_prompt) {
    *result = DownloadCheckResult::PROMPT_FOR_SCANNING;
    *reason = DownloadCheckResultReason::REASON_DEEP_SCAN_PROMPT;
    // Always set the token if Chrome should prompt for deep scanning.
    // Otherwise, client Safe Browsing reports may be missed when the
    // verdict is SAFE. See https://crbug.com/1485218.
    *token = response.token();
  }

  // Only record the UMA metric if we're in a population that potentially
  // could prompt for deep scanning.
  if (ShouldPromptForDeepScanning(/*server_requests_prompt=*/true)) {
    LogDeepScanningPrompt(deep_scanning_prompt);
  }

  bool immediate_deep_scan_prompt =
      ShouldImmediatelyDeepScan(response.request_deep_scan());
  if (immediate_deep_scan_prompt) {
    *result = DownloadCheckResult::IMMEDIATE_DEEP_SCAN;
    *reason = DownloadCheckResultReason::REASON_IMMEDIATE_DEEP_SCAN;
    // Always set the token if Chrome should prompt for deep scanning.
    // Otherwise, client Safe Browsing reports may be missed when the
    // verdict is SAFE. See https://crbug.com/1485218.
    *token = response.token();
  }

  // Only record the UMA metric if we're in a population that potentially
  // could prompt for deep scanning.
  if (ShouldImmediatelyDeepScan(/*server_requests_prompt=*/true)) {
    base::UmaHistogramBoolean(
        "SBClientDownload.ServerRequestsImmediateDeepScan2",
        immediate_deep_scan_prompt);
  }
}

void CheckClientDownloadRequestBase::OnRequestBuilt(
    DownloadRequestMaker::RequestCreationDetails details,
    std::unique_ptr<ClientDownloadRequest> request) {
  if (ShouldPromptForIncorrectPassword()) {
    LogLocalDecryptionEvent(safe_browsing::DeepScanEvent::kIncorrectPassword);
    FinishRequest(DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING,
                  REASON_LOCAL_DECRYPTION_PROMPT);
    return;
  }

  if (ShouldShowScanFailure()) {
    FinishRequest(DownloadCheckResult::DEEP_SCANNED_FAILED,
                  REASON_LOCAL_DECRYPTION_FAILED);
    return;
  }

  client_download_request_ = std::move(request);
  request_creation_details_ = details;
  SanitizeRequest();

  // If it's an archive with no archives or executables, finish early.
  if ((client_download_request_->download_type() ==
           ClientDownloadRequest::ZIPPED_EXECUTABLE ||
       client_download_request_->download_type() ==
           ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE ||
       client_download_request_->download_type() ==
           ClientDownloadRequest::SEVEN_ZIP_COMPRESSED_EXECUTABLE) &&
      client_download_request_->archive_summary().parser_status() ==
          ClientDownloadRequest::ArchiveSummary::VALID &&
      std::ranges::all_of(
          client_download_request_->archived_binary(),
          [](const ClientDownloadRequest::ArchivedBinary& archived_binary) {
            return !archived_binary.is_executable() &&
                   !archived_binary.is_archive();
          })) {
    FinishRequest(DownloadCheckResult::UNKNOWN,
                  REASON_ARCHIVE_WITHOUT_BINARIES);
    return;
  }

  if (!pingback_enabled_) {
    FinishRequest(DownloadCheckResult::UNKNOWN, REASON_PING_DISABLED);
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (is_enhanced_protection_ && token_fetcher_) {
    token_fetcher_->Start(base::BindOnce(
        &CheckClientDownloadRequestBase::OnGotAccessToken, GetWeakPtr()));
    return;
  }
#endif

  SendRequest();
}

// Start a timeout to cancel the request if it takes too long.
// This should only be called after we have finished accessing the file.
void CheckClientDownloadRequestBase::StartTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  timeout_start_time_ = base::TimeTicks::Now();
  timeout_closure_.Reset(base::BindOnce(
      &CheckClientDownloadRequestBase::FinishRequest, GetWeakPtr(),
      DownloadCheckResult::UNKNOWN, REASON_REQUEST_CANCELED));
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, timeout_closure_.callback(),
      service_->GetDownloadRequestTimeout());
}

#if !BUILDFLAG(IS_ANDROID)
void CheckClientDownloadRequestBase::OnGotAccessToken(
    const std::string& access_token) {
  access_token_ = access_token;
  SendRequest();
}
#endif

void CheckClientDownloadRequestBase::SendRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (IsCancelled()) {
    FinishRequest(DownloadCheckResult::UNKNOWN, REASON_DOWNLOAD_DESTROYED);
    return;
  }

  client_download_request_->set_skipped_url_allowlist(skipped_url_allowlist_);
  client_download_request_->set_skipped_certificate_allowlist(
      skipped_certificate_allowlist_);

  CHECK(service_);

  service_->delegate()->PreSerializeRequest(item(), *client_download_request_);

  if (!client_download_request_->SerializeToString(
          &client_download_request_data_)) {
    FinishRequest(DownloadCheckResult::UNKNOWN, REASON_INVALID_REQUEST_PROTO);
    return;
  }

  // User can manually blocklist a sha256 via flag, for testing.
  // This is checked just before the request is sent, to verify the request
  // would have been sent.  This emmulates the server returning a DANGEROUS
  // verdict as closely as possible.
  if (IsDownloadManuallyBlocklisted(*client_download_request_)) {
    DVLOG(1) << "Download verdict overridden to DANGEROUS by flag.";
    FinishRequest(DownloadCheckResult::DANGEROUS, REASON_MANUAL_BLOCKLIST);
    return;
  }

  NotifySendRequest(client_download_request_.get());

  DVLOG(2) << "Sending a request for URL: " << source_url_;
  DVLOG(2) << "Detected " << client_download_request_->archived_binary().size()
           << " archived "
           << "binaries (may be capped)";
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "client_download_request", "client_download_request_for_platform", R"(
          semantics {
            sender: "Download Protection Service"
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "chrome-counter-abuse-downloads@google.com"
              }
            }
          }
          policy {
            cookies_allowed: YES
            cookies_store: "Safe Browsing cookies store"
            setting:
              "Users can enable or disable the entire Safe Browsing service in "
              "Chromium's settings by toggling 'Protect you and your device "
              "from dangerous sites' under Privacy. This feature is enabled by "
              "default."
            chrome_policy {
              SafeBrowsingProtectionLevel {
                policy_options {mode: MANDATORY}
                SafeBrowsingProtectionLevel: 0
              }
            }
            chrome_policy {
              SafeBrowsingEnabled {
                policy_options {mode: MANDATORY}
                SafeBrowsingEnabled: false
              }
            }
            deprecated_policies: "SafeBrowsingEnabled"
          })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = service_->GetDownloadRequestUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->site_for_cookies =
      net::SiteForCookies::FromUrl(resource_request->url);

#if !BUILDFLAG(IS_ANDROID)
  // TODO(chlily): Factor this out into
  // DownloadProtectionDelegate::FinalizeResourceRequest.
  if (!access_token_.empty()) {
    LogAuthenticatedCookieResets(
        *resource_request,
        SafeBrowsingAuthenticatedEndpoint::kDownloadProtection);
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token_);
  }
#endif

  network::mojom::URLLoaderFactory* url_loader_factory =
      service_->GetURLLoaderFactory(GetBrowserContext()).get();
  if (!url_loader_factory) {
    FinishRequest(DownloadCheckResult::UNKNOWN, REASON_SERVER_PING_FAILED);
    return;
  }

  service_->delegate()->FinalizeResourceRequest(*resource_request);

  loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      service_->delegate()->CompleteClientDownloadRequestTrafficAnnotation(
          partial_traffic_annotation));
  loader_->AttachStringForUpload(client_download_request_data_);
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      service_->GetURLLoaderFactory(GetBrowserContext()).get(),
      base::BindOnce(&CheckClientDownloadRequestBase::OnURLLoaderComplete,
                     GetWeakPtr()));
  request_start_time_ = base::TimeTicks::Now();

  // The following is to log this ClientDownloadRequest on any open
  // chrome://safe-browsing pages. If no such page is open, the request is
  // dropped and the |client_download_request_| object deleted.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIInfoSingleton::AddToClientDownloadRequestsSent,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::move(client_download_request_)));
}

void CheckClientDownloadRequestBase::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool success = loader_->NetError() == net::OK;
  int response_code = 0;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();
  DVLOG(2) << "Received a response for URL: " << source_url_
           << ": success=" << success << " response_code=" << response_code;
  RecordHttpResponseOrErrorCode("SBClientDownload.DownloadRequestNetworkResult",
                                loader_->NetError(), response_code);

  DownloadCheckResultReason reason = REASON_SERVER_PING_FAILED;
  DownloadCheckResult result = DownloadCheckResult::UNKNOWN;
  std::string token;
  if (success && net::HTTP_OK == response_code) {
    ClientDownloadResponse response;
    if (!response.ParseFromString(*response_body.get())) {
      reason = REASON_INVALID_RESPONSE_PROTO;
      result = DownloadCheckResult::UNKNOWN;
    } else if (sampled_unsupported_file_) {
      // Ignore the verdict because we were just reporting a sampled file.
      reason = REASON_SAMPLED_UNSUPPORTED_FILE;
      result = DownloadCheckResult::UNKNOWN;
#if BUILDFLAG(IS_ANDROID)
    } else if (kMaliciousApkDownloadCheckTelemetryOnly.Get()) {
      // If Android download protection is in telemetry-only mode, ignore the
      // verdict.
      reason = REASON_IGNORED_VERDICT;
      result = DownloadCheckResult::UNKNOWN;
#endif
    } else {
      switch (response.verdict()) {
        case ClientDownloadResponse::SAFE:
          reason = REASON_DOWNLOAD_SAFE;
          result = DownloadCheckResult::SAFE;
          break;
        case ClientDownloadResponse::DANGEROUS:
          reason = REASON_DOWNLOAD_DANGEROUS;
          result = DownloadCheckResult::DANGEROUS;
          token = response.token();
          break;
        case ClientDownloadResponse::UNCOMMON:
          reason = REASON_DOWNLOAD_UNCOMMON;
          result = DownloadCheckResult::UNCOMMON;
          token = response.token();
          break;
        case ClientDownloadResponse::DANGEROUS_HOST:
          reason = REASON_DOWNLOAD_DANGEROUS_HOST;
          result = DownloadCheckResult::DANGEROUS_HOST;
          token = response.token();
          break;
        case ClientDownloadResponse::POTENTIALLY_UNWANTED:
          reason = REASON_DOWNLOAD_POTENTIALLY_UNWANTED;
          result = DownloadCheckResult::POTENTIALLY_UNWANTED;
          token = response.token();
          break;
        case ClientDownloadResponse::UNKNOWN:
          reason = REASON_VERDICT_UNKNOWN;
          result = DownloadCheckResult::UNKNOWN;
          break;
        case ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE:
          reason = REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE;
          result = DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE;
          token = response.token();
          break;
        default:
          LOG(DFATAL) << "Unknown download response verdict: "
                      << response.verdict();
          reason = REASON_INVALID_RESPONSE_VERDICT;
          result = DownloadCheckResult::UNKNOWN;
      }
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebUIInfoSingleton::AddToClientDownloadResponsesReceived,
            base::Unretained(WebUIInfoSingleton::GetInstance()),
            std::make_unique<ClientDownloadResponse>(response)));

    GetAdditionalPromptResult(response, &result, &reason, &token);

    if (!token.empty()) {
      SetDownloadProtectionData(
          token, response.verdict(),
#if !BUILDFLAG(IS_ANDROID)
          WebUIInfoSingleton::GetInstance()
              ->tailored_verdict_override()
              .override_value.value_or(response.tailored_verdict())
#else
          response.tailored_verdict()
#endif
      );
    }

#if !BUILDFLAG(IS_ANDROID)
    bool upload_requested = response.upload();
    MaybeBeginFeedbackForDownload(result, upload_requested,
                                  client_download_request_data_,
                                  *response_body.get());
#endif
  }

  // We don't need the loader anymore.
  loader_.reset();

  std::string histogram_name = "SBClientDownload.DownloadRequestDuration";
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  base::UmaHistogramTimes("SBClientDownload.DownloadRequestDuration", duration);

  switch (request_creation_details_.inspection_type) {
    case DownloadFileType::NONE:
      base::StrAppend(&histogram_name, {".None"});
      break;
    case DownloadFileType::ZIP:
      base::StrAppend(&histogram_name, {".Zip"});
      break;
    case DownloadFileType::RAR:
      base::StrAppend(&histogram_name, {".Rar"});
      break;
    case DownloadFileType::DMG:
      base::StrAppend(&histogram_name, {".Dmg"});
      break;
    case DownloadFileType::SEVEN_ZIP:
      base::StrAppend(&histogram_name, {".SevenZip"});
      break;
  }

  base::UmaHistogramTimes(histogram_name, duration);
  base::UmaHistogramTimes("SBClientDownload.DownloadRequestNetworkDuration",
                          base::TimeTicks::Now() - request_start_time_);

  FinishRequest(result, reason);
}

}  // namespace safe_browsing
