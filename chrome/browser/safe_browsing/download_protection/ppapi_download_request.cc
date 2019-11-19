// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/db/database_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_cache.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

using content::BrowserThread;

namespace safe_browsing {

const char PPAPIDownloadRequest::kDownloadRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/download";

PPAPIDownloadRequest::PPAPIDownloadRequest(
    const GURL& requestor_url,
    const GURL& initiating_frame_url,
    content::WebContents* web_contents,
    const base::FilePath& default_file_path,
    const std::vector<base::FilePath::StringType>& alternate_extensions,
    Profile* profile,
    CheckDownloadCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : requestor_url_(requestor_url),
      initiating_frame_url_(initiating_frame_url),
      initiating_main_frame_url_(
          web_contents ? web_contents->GetLastCommittedURL() : GURL()),
      tab_id_(SessionTabHelper::IdForTab(web_contents)),
      default_file_path_(default_file_path),
      alternate_extensions_(alternate_extensions),
      callback_(std::move(callback)),
      service_(service),
      database_manager_(database_manager),
      start_time_(base::TimeTicks::Now()),
      supported_path_(
          GetSupportedFilePath(default_file_path, alternate_extensions)) {
  DCHECK(profile);
  is_extended_reporting_ = IsExtendedReportingEnabled(*profile->GetPrefs());

  if (service->navigation_observer_manager()) {
    has_user_gesture_ =
        service->navigation_observer_manager()->HasUserGesture(web_contents);
    if (has_user_gesture_) {
      service->navigation_observer_manager()->OnUserGestureConsumed(
          web_contents, base::Time::Now());
    }
  }
}

PPAPIDownloadRequest::~PPAPIDownloadRequest() {
  if (loader_ && !callback_.is_null())
    Finish(RequestOutcome::REQUEST_DESTROYED, DownloadCheckResult::UNKNOWN);
}

// Start the process of checking the download request. The callback passed as
// the |callback| parameter to the constructor will be invoked with the result
// of the check at some point in the future.
//
// From the this point on, the code is arranged to follow the most common
// workflow.
//
// Note that |this| should be added to the list of pending requests in the
// associated DownloadProtectionService object *before* calling Start().
// Otherwise a synchronous Finish() call may result in leaking the
// PPAPIDownloadRequest object. This is enforced via a DCHECK in
// DownloadProtectionService.
void PPAPIDownloadRequest::Start() {
  DVLOG(2) << "Starting SafeBrowsing download check for PPAPI download from "
           << requestor_url_ << " for [" << default_file_path_.value() << "] "
           << "supported path is [" << supported_path_.value() << "]";

  if (supported_path_.empty()) {
    // Neither the default_file_path_ nor any path resulting of trying out
    // |alternate_extensions_| are supported by SafeBrowsing.
    Finish(RequestOutcome::UNSUPPORTED_FILE_TYPE, DownloadCheckResult::SAFE);
    return;
  }

  // In case the request take too long, the check will abort with an UNKNOWN
  // verdict. The weak pointer used for the timeout will be invalidated (and
  // hence would prevent the timeout) if the check completes on time and
  // execution reaches Finish().
  base::PostDelayedTask(FROM_HERE, {BrowserThread::UI},
                        base::BindOnce(&PPAPIDownloadRequest::OnRequestTimedOut,
                                       weakptr_factory_.GetWeakPtr()),
                        base::TimeDelta::FromMilliseconds(
                            service_->download_request_timeout_ms()));

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PPAPIDownloadRequest::CheckWhitelistsOnIOThread,
                     requestor_url_, database_manager_,
                     weakptr_factory_.GetWeakPtr()));
}

// static
GURL PPAPIDownloadRequest::GetDownloadRequestUrl() {
  GURL url(kDownloadRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));

  return url;
}

// Whitelist checking needs to the done on the IO thread.
void PPAPIDownloadRequest::CheckWhitelistsOnIOThread(
    const GURL& requestor_url,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<PPAPIDownloadRequest> download_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << " checking whitelists for requestor URL:" << requestor_url;

  bool url_was_whitelisted =
      requestor_url.is_valid() && database_manager &&
      database_manager->MatchDownloadWhitelistUrl(requestor_url);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PPAPIDownloadRequest::WhitelistCheckComplete,
                                download_request, url_was_whitelisted));
}

void PPAPIDownloadRequest::WhitelistCheckComplete(bool was_on_whitelist) {
  DVLOG(2) << __func__ << " was_on_whitelist:" << was_on_whitelist;
  if (was_on_whitelist) {
    RecordCountOfWhitelistedDownload(URL_WHITELIST);
    // TODO(asanka): Should sample whitelisted downloads based on
    // service_->whitelist_sample_rate(). http://crbug.com/610924
    Finish(RequestOutcome::WHITELIST_HIT, DownloadCheckResult::SAFE);
    return;
  }

  // Not on whitelist, so we are going to check with the SafeBrowsing
  // backend.
  SendRequest();
}

