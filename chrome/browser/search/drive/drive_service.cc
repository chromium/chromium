// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service.h"

#include <memory>
#include <utility>

#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "net/base/load_flags.h"

namespace {
// The scope required for an access token in order to query ItemSuggest.
constexpr char kDriveScope[] = "https://www.googleapis.com/auth/drive.readonly";
// TODO(crbug/1171898): Will need to change and verify client_info in the
// future.
constexpr char kRequestBody[] = R"({
      'client_info': {
        'platform_type': 'UNSPECIFIED_PLATFORM',
        'scenario_type': 'CHROME_NTP_FILES',
        'request_type': 'LIVE_REQUEST'
      },
      'max_suggestions': 10,
      'type_detail_fields': 'drive_item.title,justification.display_text'
    })";
// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;
const char server_url[] = "https://appsitemsuggest-pa.googleapis.com/v1/items";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("drive_service", R"(
      semantics {
        sender: "Drive Service"
        description:
          "The Drive Service requests suggestions for Drive files from "
          "the Drive ItemSuggest API. The response will be displayed in NTP's "
          "Drive Module."
        trigger:
          "Each time a user navigates to the NTP while "
          "the Drive module is enabled and the user is "
          "signed in."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can control this feature by (1) selecting "
          "a non-Google default search engine in Chrome "
          "settings under 'Search Engine', (2) signing out, "
          "or (3) disabling the Drive module."
        chrome_policy {
          DefaultSearchProviderEnabled {
            policy_options {mode: MANDATORY}
            DefaultSearchProviderEnabled: false
          }
          BrowserSignin {
            policy_options {mode: MANDATORY}
            BrowserSignin: 0
          }
          NTPCardsVisible {
            NTPCardsVisible: false
          }
        }
      })");
}  // namespace

DriveService::~DriveService() = default;

DriveService::DriveService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {}

void DriveService::GetDriveSuggestions(SuggestionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug/1168763) May need to handle multiple requests after
  // token_fetcher has been set.
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ntp_drive_module", identity_manager_, signin::ScopeSet({kDriveScope}),
      base::BindOnce(&DriveService::OnTokenReceived, weak_factory_.GetWeakPtr(),
                     std::move(callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void DriveService::OnTokenReceived(SuggestionsCallback callback,
                                   GoogleServiceAuthError error,
                                   signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run("");
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = GURL(server_url);
  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ignore cache for fresh results.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->AttachStringForUpload(kRequestBody, "application/json");
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DriveService::OnSuggestionsReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSize);
}

void DriveService::OnSuggestionsReceived(
    SuggestionsCallback callback,
    const std::unique_ptr<std::string> json_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  if (net_error != net::OK) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run("JSON response");
}
