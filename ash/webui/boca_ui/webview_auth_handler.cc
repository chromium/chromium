// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/webview_auth_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "url/gurl.h"

namespace ash::boca {

WebviewAuthHandler::WebviewAuthHandler(
    std::unique_ptr<WebviewAuthDelegate> delegate,
    content::BrowserContext* context,
    const std::string& webview_host_name)
    : delegate_(std::move(delegate)), context_(context) {
  CHECK(delegate_);
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

void WebviewAuthHandler::AuthenticateWebview(
    AuthenticateWebviewCallback callback) {
  if (cookie_loader_) {
    cookie_loader_.reset();
  }

  signin::IdentityManager* identity_manager = delegate_->GetIdentityManager();
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
    AuthenticateWebviewCallback callback,
    signin::SetAccountsInCookieResult cookie_result) {
  cookie_loader_.reset();
  std::move(callback).Run(cookie_result ==
                          signin::SetAccountsInCookieResult::kSuccess);
}

content::StoragePartition* WebviewAuthHandler::GetStoragePartition() {
  return delegate_->GetStoragePartition(context_, storage_partition_config_);
}

}  // namespace ash::boca
