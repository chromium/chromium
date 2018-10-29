// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/load_flags.h"
#include "net/http/http_cache.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

const char kDownloadExtensionUmaName[] = "SBClientDownload.DownloadExtensions";
const char kUnsupportedSchemeUmaPrefix[] = "SBClientDownload.UnsupportedScheme";

void RecordFileExtensionType(const std::string& metric_name,
                             const base::FilePath& file) {
  base::UmaHistogramSparse(
      metric_name, FileTypePolicies::GetInstance()->UmaValueForFile(file));
}

std::string GetUnsupportedSchemeName(const GURL& download_url) {
  if (download_url.SchemeIs(url::kContentScheme))
    return "ContentScheme";
  if (download_url.SchemeIs(url::kContentIDScheme))
    return "ContentIdScheme";
  if (download_url.SchemeIsFile())
    return download_url.has_host() ? "RemoteFileScheme" : "LocalFileScheme";
  if (download_url.SchemeIsFileSystem())
    return "FileSystemScheme";
  if (download_url.SchemeIs(url::kFtpScheme))
    return "FtpScheme";
  if (download_url.SchemeIs(url::kGopherScheme))
    return "GopherScheme";
  if (download_url.SchemeIs(url::kJavaScriptScheme))
    return "JavaScriptScheme";
  if (download_url.SchemeIsWSOrWSS())
    return "WSOrWSSScheme";
  return "OtherUnsupportedScheme";
}

}  // namespace

CheckClientDownloadRequest::CheckClientDownloadRequest(
    download::DownloadItem* item,
    const CheckDownloadCallback& callback,
    DownloadProtectionService* service,
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    BinaryFeatureExtractor* binary_feature_extractor)
    : item_(item),
      url_chain_(item->GetUrlChain()),
      referrer_url_(item->GetReferrerUrl()),
      tab_url_(item->GetTabUrl()),
      tab_referrer_url_(item->GetTabReferrerUrl()),
      archived_executable_(false),
      archive_is_valid_(FileAnalyzer::ArchiveValid::UNSET),
#if defined(OS_MACOSX)
      disk_image_signature_(nullptr),
#endif
      callback_(callback),
      service_(service),
      binary_feature_extractor_(binary_feature_extractor),
      database_manager_(database_manager),
      pingback_enabled_(service_->enabled()),
      file_analyzer_(new FileAnalyzer(binary_feature_extractor_)),
      finished_(false),
      type_(ClientDownloadRequest::WIN_EXECUTABLE),
      start_time_(base::TimeTicks::Now()),
      skipped_url_whitelist_(false),
      skipped_certificate_whitelist_(false),
      is_extended_reporting_(false),
      is_incognito_(false),
      is_under_advanced_protection_(false),
      weakptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->AddObserver(this);
}

bool CheckClientDownloadRequest::ShouldSampleUnsupportedFile(
    const base::FilePath& filename) {
  // If this extension is specifically marked as SAMPLED_PING (as are
  // all "unknown" extensions), we may want to sample it. Sampling it means
  // we'll send a "light ping" with private info removed, and we won't
  // use the verdict.
  const FileTypePolicies* policies = FileTypePolicies::GetInstance();
  return service_ && is_extended_reporting_ && !is_incognito_ &&
         base::RandDouble() < policies->SampledPingProbability() &&
         policies->PingSettingForFile(filename) ==
             DownloadFileType::SAMPLED_PING;
}

void CheckClientDownloadRequest::Start() {
  DVLOG(2) << "Starting SafeBrowsing download check for: "
           << item_->DebugString(true);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item_);
  if (browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    is_extended_reporting_ =
        profile && IsExtendedReportingEnabled(*profile->GetPrefs());
    is_incognito_ = browser_context->IsOffTheRecord();
    is_under_advanced_protection_ =
        profile &&
        AdvancedProtectionStatusManager::IsUnderAdvancedProtection(profile);
  }

  // If whitelist check passes, PostFinishTask() will be called to avoid
  // analyzing file. Otherwise, AnalyzeFile() will be called to continue with
  // analysis.
  auto io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
  cancelable_task_tracker_.PostTask(
      io_task_runner.get(), FROM_HERE,
      base::BindOnce(&CheckClientDownloadRequest::CheckUrlAgainstWhitelist,
                     this));
}

