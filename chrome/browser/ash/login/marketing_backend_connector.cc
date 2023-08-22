// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/marketing_backend_connector.h"

#include <cstddef>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/credentials_mode.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace {

// The scope that will be used to access the ChromebookEmailService API.
const char kChromebookOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromebook.email";

// API Endpoint
const char kAccessPointsApiEndpoint[] = "https://accesspoints.googleapis.com/";
const char kChromebookEmailServicePath[] = "v2/chromebookEmailPreferences";
constexpr size_t kResponseMaxBodySize = 4 * 1024 * 1024;  // 4MiB

const GURL GetChromebookServiceEndpoint() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Allows the URL to be completely overridden from the command line.
  return (command_line->HasSwitch(switches::kMarketingOptInUrl))
             ? GURL(command_line->GetSwitchValueASCII(
                   switches::kMarketingOptInUrl))
             : GURL(kAccessPointsApiEndpoint +
                    std::string(kChromebookEmailServicePath));
}

// UMA Metrics
void RecordUMAHistogram(MarketingBackendConnector::BackendConnectorEvent event,
                        const std::string& country) {
  base::UmaHistogramEnumeration(
      "OOBE.MarketingOptInScreen.BackendConnector." + country, event);

  // Generic event aggregating data from all countries.
  base::UmaHistogramEnumeration("OOBE.MarketingOptInScreen.BackendConnector",
                                event);
}

std::unique_ptr<network::ResourceRequest> GetResourceRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetChromebookServiceEndpoint();
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  resource_request->method = "POST";
  return resource_request;
}

}  // namespace

// static
base::RepeatingCallback<void(std::string)>*
    MarketingBackendConnector::request_finished_for_tests_ = nullptr;

// static
void MarketingBackendConnector::UpdateEmailPreferences(
    Profile* profile,
    const std::string& country_code) {
  DCHECK(profile);
  VLOG(1) << "Subscribing the user to all chromebook email campaigns.";

  // Early exit for testing
  if (MarketingBackendConnector::request_finished_for_tests_ != nullptr) {
    std::move(*MarketingBackendConnector::request_finished_for_tests_)
        .Run(country_code);
    return;
  }

  // No requests without a Gaia account
  if (profile->IsOffTheRecord())
    return;

  scoped_refptr<MarketingBackendConnector> ref =
      new MarketingBackendConnector(profile);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MarketingBackendConnector::PerformRequest, ref,
                                country_code));
}

MarketingBackendConnector::MarketingBackendConnector(Profile* profile)
    : profile_(profile) {}

void MarketingBackendConnector::PerformRequest(
    const std::string& country_code) {
  country_code_ = country_code;
  StartTokenFetch();
}

void MarketingBackendConnector::StartTokenFetch() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    RecordUMAHistogram(BackendConnectorEvent::kErrorOther, country_code_);
    return;
  }

  signin::ScopeSet chromebook_scope;
  chromebook_scope.insert(kChromebookOAuth2Scope);
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "MarketingBackendConnector", identity_manager, chromebook_scope,
      base::BindOnce(&MarketingBackendConnector::OnAccessTokenRequestCompleted,
                     this),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void MarketingBackendConnector::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::NONE) {
    access_token_ = access_token_info.token;
    VLOG(2) << "Token fetch succeeded.";
    SetTokenAndStartRequest();
  } else {
    VLOG(1) << "Auth Error: " << error.ToString();
    RecordUMAHistogram(BackendConnectorEvent::kErrorAuth, country_code_);
  }
}

void MarketingBackendConnector::SetTokenAndStartRequest() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chromebook_mail_api", R"(
      semantics {
        sender: "Chrome OS Marketing Opt-In Screen"
        description:
          "Communication with the Chromebook Email API to change the user's "
          "preference regarding marketing emails. It is only used on the "
          "last screen of the Chrome OS OOBE - Marketing Opt-In Screen."
        trigger:
          "The request is triggered when the user opts-in for marketing "
          "emails by enabling the toggle on the marketing opt-in screen."
        data:
          "The only transmitted information is the country and the language "
          "of the user's account. This information is used for delivering "
          "emails to the user in the requested language."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "Not opting-in to the emails will not generate a request."
        cookies_allowed: NO
        policy_exception_justification:
          "Managed users are not presented with the option to opt-in."
      }
      )");

  auto resource_request = GetResourceRequest();
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      std::string("Bearer ") + access_token_);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->AttachStringForUpload(GetRequestContent(),
                                            "application/json");

  url_loader_factory_ = profile_->GetURLLoaderFactory();
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&MarketingBackendConnector::OnSimpleLoaderComplete, this),
      kResponseMaxBodySize);
}

void MarketingBackendConnector::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  std::string raw_header;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  std::string data;
  if (response_body)
    data = std::move(*response_body);

  OnSimpleLoaderCompleteInternal(response_code, data);
}

void MarketingBackendConnector::OnSimpleLoaderCompleteInternal(
    int response_code,
    const std::string& data) {
  VLOG(2) << "Response Code = " << response_code << " Data = " << data;

  switch (response_code) {
    case net::HTTP_OK: {
      VLOG(1) << "Successfully set the user preferences on the server.";
      RecordUMAHistogram(BackendConnectorEvent::kSuccess, country_code_);
      return;
    }
    case net::HTTP_INTERNAL_SERVER_ERROR: {
      VLOG(1) << "Internal server error occurred.";
      RecordUMAHistogram(BackendConnectorEvent::kErrorServerInternal,
                         country_code_);
      return;
    }
    case net::HTTP_REQUEST_TIMEOUT: {
      RecordUMAHistogram(BackendConnectorEvent::kErrorRequestTimeout,
                         country_code_);
      return;
    }
    case net::HTTP_UNAUTHORIZED: {
      RecordUMAHistogram(BackendConnectorEvent::kErrorAuth, country_code_);
      return;
    }
  }
  // Failure. There is nothing we can do at this point.
  RecordUMAHistogram(BackendConnectorEvent::kErrorOther, country_code_);
}

std::string MarketingBackendConnector::GetRequestContent() {
  base::Value::Dict request_dict;
  request_dict.Set("country_code", country_code_);
  request_dict.Set("language", "en");

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  return request_content;
}

MarketingBackendConnector::~MarketingBackendConnector() = default;

ScopedRequestCallbackSetter::ScopedRequestCallbackSetter(
    std::unique_ptr<base::RepeatingCallback<void(std::string)>> callback)
    : callback_(std::move(callback)) {
  MarketingBackendConnector::request_finished_for_tests_ = callback_.get();
}

ScopedRequestCallbackSetter::~ScopedRequestCallbackSetter() {
  MarketingBackendConnector::request_finished_for_tests_ = nullptr;
}

}  // namespace ash
