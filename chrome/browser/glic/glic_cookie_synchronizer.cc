// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_cookie_synchronizer.h"

#include <iterator>
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

namespace glic {

GlicCookieSynchronizer::GlicCookieSynchronizer(
    content::BrowserContext* context,
    signin::IdentityManager* identity_manager)
    : context_(context), identity_manager_(identity_manager) {
  CHECK(context_);
  // This storage partition must match the partition attribute in
  // chrome/browser/resources/glic/glic.html: "persist:glicpart".
  storage_partition_config_ = content::StoragePartitionConfig::Create(
      context_, "glic",
      /*partition_name=*/"glicpart", /*in_memory=*/false);
}

GlicCookieSynchronizer::~GlicCookieSynchronizer() = default;

std::unique_ptr<GaiaAuthFetcher>
GlicCookieSynchronizer::CreateGaiaAuthFetcherForPartition(
    GaiaAuthConsumer* consumer,
    const gaia::GaiaSource& source) {
  return std::make_unique<GaiaAuthFetcher>(
      consumer, source,
      GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess());
}

network::mojom::CookieManager*
GlicCookieSynchronizer::GetCookieManagerForPartition() {
  return GetStoragePartition()->GetCookieManagerForBrowserProcess();
}

void GlicCookieSynchronizer::CopyCookiesToWebviewStoragePartition(
    OnWebviewAuth callback) {
  CHECK(!callback.is_null());
  callbacks_.push_back(std::move(callback));

  if (cookie_loader_) {
    // A request is in progress already.
    return;
  }

  if (!GetStoragePartition()) {
    DLOG(ERROR) << "glic webview storage partition does not exist";
    CompleteAuth(false);
    return;
  }

  // We only need primary account authentication in the webview.
  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (primary_account_id.empty()) {
    DLOG(ERROR) << "can't sync cookies, user not signed in";
    CompleteAuth(false);
    return;
  }

  cookie_loader_ =
      identity_manager_->GetAccountsCookieMutator()
          ->SetAccountsInCookieForPartition(
              this,
              {gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
               {primary_account_id}},
              gaia::GaiaSource::kChrome,
              base::BindOnce(&GlicCookieSynchronizer::OnAuthFinished,
                             GetWeakPtr()));
}

void GlicCookieSynchronizer::OnAuthFinished(
    signin::SetAccountsInCookieResult cookie_result) {
  switch (cookie_result) {
    case signin::SetAccountsInCookieResult::kSuccess:
      CompleteAuth(/*is_success=*/true);
      break;
    case signin::SetAccountsInCookieResult::kTransientError:
      CompleteAuth(/*is_success=*/false);
      break;
    case signin::SetAccountsInCookieResult::kPersistentError:
      CompleteAuth(/*is_success=*/false);
      break;
  }
}

void GlicCookieSynchronizer::CompleteAuth(bool is_success) {
  cookie_loader_.reset();
  std::vector<base::OnceCallback<void(bool)>> callbacks;
  std::swap(callbacks, callbacks_);

  for (auto i = std::make_move_iterator(callbacks.begin());
       i != std::make_move_iterator(callbacks.end()); ++i) {
    (*i).Run(is_success);
  }
}

content::StoragePartition* GlicCookieSynchronizer::GetStoragePartition() {
  content::StoragePartition* partition =
      context_->GetStoragePartition(storage_partition_config_);
  return partition;
}

}  // namespace glic
