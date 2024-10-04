// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_browser_signin_detector.h"

#include <cstddef>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

const char kIdentityProviderDomain[] = "google.com";

DIPSBrowserSigninDetector::DIPSBrowserSigninDetector(
    base::PassKey<DIPSBrowserSigninDetectorFactory>,
    DIPSService* dips_service,
    signin::IdentityManager* identity_manager)
    : dips_service_(dips_service), identity_manager_(identity_manager) {
  CHECK(dips_service_);
  if (!identity_manager_) {
    // If there's no identity manager, then don't try to observe it.
    return;
  }
  scoped_observation_.Observe(identity_manager_.get());

  // No need to check the cookie jar if there's no presence of a primary
  // account.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  auto accounts = identity_manager_->GetAccountsInCookieJar();

  // Check the cookie jar in case the identity manager updated the accounts
  // before the observation kicked-in.
  for (const auto& account : accounts.GetPotentiallyInvalidSignedInAccounts()) {
    RecordInteractionsIfRelevant(
        identity_manager_->FindExtendedAccountInfoByAccountId(account.id));
  }
}

DIPSBrowserSigninDetector::~DIPSBrowserSigninDetector() = default;

void DIPSBrowserSigninDetector::Shutdown() {
  scoped_observation_.Reset();
  dips_service_ = nullptr;
  identity_manager_ = nullptr;
}

// Evaluates whether an information is relevant for DIPS. An info is relevant if
// its core infos are non empty and the |hosted_domain| info is provided.
bool IsInfoRelevant(const AccountInfo& info) {
  // Note: extended infos such as |hosted_domain| are filled asynchronously.
  return !info.CoreAccountInfo::IsEmpty() && !info.hosted_domain.empty();
}

void DIPSBrowserSigninDetector::RecordInteractionsIfRelevant(
    const AccountInfo& info) {
  if (!IsInfoRelevant(info)) {
    return;
  }

  // Record an interaction for `kIdentityProviderDomain`.
  // Note: All accounts in the identity manager are GAIA accounts. Thus,
  // non-enterprise accounts (ex. "gmail.com", "yahoo.com") will be treated as
  // having an interaction with `kIdentityProviderDomain`.
  dips_service_->RecordBrowserSignIn(kIdentityProviderDomain);

  // Skip handled cases.
  if (info.hosted_domain == kNoHostedDomainFound ||
      info.hosted_domain == kIdentityProviderDomain) {
    return;
  }

  // Record an interaction for the |info.host_domain| of all enterprise
  // accounts.
  dips_service_->RecordBrowserSignIn(info.hosted_domain);
}

void DIPSBrowserSigninDetector::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  RecordInteractionsIfRelevant(info);
}