// Start a timeout to cancel the request if it takes too long.
// This should only be called after we have finished accessing the file.
void CheckClientDownloadRequest::StartTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_) {
    // Request has already been cancelled.
    return;
  }
  timeout_start_time_ = base::TimeTicks::Now();
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckClientDownloadRequest::Cancel,
                     weakptr_factory_.GetWeakPtr(), false),
      base::TimeDelta::FromMilliseconds(
          service_->download_request_timeout_ms()));
}

// Canceling a request will cause us to always report the result as
// DownloadCheckResult::UNKNOWN unless a pending request is about to call
// FinishRequest.
void CheckClientDownloadRequest::Cancel(bool download_destroyed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cancelable_task_tracker_.TryCancelAll();
  if (loader_.get()) {
    // The DownloadProtectionService is going to release its reference, so we
    // might be destroyed before the URLLoader completes.  Cancel the
    // loader so it does not try to invoke OnURLFetchComplete.
    loader_.reset();
  }
  // Note: If there is no loader, then some callback is still holding a
  // reference to this object.  We'll eventually wind up in some method on
  // the UI thread that will call FinishRequest() again.  If FinishRequest()
  // is called a second time, it will be a no-op.
  FinishRequest(DownloadCheckResult::UNKNOWN, download_destroyed
                                                  ? REASON_DOWNLOAD_DESTROYED
                                                  : REASON_REQUEST_CANCELED);
  // Calling FinishRequest might delete this object, we may be deleted by
  // this point.
}

// download::DownloadItem::Observer implementation.
void CheckClientDownloadRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  Cancel(/*download_destroyed=*/true);
}

// TODO: this method puts "DownloadProtectionService::" in front of a lot of
// stuff to avoid referencing the enums i copied to this .h file.
void CheckClientDownloadRequest::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool success = loader_->NetError() == net::OK;
  int response_code = 0;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();
  DVLOG(2) << "Received a response for URL: " << item_->GetUrlChain().back()
           << ": success=" << success << " response_code=" << response_code;
  if (success) {
    base::UmaHistogramSparse("SBClientDownload.DownloadRequestResponseCode",
                             response_code);
  }
  base::UmaHistogramSparse("SBClientDownload.DownloadRequestNetError",
                           -loader_->NetError());
  DownloadCheckResultReason reason = REASON_SERVER_PING_FAILED;
  DownloadCheckResult result = DownloadCheckResult::UNKNOWN;
  std::string token;
  if (success && net::HTTP_OK == response_code) {
    ClientDownloadResponse response;
    if (!response.ParseFromString(*response_body.get())) {
      reason = REASON_INVALID_RESPONSE_PROTO;
      result = DownloadCheckResult::UNKNOWN;
    } else if (type_ == ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE) {
      // Ignore the verdict because we were just reporting a sampled file.
      reason = REASON_SAMPLED_UNSUPPORTED_FILE;
      result = DownloadCheckResult::UNKNOWN;
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
        default:
          LOG(DFATAL) << "Unknown download response verdict: "
                      << response.verdict();
          reason = REASON_INVALID_RESPONSE_VERDICT;
          result = DownloadCheckResult::UNKNOWN;
      }
    }

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &WebUIInfoSingleton::AddToClientDownloadResponsesReceived,
            base::Unretained(WebUIInfoSingleton::GetInstance()),
            std::make_unique<ClientDownloadResponse>(response)));

    if (!token.empty())
      DownloadProtectionService::SetDownloadPingToken(item_, token);

    bool upload_requested = response.upload();
    DownloadFeedbackService::MaybeStorePingsForDownload(
        result, upload_requested, item_, client_download_request_data_,
        *response_body.get());
  }
  // We don't need the loader anymore.
  loader_.reset();
  UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestDuration",
                      base::TimeTicks::Now() - start_time_);
  UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestNetworkDuration",
                      base::TimeTicks::Now() - request_start_time_);

  FinishRequest(result, reason);
}