void PPAPIDownloadRequest::SendRequest() {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ClientDownloadRequest request;
  auto population = is_extended_reporting_
                        ? ChromeUserPopulation::EXTENDED_REPORTING
                        : ChromeUserPopulation::SAFE_BROWSING;
  request.mutable_population()->set_user_population(population);
  request.mutable_population()->set_profile_management_status(
      GetProfileManagementStatus(
          g_browser_process->browser_policy_connector()));
  request.set_download_type(ClientDownloadRequest::PPAPI_SAVE_REQUEST);
  ClientDownloadRequest::Resource* resource = request.add_resources();
  resource->set_type(ClientDownloadRequest::PPAPI_DOCUMENT);
  resource->set_url(requestor_url_.spec());
  request.set_url(requestor_url_.spec());
  request.set_file_basename(supported_path_.BaseName().AsUTF8Unsafe());
  request.set_length(0);
  request.mutable_digests()->set_md5(std::string());
  for (const auto& alternate_extension : alternate_extensions_) {
    if (alternate_extension.empty())
      continue;
    DCHECK_EQ(base::FilePath::kExtensionSeparator, alternate_extension[0]);
    *(request.add_alternate_extensions()) =
        base::FilePath(alternate_extension).AsUTF8Unsafe();
  }
  if (supported_path_ != default_file_path_) {
    *(request.add_alternate_extensions()) =
        base::FilePath(default_file_path_.FinalExtension()).AsUTF8Unsafe();
  }

  service_->AddReferrerChainToPPAPIClientDownloadRequest(
      initiating_frame_url_, initiating_main_frame_url_, tab_id_,
      has_user_gesture_, &request);

  if (!request.SerializeToString(&client_download_request_data_)) {
    // More of an internal error than anything else. Note that the UNKNOWN
    // verdict gets interpreted as "allowed".
    Finish(RequestOutcome::REQUEST_MALFORMED, DownloadCheckResult::UNKNOWN);
    return;
  }

  service_->ppapi_download_request_callbacks_.Notify(&request);
  DVLOG(2) << "Sending a PPAPI download request for URL: " << request.url();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ppapi_download_request", R"(
      semantics {
        sender: "Download Protection Service"
        description:
          "Chromium checks whether a given PPAPI download is likely to be "
          "dangerous by sending this client download request to Google's "
          "Safe Browsing servers. Safe Browsing server will respond to "
          "this request by sending back a verdict, indicating if this "
          "download is safe or the danger type of this download (e.g. "
          "dangerous content, uncommon content, potentially harmful, etc)."
        trigger:
          "When user triggers a non-whitelisted PPAPI download, and the "
          "file extension is supported by download protection service. "
          "Please refer to https://cs.chromium.org/chromium/src/chrome/"
          "browser/resources/safe_browsing/download_file_types.asciipb for "
          "the complete list of supported files."
        data:
          "Download's URL, its referrer chain, and digest. Please refer to "
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
  resource_request->url = GetDownloadRequestUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             traffic_annotation);
  loader_->AttachStringForUpload(client_download_request_data_,
                                 "application/octet-stream");
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      service_->url_loader_factory_.get(),
      base::BindOnce(&PPAPIDownloadRequest::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void PPAPIDownloadRequest::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int response_code = 0;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();
  if (loader_->NetError() != net::OK || net::HTTP_OK != response_code) {
    Finish(RequestOutcome::FETCH_FAILED, DownloadCheckResult::UNKNOWN);
    return;
  }

  ClientDownloadResponse response;

  if (response.ParseFromString(*response_body.get())) {
    Finish(RequestOutcome::SUCCEEDED,
           DownloadCheckResultFromClientDownloadResponse(response.verdict()));
  } else {
    Finish(RequestOutcome::RESPONSE_MALFORMED, DownloadCheckResult::UNKNOWN);
  }
}

void PPAPIDownloadRequest::OnRequestTimedOut() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;
  Finish(RequestOutcome::TIMEDOUT, DownloadCheckResult::UNKNOWN);
}

void PPAPIDownloadRequest::Finish(RequestOutcome reason,
                                  DownloadCheckResult response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__ << " response: " << static_cast<int>(response);
  if (!callback_.is_null())
    std::move(callback_).Run(response);
  loader_.reset();
  weakptr_factory_.InvalidateWeakPtrs();

  // If the request is being destroyed, don't notify the service_. It already
  // knows.
  if (reason == RequestOutcome::REQUEST_DESTROYED)
    return;

  service_->PPAPIDownloadCheckRequestFinished(this);
  // |this| is deleted.
}

DownloadCheckResult
PPAPIDownloadRequest::DownloadCheckResultFromClientDownloadResponse(
    ClientDownloadResponse::Verdict verdict) {
  switch (verdict) {
    case ClientDownloadResponse::SAFE:
      return DownloadCheckResult::SAFE;
    case ClientDownloadResponse::UNCOMMON:
      return DownloadCheckResult::UNCOMMON;
    case ClientDownloadResponse::POTENTIALLY_UNWANTED:
      return DownloadCheckResult::POTENTIALLY_UNWANTED;
    case ClientDownloadResponse::DANGEROUS:
      return DownloadCheckResult::DANGEROUS;
    case ClientDownloadResponse::DANGEROUS_HOST:
      return DownloadCheckResult::DANGEROUS_HOST;
    case ClientDownloadResponse::UNKNOWN:
      return DownloadCheckResult::UNKNOWN;
  }
  return DownloadCheckResult::UNKNOWN;
}

// Given a |default_file_path| and a list of |alternate_extensions|,
// constructs a FilePath with each possible extension and returns one that
// satisfies IsCheckedBinaryFile(). If none are supported, returns an
// empty FilePath.

// static TODO: put above description in .h
base::FilePath PPAPIDownloadRequest::GetSupportedFilePath(
    const base::FilePath& default_file_path,
    const std::vector<base::FilePath::StringType>& alternate_extensions) {
  const FileTypePolicies* file_type_policies = FileTypePolicies::GetInstance();
  if (file_type_policies->IsCheckedBinaryFile(default_file_path))
    return default_file_path;

  for (const auto& extension : alternate_extensions) {
    base::FilePath alternative_file_path =
        default_file_path.ReplaceExtension(extension);
    if (file_type_policies->IsCheckedBinaryFile(alternative_file_path))
      return alternative_file_path;
  }

  return base::FilePath();
}

}  // namespace safe_browsing
