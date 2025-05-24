// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/graduation/webview_auth_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/graduation/graduation_manager.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/timer.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace ash::graduation {

namespace {

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    500,        // Initial delay in ms.
    2.0,        // Waiting time multiplier.
    0.1,        // Fuzzing percentage.
    60 * 1000,  // Maximum delay in ms.
    -1,         // Never discard the entry.
    true,       // Use initial delay.
};

WebviewAuthHandler::AuthResult GetAuthResultHistogramBucket(
    signin::SetAccountsInCookieResult result) {
  switch (result) {
    case signin::SetAccountsInCookieResult::kSuccess:
      return WebviewAuthHandler::AuthResult::kSuccess;
    case signin::SetAccountsInCookieResult::kTransientError:
      return WebviewAuthHandler::AuthResult::kTransientFailure;
    case signin::SetAccountsInCookieResult::kPersistentError:
      return WebviewAuthHandler::AuthResult::kPersistentFailure;
  }
}

}  // namespace

WebviewAuthHandler::WebviewAuthHandler(content::BrowserContext* context,
                                       const std::string& webview_host_name)
    : context_(context), retry_auth_backoff_(&kRetryBackoffPolicy) {
  CHECK(context_);
  storage_partition_config_ = content::StoragePartitionConfig::Create(
      context_, webview_host_name,
      /*partition_name=*/std::string(), /*in_memory=*/true);
}

WebviewAuthHandler::~WebviewAuthHandler() = default;

std::unique_ptr<GaiaAuthFetcher>
WebviewAuthHandler::CreateGaiaAuthFetcherForPartition(
    GaiaAuthConsumer* consumer,
    const gaia::GaiaSource& source) {
  return std::make_unique<GaiaAuthFetcher>(
      consumer, source,
      GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess());
}

network::mojom::CookieManager*
WebviewAuthHandler::GetCookieManagerForPartition() {
  return GetStoragePartition()->GetCookieManagerForBrowserProcess();
}

void WebviewAuthHandler::AuthenticateWebview(OnWebviewAuth callback) {
  VLOG(1) << "graduation: webview auth started";

  if (cookie_loader_) {
    cookie_loader_.reset();
  }
  retry_auth_timer_.Stop();

  signin::IdentityManager* identity_manager =
      GraduationManager::Get()->GetIdentityManager(context_);
  CHECK(identity_manager);

  // We only need primary account authentication in the webview.
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  cookie_loader_ =
      identity_manager->GetAccountsCookieMutator()
          ->SetAccountsInCookieForPartition(
              this,
              {gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
               {primary_account_id}},
              gaia::GaiaSource::kChromeOS,
              base::BindOnce(&WebviewAuthHandler::OnAuthFinished,
                             base::Unretained(this), std::move(callback)));
}

void WebviewAuthHandler::OnAuthFinished(
    OnWebviewAuth callback,
    signin::SetAccountsInCookieResult cookie_result) {
  VLOG(1) << "graduation: webview auth finished";
  base::UmaHistogramEnumeration(kAuthResultHistogramName,
                                GetAuthResultHistogramBucket(cookie_result));
  switch (cookie_result) {
    case signin::SetAccountsInCookieResult::kSuccess:
      VLOG(1) << "graduation: webview auth successful";
      CompleteAuth(std::move(callback), /*is_success=*/true);
      break;
    case signin::SetAccountsInCookieResult::kTransientError:
      retry_auth_backoff_.InformOfRequest(/*succeeded=*/false);
      LOG(WARNING) << "graduation: transient failure in webview auth";
      if (retry_auth_backoff_.failure_count() < kMaxRetries) {
        RetryAuth(std::move(callback));
      } else {
        LOG(ERROR) << "graduation: max retries reached on transient failure";
        CompleteAuth(std::move(callback), /*is_success=*/false);
      }
      break;
    case signin::SetAccountsInCookieResult::kPersistentError:
      LOG(ERROR) << "graduation: persistent failure in webview auth";
      CompleteAuth(std::move(callback), /*is_success=*/false);
      break;
  }
}

void WebviewAuthHandler::CompleteAuth(OnWebviewAuth callback, bool is_success) {
  cookie_loader_.reset();
  retry_auth_backoff_.Reset();
  std::move(callback).Run(is_success);
}

void WebviewAuthHandler::RetryAuth(OnWebviewAuth callback) {
  base::TimeDelta backoff_delay = retry_auth_backoff_.GetTimeUntilRelease();
  VLOG(1) << "graduation: webview auth retry in: "
          << backoff_delay.InMilliseconds() << " ms";
  retry_auth_timer_.Start(
      FROM_HERE, backoff_delay,
      base::BindOnce(&WebviewAuthHandler::AuthenticateWebview,
                     base::Unretained(this), std::move(callback)));
}

content::StoragePartition* WebviewAuthHandler::GetStoragePartition() {
  content::StoragePartition* partition =
      GraduationManager::Get()->GetStoragePartition(context_,
                                                    storage_partition_config_);
  CHECK(partition) << "graduation: invalid storage partition";
  return partition;
}

}  // namespace ash::graduation