// static
bool CheckClientDownloadRequest::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path,
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  if (item.GetUrlChain().empty()) {
    *reason = REASON_EMPTY_URL_CHAIN;
    return false;
  }
  const GURL& final_url = item.GetUrlChain().back();
  if (!final_url.is_valid() || final_url.is_empty()) {
    *reason = REASON_INVALID_URL;
    return false;
  }
  if (!final_url.IsStandard() && !final_url.SchemeIsBlob() &&
      !final_url.SchemeIs(url::kDataScheme)) {
    *reason = REASON_UNSUPPORTED_URL_SCHEME;
    return false;
  }
  // TODO(crbug.com/814813): Remove duplicated counting of REMOTE_FILE
  // and LOCAL_FILE in SBClientDownload.UnsupportedScheme.*.
  if (final_url.SchemeIsFile()) {
    *reason = final_url.has_host() ? REASON_REMOTE_FILE : REASON_LOCAL_FILE;
    return false;
  }
  // This check should be last, so we know the earlier checks passed.
  if (!FileTypePolicies::GetInstance()->IsCheckedBinaryFile(target_path)) {
    *reason = REASON_NOT_BINARY_FILE;
    return false;
  }
  *type = download_type_util::GetDownloadType(target_path);
  return true;
}

CheckClientDownloadRequest::~CheckClientDownloadRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(item_ == NULL);
}

void CheckClientDownloadRequest::AnalyzeFile() {
  // Returns if DownloadItem is destroyed during whitelist check.
  if (item_ == nullptr) {
    PostFinishTask(DownloadCheckResult::UNKNOWN, REASON_REQUEST_CANCELED);
    return;
  }

  DownloadCheckResultReason reason = REASON_MAX;
  if (!IsSupportedDownload(*item_, item_->GetTargetFilePath(), &reason,
                           &type_)) {
    switch (reason) {
      case REASON_EMPTY_URL_CHAIN:
      case REASON_INVALID_URL:
      case REASON_LOCAL_FILE:
      case REASON_REMOTE_FILE:
        PostFinishTask(DownloadCheckResult::UNKNOWN, reason);
        return;
      case REASON_UNSUPPORTED_URL_SCHEME:
        RecordFileExtensionType(
            base::StringPrintf(
                "%s.%s", kUnsupportedSchemeUmaPrefix,
                GetUnsupportedSchemeName(item_->GetUrlChain().back()).c_str()),
            item_->GetTargetFilePath());
        PostFinishTask(DownloadCheckResult::UNKNOWN, reason);
        return;
      case REASON_NOT_BINARY_FILE:
        if (ShouldSampleUnsupportedFile(item_->GetTargetFilePath())) {
          // Send a "light ping" and don't use the verdict.
          type_ = ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE;
          break;
        }
        RecordFileExtensionType(kDownloadExtensionUmaName,
                                item_->GetTargetFilePath());
        PostFinishTask(DownloadCheckResult::UNKNOWN, reason);
        return;

      default:
        // We only expect the reasons explicitly handled above.
        NOTREACHED();
    }
  }
  RecordFileExtensionType(kDownloadExtensionUmaName,
                          item_->GetTargetFilePath());

  file_analyzer_->Start(
      item_->GetTargetFilePath(), item_->GetFullPath(),
      base::BindOnce(&CheckClientDownloadRequest::OnFileFeatureExtractionDone,
                     weakptr_factory_.GetWeakPtr()));
}

