// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace contextual_tasks {

namespace {

content::StoragePartitionConfig GetContextualTasksStoragePartitionConfig(
    content::BrowserContext* browser_context) {
  // This storage partition must match the partition attribute in
  // chrome/browser/resources/contextual_tasks/app.html.ts:
  // "persist:contextual-tasks".
  return content::StoragePartitionConfig::Create(
      browser_context, "contextual-tasks",
      /*partition_name=*/"contextual-tasks",
      /*in_memory=*/false);
}

}  // namespace

ContextualTasksCookieSynchronizer::ContextualTasksCookieSynchronizer(
    content::BrowserContext* context,
    signin::IdentityManager* identity_manager)
    : context_(context), identity_manager_(identity_manager) {
  CHECK(context_);
  observation_.Observe(identity_manager);
}

ContextualTasksCookieSynchronizer::~ContextualTasksCookieSynchronizer() =
    default;

std::unique_ptr<GaiaAuthFetcher>
ContextualTasksCookieSynchronizer::CreateGaiaAuthFetcherForPartition(
    GaiaAuthConsumer* consumer,
    const gaia::GaiaSource& source) {
  return std::make_unique<GaiaAuthFetcher>(
      consumer, source,
      GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess());
}

network::mojom::CookieManager*
ContextualTasksCookieSynchronizer::GetCookieManagerForPartition() {
  return GetStoragePartition()->GetCookieManagerForBrowserProcess();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
network::mojom::DeviceBoundSessionManager*
ContextualTasksCookieSynchronizer::GetDeviceBoundSessionManagerForPartition() {
  return GetStoragePartition()->GetDeviceBoundSessionManager();;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void ContextualTasksCookieSynchronizer::CopyCookiesToWebviewStoragePartition() {
  if (cookie_loader_) {
    // A request is in progress already.
    return;
  }

  // Set a timeout to avoid hanging if multilogin hangs.
  timeout_.Start(FROM_HERE, kCookieSyncDefaultTimeout,
                 base::BindOnce(&ContextualTasksCookieSynchronizer::OnTimeout,
                                base::Unretained(this)));

  if (!GetStoragePartition()) {
    CompleteAuth(false);
    return;
  }

  BeginCookieSync();
}

void ContextualTasksCookieSynchronizer::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  observation_.Reset();
  cookie_loader_.reset();
}

void ContextualTasksCookieSynchronizer::BeginCookieSync() {
  // We only need primary account authentication in the webview.
  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (primary_account_id.empty()) {
    CompleteAuth(false);
    return;
  }
  signin::MultiloginParameters parameters = {
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {primary_account_id}};

  // Use kChrome source with a suffix.
  gaia::GaiaSource source(gaia::GaiaSource::kChrome, "contextual-tasks");

  cookie_loader_ =
      identity_manager_->GetAccountsCookieMutator()
          ->SetAccountsInCookieForPartition(
              this, parameters, source,
              base::BindOnce(&ContextualTasksCookieSynchronizer::OnAuthFinished,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ContextualTasksCookieSynchronizer::OnAuthFinished(
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

void ContextualTasksCookieSynchronizer::OnTimeout() {
  // TODO(crbug.com/40284489): Add UMA metrics if needed.
  CompleteAuth(/*is_success=*/false);
}

void ContextualTasksCookieSynchronizer::CompleteAuth(bool is_success) {
  timeout_.Stop();
  cookie_loader_.reset();
}

content::StoragePartition*
ContextualTasksCookieSynchronizer::GetStoragePartition() {
  content::StoragePartition* partition = context_->GetStoragePartition(
      GetContextualTasksStoragePartitionConfig(context_));
  return partition;
}

}  // namespace contextual_tasks
