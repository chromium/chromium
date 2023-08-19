// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/gaia_reauth_token_fetcher.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {
namespace {

const char kApiKeyParameter[] = "key";

constexpr base::TimeDelta kWaitTimeout = base::Seconds(5);

GURL GetFetchReauthTokenUrl() {
  GURL url = GetRecoveryServiceReauthTokenURL();
  return net::AppendQueryParameter(url, kApiKeyParameter,
                                   google_apis::GetAPIKey());
}

}  // namespace

GaiaReauthTokenFetcher::GaiaReauthTokenFetcher(FetchCompleteCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

GaiaReauthTokenFetcher::~GaiaReauthTokenFetcher() = default;

void GaiaReauthTokenFetcher::Fetch() {
  DCHECK(!simple_url_loader_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetFetchReauthTokenUrl();
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  resource_request->method = "GET";

  // TODO(b/197615068): Update the "policy" field in the traffic
  // annotation when a setting to disable the API is properly implemented.
  net::NetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("gaia_reauth_token_fetcher",
                                          R"(
        semantics {
          sender: "Chrome OS Gaia Sign-in Screen"
          description:
            "This request is used to fetch a Gaia reauth request token. The "
            "token will be passed to Gaia Backend to issue a reauth proof "
            "token, which will be used in the Cryptohome recovery flow as an "
            "authentication claim."
          trigger:
            "Triggered in some cases before the user performs an online "
            "sign-in, when Cryptohome recovery is likely to be needed based on "
            "heursitics."
          data:
            "Chrome API key only. No user-related data is sent as this is "
            "triggered before user logs in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users could opt-out from Cryptohome recovery feature, which "
            "prevents generating this request."
          policy_exception_justification:
            "The feature is still under development and only enabled "
            "explicitly by flag."
        })"));

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->SetTimeoutDuration(kWaitTimeout);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory(),
      base::BindOnce(&GaiaReauthTokenFetcher::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  fetch_timer_ = std::make_unique<base::ElapsedTimer>();
}

void GaiaReauthTokenFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  // TODO(b/200782732): Add metrics to record error code.
  if (response_code == net::HTTP_OK) {
    auto message_value = base::JSONReader::Read(*response_body);
    if (message_value && message_value->is_dict()) {
      const std::string* token =
          message_value->GetDict().FindString("encodedReauthRequestToken");
      if (token != nullptr) {
        VLOG(1) << "Successfully fetched reauth request token.";
        base::UmaHistogramTimes("Login.ReauthToken.FetchDuration.Success",
                                fetch_timer_->Elapsed());
        std::move(callback_).Run(*token);
        return;
      } else {
        LOG(WARNING)
            << "Invalid response: no encodedReauthRequestToken field present.";
      }
    } else {
      LOG(WARNING) << "Invalid response: failed to parse the response.";
    }
  } else {
    LOG(WARNING) << "Failed to fetch reauth request token.";
  }
  base::UmaHistogramTimes("Login.ReauthToken.FetchDuration.Failure",
                          fetch_timer_->Elapsed());
  std::move(callback_).Run(/*token=*/std::string());
}

}  // namespace ash