void CheckClientDownloadRequest::OnFileFeatureExtractionDone(
    FileAnalyzer::Results results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This can run in any thread, since it just posts more messages.
  if (item_ == nullptr) {
    PostFinishTask(DownloadCheckResult::UNKNOWN, REASON_REQUEST_CANCELED);
    return;
  }

  // If it's an archive with no archives or executables, finish early.
  if ((type_ == ClientDownloadRequest::ZIPPED_EXECUTABLE ||
       type_ == ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE) &&
      !results.archived_executable && !results.archived_archive &&
      results.archive_is_valid == FileAnalyzer::ArchiveValid::VALID) {
    PostFinishTask(DownloadCheckResult::UNKNOWN,
                   REASON_ARCHIVE_WITHOUT_BINARIES);
  }

  // The content checks cannot determine that we decided to sample this file, so
  // special case that DownloadType.
  if (type_ != ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE)
    type_ = results.type;
  archived_executable_ = results.archived_executable;
  archive_is_valid_ = results.archive_is_valid;
  archived_binaries_.CopyFrom(results.archived_binaries);
  signature_info_ = results.signature_info;
  image_headers_.reset(new ClientDownloadRequest_ImageHeaders());
  *image_headers_ = results.image_headers;

#if defined(OS_MACOSX)
  if (!results.disk_image_signature.empty())
    disk_image_signature_ =
        std::make_unique<std::vector<uint8_t>>(results.disk_image_signature);
  else
    disk_image_signature_ = nullptr;
  detached_code_signatures_.CopyFrom(results.detached_code_signatures);
#endif

  // TODO(noelutz): DownloadInfo should also contain the IP address of
  // every URL in the redirect chain.  We also should check whether the
  // download URL is hosted on the internal network.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &CheckClientDownloadRequest::CheckCertificateChainAgainstWhitelist,
          this));

  // We wait until after the file checks finish to start the timeout, as
  // windows can cause permissions errors if the timeout fired while we were
  // checking the file signature and we tried to complete the download.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckClientDownloadRequest::StartTimeout, this));
}

bool CheckClientDownloadRequest::ShouldSampleWhitelistedDownload() {
  // We currently sample 1% whitelisted downloads from users who opted
  // in extended reporting and are not in incognito mode.
  return service_ && is_extended_reporting_ && !is_incognito_ &&
         base::RandDouble() < service_->whitelist_sample_rate();
}

void CheckClientDownloadRequest::CheckUrlAgainstWhitelist() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!database_manager_.get()) {
    PostFinishTask(DownloadCheckResult::UNKNOWN, REASON_SB_DISABLED);
    return;
  }

  const GURL& url = url_chain_.back();
  if (url.is_valid() && database_manager_->MatchDownloadWhitelistUrl(url)) {
    DVLOG(2) << url << " is on the download whitelist.";
    RecordCountOfWhitelistedDownload(URL_WHITELIST);
    if (ShouldSampleWhitelistedDownload()) {
      skipped_url_whitelist_ = true;
    } else {
      // TODO(grt): Continue processing without uploading so that
      // ClientDownloadRequest callbacks can be run even for this type of safe
      // download.
      PostFinishTask(DownloadCheckResult::SAFE, REASON_WHITELISTED_URL);
      return;
    }
  }

  // Posts task to continue with analysis.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckClientDownloadRequest::AnalyzeFile, this));
}

void CheckClientDownloadRequest::CheckCertificateChainAgainstWhitelist() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!database_manager_.get()) {
    PostFinishTask(DownloadCheckResult::UNKNOWN, REASON_SB_DISABLED);
    return;
  }

  if (!skipped_url_whitelist_ && signature_info_.trusted()) {
    for (int i = 0; i < signature_info_.certificate_chain_size(); ++i) {
      if (CertificateChainIsWhitelisted(signature_info_.certificate_chain(i))) {
        RecordCountOfWhitelistedDownload(SIGNATURE_WHITELIST);
        if (ShouldSampleWhitelistedDownload()) {
          skipped_certificate_whitelist_ = true;
          break;
        } else {
          // TODO(grt): Continue processing without uploading so that
          // ClientDownloadRequest callbacks can be run even for this type of
          // safe download.
          PostFinishTask(DownloadCheckResult::SAFE, REASON_TRUSTED_EXECUTABLE);
          return;
        }
      }
    }
  }

  RecordCountOfWhitelistedDownload(NO_WHITELIST_MATCH);

  if (!pingback_enabled_) {
    PostFinishTask(DownloadCheckResult::UNKNOWN, REASON_PING_DISABLED);
    return;
  }

  // The URLLoader is owned by the UI thread, so post a message to
  // start the pingback.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckClientDownloadRequest::GetTabRedirects, this));
}

