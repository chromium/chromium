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
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

const char kIdentityProviderDomain[] = "google.com";

DIPSBrowserSigninDetector::DIPSBrowserSigninDetector(
    DIPSService* dips_service,
    signin::IdentityManager* identity_manager)
    : dips_service_(dips_service), identity_manager_(identity_manager) {
  scoped_observation_.Observe(identity_manager_.get());

  // No need to check the cookie jar if there's no presence of a primary
  // account.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  auto accounts = identity_manager_->GetAccountsInCookieJar();

  // Check the cookie jar in case the identity manager updated the accounts
  // before the observation kicked-in.
  for (const auto& account : accounts.signed_in_accounts) {
    RecordInteractionsIfRelevant(
        identity_manager_->FindExtendedAccountInfoByAccountId(account.id));
  }
}

DIPSBrowserSigninDetector::~DIPSBrowserSigninDetector() = default;

// Evaluates whether an information is relevant for DIPS. An info is relevant if
// its core infos are non empty and the |hosted_domain| info is provided.
bool IsInfoRelevant(const AccountInfo& info) {
  // Note: extended infos such as |hosted_domain| are filled asynchronously.
  return !info.CoreAccountInfo::IsEmpty() && !info.hosted_domain.empty();
}

// Provides a URL from the provided |domain|, adequate for DIPS storage API.
// Note: The provided |domain| are of type eTLD+1s.
GURL GetURL(const std::string domain) {
  return GURL(base::StrCat({"http://", domain}));
}

void DIPSBrowserSigninDetector::RecordInteractionsIfRelevant(
    const AccountInfo& info) {
  if (!IsInfoRelevant(info)) {
    return;
  }

  base::Time now = base::Time::Now();

  // Record an interaction for `kIdentityProviderDomain`.
  // Note: All accounts in the identity manager are GAIA accounts. Thus,
  // non-enterprise accounts (ex. "gmail.com", "yahoo.com") will be treated as
  // having an interaction with `kIdentityProviderDomain`.
  dips_service_->storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(GetURL(kIdentityProviderDomain), now,
                dips_service_->GetCookieMode());

  // Skip handled cases.
  if (info.hosted_domain == kNoHostedDomainFound ||
      info.hosted_domain == kIdentityProviderDomain) {
    return;
  }

  // Record an interaction for the |info.host_domain| of all enterprise
  // accounts.
  dips_service_->storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(GetURL(info.hosted_domain), now,
                dips_service_->GetCookieMode());
}

void DIPSBrowserSigninDetector::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  RecordInteractionsIfRelevant(info);
}
