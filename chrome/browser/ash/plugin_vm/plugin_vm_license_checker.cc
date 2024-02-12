// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_license_checker.h"

#include <cstddef>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace plugin_vm {

namespace {

constexpr char kValidationOAuth2Scope[] =
    "https://www.googleapis.com/auth/applicense.bytebot";
constexpr char kValidationEndpoint[] = "https://bytebot.googleapis.com/";
constexpr char kValidationServicePath[] =
    "v1/applications/chromePluginVm:getLicenseStatus";
constexpr char kValidationServiceQuery[] = "?checkOnly=true&access_token=";
constexpr size_t kResponseMaxBodySize = 4 * 1024 * 1024;  // 4 MiB

const GURL GetValidationEndpoint() {
  return GURL(base::StrCat(
      {kValidationEndpoint, kValidationServicePath, kValidationServiceQuery}));
}

const net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("chrome_plugin_vm_api",
                                             R"(
      semantics {
        sender: "Chrome Plugin VM License Checker"
        description:
          "Communication with the Plugin VM License Checker API to confirm "
          "that the current managed user has a valid Plugin VM license."
        trigger:
          "The request is triggered when the system receives a PluginVmUserId."
        data:
          "The only transmitted information is an OAuth token. This "
          "information is used to verify the Plugin VM license."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "There is no setting"
        cookies_allowed: NO
        chrome_policy {
            UserPluginVmAllowed {
                UserPluginVmAllowed: false
            }
          }
      }
  )");
}

// Response Codes that indicate we don't need to evaluate the response body.
bool IsEarlyResponseCode(int response_code) {
  // A 5XX code indicates a server issue, we will assume the license is valid
  // and a later check during installation will validate the license.
  return response_code >= 500 && response_code < 600;
}

// Response codes that indicate that we can return success.
bool IsAcceptedResponseCode(int response_code) {
  return (response_code >= 200 && response_code < 300) ||
         IsEarlyResponseCode(response_code);
}

// A valid license will response with a 2XX code with an ACTIVE status in the
// body.
bool ResponseIndicatesValidLicense(int response_code,
                                   std::string response_body) {
  if (!IsAcceptedResponseCode(response_code)) {
    LOG(ERROR) << "Unable to validate license due to response code: "
               << response_code;
    return false;
  }

  if (IsEarlyResponseCode(response_code))
    return true;

  // Expected response body:
  // { "status": "ACTIVE", ...}
  std::optional<base::Value::Dict> response =
      base::JSONReader::ReadDict(response_body);
  if (!response) {
    LOG(ERROR) << "response_body was of unexpected format.";
    return false;
  }

  std::string* status = response->FindString("status");
  if (!status) {
    LOG(ERROR) << "response_body did not contain status.";
    return false;
  }

  return *status == "ACTIVE";
}

}  // namespace

PluginVmLicenseChecker::PluginVmLicenseChecker(Profile* profile)
    : profile_(profile),
      validation_url_(GetValidationEndpoint()),
      traffic_annotation_(GetTrafficAnnotation()) {
  DCHECK(profile_);
}

PluginVmLicenseChecker::~PluginVmLicenseChecker() = default;

void PluginVmLicenseChecker::CheckLicense(LicenseCheckedCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  DCHECK(!profile_->IsOffTheRecord());

  url_loader_factory_ = profile_->GetURLLoaderFactory();
  DCHECK(url_loader_factory_);

  FetchAccessToken();
}

void PluginVmLicenseChecker::FetchAccessToken() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager);

  signin::ScopeSet validation_scope;
  validation_scope.insert(kValidationOAuth2Scope);

  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ChromePluginVm", identity_manager, validation_scope,
      base::BindOnce(&PluginVmLicenseChecker::CallEndpointWithAccessToken,
                     weak_ptr_factory_.GetWeakPtr()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void PluginVmLicenseChecker::CallEndpointWithAccessToken(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Encountered GoogleServiceAuthError while attempting to"
               << " fetch OAuth2 access token. Error Info: "
               << error.ToString();
    std::move(callback_).Run(false);
    return;
  }

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest(access_token_info.token);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation_);

  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PluginVmLicenseChecker::HandleStringResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      kResponseMaxBodySize);
}

std::unique_ptr<network::ResourceRequest>
PluginVmLicenseChecker::CreateResourceRequest(std::string_view access_token) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(base::StrCat({validation_url_.spec(), access_token}));
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "GET";
  return resource_request;
}

void PluginVmLicenseChecker::HandleStringResponse(
    std::unique_ptr<std::string> response_body) {
  if (!simple_url_loader_->ResponseInfo() ||
      !simple_url_loader_->ResponseInfo()->headers) {
    LOG(ERROR) << "Did not recieve a response from server while attempting to"
               << " validate the license.";
    std::move(callback_).Run(false);
    return;
  }

  int response_code =
      simple_url_loader_->ResponseInfo()->headers->response_code();

  std::move(callback_).Run(
      ResponseIndicatesValidLicense(response_code, *response_body));
}

}  // namespace plugin_vm