void CheckClientDownloadRequest::GetTabRedirects() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_)
    return;

  if (!tab_url_.is_valid()) {
    SendRequest();
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history) {
    SendRequest();
    return;
  }

  history->QueryRedirectsTo(
      tab_url_,
      base::Bind(&CheckClientDownloadRequest::OnGotTabRedirects,
                 base::Unretained(this), tab_url_),
      &request_tracker_);
}

void CheckClientDownloadRequest::OnGotTabRedirects(
    const GURL& url,
    const history::RedirectList* redirect_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(url, tab_url_);
  if (!service_)
    return;

  if (!redirect_list->empty()) {
    tab_redirects_.insert(tab_redirects_.end(), redirect_list->rbegin(),
                          redirect_list->rend());
  }

  SendRequest();
}

// If the hash of either the original file or any executables within an
// archive matches the blacklist flag, return true.
bool CheckClientDownloadRequest::IsDownloadManuallyBlacklisted(
    const ClientDownloadRequest& request) {
  if (service_->IsHashManuallyBlacklisted(request.digests().sha256()))
    return true;

  for (auto bin_itr : request.archived_binary()) {
    if (service_->IsHashManuallyBlacklisted(bin_itr.digests().sha256()))
      return true;
  }
  return false;
}

// Prepares URLs to be put into a ping message. Currently this just shortens
// data: URIs, other URLs are included verbatim. If this is a sampled binary,
// we'll send a light-ping which strips PII from the URL.
std::string CheckClientDownloadRequest::SanitizeUrl(const GURL& url) const {
  if (type_ == ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE)
    return url.GetOrigin().spec();

  return ShortURLForReporting(url);
}

void CheckClientDownloadRequest::SendRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is our last chance to check whether the request has been canceled
  // before sending it.
  if (!service_)
    return;

  auto request = std::make_unique<ClientDownloadRequest>();
  auto population = is_extended_reporting_
                        ? ChromeUserPopulation::EXTENDED_REPORTING
                        : ChromeUserPopulation::SAFE_BROWSING;
  request->mutable_population()->set_user_population(population);
  request->mutable_population()->set_profile_management_status(
      GetProfileManagementStatus(
          g_browser_process->browser_policy_connector()));
  request->mutable_population()->set_is_under_advanced_protection(
      is_under_advanced_protection_);

  request->set_url(SanitizeUrl(item_->GetUrlChain().back()));
  request->mutable_digests()->set_sha256(item_->GetHash());
  request->set_length(item_->GetReceivedBytes());
  request->set_skipped_url_whitelist(skipped_url_whitelist_);
  request->set_skipped_certificate_whitelist(skipped_certificate_whitelist_);
  request->set_locale(g_browser_process->GetApplicationLocale());
  for (size_t i = 0; i < item_->GetUrlChain().size(); ++i) {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    resource->set_url(SanitizeUrl(item_->GetUrlChain()[i]));
    if (i == item_->GetUrlChain().size() - 1) {
      // The last URL in the chain is the download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource->set_referrer(SanitizeUrl(item_->GetReferrerUrl()));
      DVLOG(2) << "dl url " << resource->url();
      if (!item_->GetRemoteAddress().empty()) {
        resource->set_remote_ip(item_->GetRemoteAddress());
        DVLOG(2) << "  dl url remote addr: " << resource->remote_ip();
      }
      DVLOG(2) << "dl referrer " << resource->referrer();
    } else {
      DVLOG(2) << "dl redirect " << i << " " << resource->url();
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }
    // TODO(noelutz): fill out the remote IP addresses.
  }
  // TODO(mattm): fill out the remote IP addresses for tab resources.
  for (size_t i = 0; i < tab_redirects_.size(); ++i) {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    DVLOG(2) << "tab redirect " << i << " " << tab_redirects_[i].spec();
    resource->set_url(SanitizeUrl(tab_redirects_[i]));
    resource->set_type(ClientDownloadRequest::TAB_REDIRECT);
  }
  if (tab_url_.is_valid()) {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    resource->set_url(SanitizeUrl(tab_url_));
    DVLOG(2) << "tab url " << resource->url();
    resource->set_type(ClientDownloadRequest::TAB_URL);
    if (tab_referrer_url_.is_valid()) {
      resource->set_referrer(SanitizeUrl(tab_referrer_url_));
      DVLOG(2) << "tab referrer " << resource->referrer();
    }
  }

  request->set_user_initiated(item_->HasUserGesture());
  request->set_file_basename(
      item_->GetTargetFilePath().BaseName().AsUTF8Unsafe());
  request->set_download_type(type_);

  ReferrerChainData* referrer_chain_data = static_cast<ReferrerChainData*>(
      item_->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey));
  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
    UMA_HISTOGRAM_COUNTS_100(
        "SafeBrowsing.ReferrerURLChainSize.DownloadAttribution",
        referrer_chain_data->referrer_chain_length());
    if (type_ == ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE) {
      SafeBrowsingNavigationObserverManager::SanitizeReferrerChain(
          request->mutable_referrer_chain());
    }
  }

