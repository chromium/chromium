// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/safe_search_url_reporter.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

const char kSafeSearchReportApiUrl[] =
    "https://safesearch.googleapis.com/v1:report";
const char kSafeSearchReportApiScope[] =
    "https://www.googleapis.com/auth/safesearch.reporting";

const int kNumSafeSearchReportRetries = 1;

struct SafeSearchURLReporter::Report {
  Report(const GURL& url, SuccessCallback callback);
  ~Report();

  GURL url;
  SuccessCallback callback;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request;
  std::string access_token;
  bool access_token_expired;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
};

SafeSearchURLReporter::Report::Report(const GURL& url, SuccessCallback callback)
    : url(url), callback(std::move(callback)), access_token_expired(false) {}

SafeSearchURLReporter::Report::~Report() {}

SafeSearchURLReporter::SafeSearchURLReporter(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : OAuth2TokenService::Consumer("safe_search_url_reporter"),
      oauth2_token_service_(oauth2_token_service),
      account_id_(account_id),
      url_loader_factory_(std::move(url_loader_factory)) {}

SafeSearchURLReporter::~SafeSearchURLReporter() {}

// static
std::unique_ptr<SafeSearchURLReporter> SafeSearchURLReporter::CreateWithProfile(
    Profile* profile) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  return base::WrapUnique(new SafeSearchURLReporter(
      token_service, signin->GetAuthenticatedAccountId(),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess()));
}

void SafeSearchURLReporter::ReportUrl(const GURL& url,
                                      SuccessCallback callback) {
  reports_.push_back(std::make_unique<Report>(url, std::move(callback)));
  StartFetching(reports_.back().get());
}

void SafeSearchURLReporter::StartFetching(Report* report) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kSafeSearchReportApiScope);
  report->access_token_request =
      oauth2_token_service_->StartRequest(account_id_, scopes, this);
}

void SafeSearchURLReporter::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  auto it = reports_.begin();
  while (it != reports_.end()) {
    if (request == (*it)->access_token_request.get())
      break;
    it++;
  }
  DCHECK(it != reports_.end());

  (*it)->access_token = token_response.access_token;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_search_url_reporter", R"(
        semantics {
          sender: "Supervised Users"
          description: "Reports a URL wrongfully flagged by SafeSearch."
          trigger: "Initiated by the user."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account and contains the URL that was "
            "wrongfully flagged."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings and is only enabled "
            "for child accounts. If sign-in is restricted to accounts from a "
            "managed domain, those accounts are not going to be child accounts."
          chrome_policy {
            RestrictSigninToPattern {
              policy_options {mode: MANDATORY}
              RestrictSigninToPattern: "*@manageddomain.com"
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kSafeSearchReportApiUrl);
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         token_response.access_token.c_str()));
  (*it)->simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  base::DictionaryValue dict;
  dict.SetKey("url", base::Value((*it)->url.spec()));
  std::string body;
  base::JSONWriter::Write(dict, &body);
  (*it)->simple_url_loader->AttachStringForUpload(body, "application/json");
  (*it)->simple_url_loader->SetRetryOptions(
      kNumSafeSearchReportRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // TODO(https://crbug.com/808498): Re-add data use measurement once
  // SimpleURLLoader supports it.
  // ID=data_use_measurement::DataUseUserData::SUPERVISED_USER
  (*it)->simple_url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SafeSearchURLReporter::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(it)));
}

void SafeSearchURLReporter::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  auto it = reports_.begin();
  while (it != reports_.end()) {
    if (request == (*it)->access_token_request.get())
      break;
    it++;
  }
  DCHECK(it != reports_.end());
  LOG(WARNING) << "Token error: " << error.ToString();
  DispatchResult(it, false);
}

void SafeSearchURLReporter::OnSimpleLoaderComplete(
    ReportList::iterator it,
    std::unique_ptr<std::string> response_body) {
  Report* report = it->get();
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(report->simple_url_loader);
  int response_code = -1;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }
  if (response_code == net::HTTP_UNAUTHORIZED &&
      !report->access_token_expired) {
    (*it)->access_token_expired = true;
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(kSafeSearchReportApiScope);
    oauth2_token_service_->InvalidateAccessToken(account_id_, scopes,
                                                 report->access_token);
    StartFetching(report);
    return;
  }
  if (response_code > 0) {
    LOG(WARNING) << "HTTP error " << response_code;
  } else if (!response_body) {
    LOG(WARNING) << "Network error " << simple_url_loader->NetError();
  }
  DispatchResult(std::move(it), response_body != nullptr);
}

void SafeSearchURLReporter::DispatchResult(ReportList::iterator it,
                                           bool success) {
  std::move((*it)->callback).Run(success);
  reports_.erase(it);
}