#if defined(OS_MACOSX)
  UMA_HISTOGRAM_BOOLEAN(
      "SBClientDownload."
      "DownloadFileHasDmgSignature",
      disk_image_signature_ != nullptr);
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.DownloadFileHasDetachedSignatures",
                        !detached_code_signatures_.empty());

  if (disk_image_signature_) {
    request->set_udif_code_signature(disk_image_signature_->data(),
                                     disk_image_signature_->size());
  }
  if (!detached_code_signatures_.empty()) {
    request->mutable_detached_code_signature()->Swap(
        &detached_code_signatures_);
  }
#endif

  if (archive_is_valid_ != FileAnalyzer::ArchiveValid::UNSET)
    request->set_archive_valid(archive_is_valid_ ==
                               FileAnalyzer::ArchiveValid::VALID);
  request->mutable_signature()->CopyFrom(signature_info_);
  if (image_headers_)
    request->set_allocated_image_headers(image_headers_.release());
  if (!archived_binaries_.empty())
    request->mutable_archived_binary()->Swap(&archived_binaries_);
  if (!request->SerializeToString(&client_download_request_data_)) {
    FinishRequest(DownloadCheckResult::UNKNOWN, REASON_INVALID_REQUEST_PROTO);
    return;
  }

  // User can manually blacklist a sha256 via flag, for testing.
  // This is checked just before the request is sent, to verify the request
  // would have been sent.  This emmulates the server returning a DANGEROUS
  // verdict as closely as possible.
  if (IsDownloadManuallyBlacklisted(*request)) {
    DVLOG(1) << "Download verdict overridden to DANGEROUS by flag.";
    PostFinishTask(DownloadCheckResult::DANGEROUS, REASON_MANUAL_BLACKLIST);
    return;
  }

  service_->client_download_request_callbacks_.Notify(item_, request.get());

  DVLOG(2) << "Sending a request for URL: " << item_->GetUrlChain().back();
  DVLOG(2) << "Detected " << request->archived_binary().size() << " archived "
           << "binaries (may be capped)";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("client_download_request", R"(
          semantics {
            sender: "Download Protection Service"
            description:
              "Chromium checks whether a given download is likely to be "
              "dangerous by sending this client download request to Google's "
              "Safe Browsing servers. Safe Browsing server will respond to "
              "this request by sending back a verdict, indicating if this "
              "download is safe or the danger type of this download (e.g. "
              "dangerous content, uncommon content, potentially harmful, etc)."
            trigger:
              "This request is triggered when a download is about to complete, "
              "the download is not whitelisted, and its file extension is "
              "supported by download protection service (e.g. executables, "
              "archives). Please refer to https://cs.chromium.org/chromium/src/"
              "chrome/browser/resources/safe_browsing/"
              "download_file_types.asciipb for the complete list of supported "
              "files."
            data:
              "URL of the file to be downloaded, its referrer chain, digest "
              "and other features extracted from the downloaded file. Refer to "
              "ClientDownloadRequest message in https://cs.chromium.org/"
              "chromium/src/components/safe_browsing/csd.proto for all "
              "submitted features."
            destination: GOOGLE_OWNED_SERVICE
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
              SafeBrowsingEnabled {
                policy_options {mode: MANDATORY}
                SafeBrowsingEnabled: false
              }
            }
          })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = PPAPIDownloadRequest::GetDownloadRequestUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             traffic_annotation);
  loader_->AttachStringForUpload(client_download_request_data_,
                                 "application/octet-stream");
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      service_->url_loader_factory_.get(),
      base::BindOnce(&CheckClientDownloadRequest::OnURLLoaderComplete,
                     base::Unretained(this)));
  request_start_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_COUNTS_1M("SBClientDownload.DownloadRequestPayloadSize",
                          client_download_request_data_.size());

  // The following is to log this ClientDownloadRequest on any open
  // chrome://safe-browsing pages. If no such page is open, the request is
  // dropped and the |request| object deleted.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebUIInfoSingleton::AddToClientDownloadRequestsSent,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::move(request)));
}

void CheckClientDownloadRequest::PostFinishTask(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckClientDownloadRequest::FinishRequest, this, result,
                     reason));
}

void CheckClientDownloadRequest::FinishRequest(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (finished_) {
    return;
  }
  finished_ = true;

  // Ensure the timeout task is cancelled while we still have a non-zero
  // refcount. (crbug.com/240449)
  weakptr_factory_.InvalidateWeakPtrs();
  if (!request_start_time_.is_null()) {
    UMA_HISTOGRAM_ENUMERATION("SBClientDownload.DownloadRequestNetworkStats",
                              reason, REASON_MAX);
  }
  if (!timeout_start_time_.is_null()) {
    UMA_HISTOGRAM_ENUMERATION("SBClientDownload.DownloadRequestTimeoutStats",
                              reason, REASON_MAX);
    if (reason != REASON_REQUEST_CANCELED) {
      UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestTimeoutDuration",
                          base::TimeTicks::Now() - timeout_start_time_);
    }
  }
  if (service_) {
    DVLOG(2) << "SafeBrowsing download verdict for: "
             << item_->DebugString(true) << " verdict:" << reason
             << " result:" << static_cast<int>(result);
    UMA_HISTOGRAM_ENUMERATION("SBClientDownload.CheckDownloadStats", reason,
                              REASON_MAX);
    if (reason != REASON_DOWNLOAD_DESTROYED)
      callback_.Run(result);
    item_->RemoveObserver(this);
    item_ = NULL;
    DownloadProtectionService* service = service_;
    service_ = NULL;
    service->RequestFinished(this);
    // DownloadProtectionService::RequestFinished will decrement our refcount,
    // so we may be deleted now.
  } else {
    callback_.Run(DownloadCheckResult::UNKNOWN);
    item_ = NULL;
  }
}

bool CheckClientDownloadRequest::CertificateChainIsWhitelisted(
    const ClientDownloadRequest_CertificateChain& chain) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (chain.element_size() < 2) {
    // We need to have both a signing certificate and its issuer certificate
    // present to construct a whitelist entry.
    return false;
  }
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(
          chain.element(0).certificate().data(),
          chain.element(0).certificate().size());
  if (!cert.get()) {
    return false;
  }

  for (int i = 1; i < chain.element_size(); ++i) {
    scoped_refptr<net::X509Certificate> issuer =
        net::X509Certificate::CreateFromBytes(
            chain.element(i).certificate().data(),
            chain.element(i).certificate().size());
    if (!issuer.get()) {
      return false;
    }
    std::vector<std::string> whitelist_strings;
    DownloadProtectionService::GetCertificateWhitelistStrings(
        *cert.get(), *issuer.get(), &whitelist_strings);
    for (size_t j = 0; j < whitelist_strings.size(); ++j) {
      if (database_manager_->MatchDownloadWhitelistString(
              whitelist_strings[j])) {
        DVLOG(2) << "Certificate matched whitelist, cert="
                 << cert->subject().GetDisplayName()
                 << " issuer=" << issuer->subject().GetDisplayName();
        return true;
      }
    }
    cert = issuer;
  }
  return false;
}

}  // namespace safe_browsing
