// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_id_token_decoder.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

const base::TimeDelta kRefreshAdvancedProtectionDelay =
    base::TimeDelta::FromDays(1);
const base::TimeDelta kRetryDelay = base::TimeDelta::FromMinutes(5);
const base::TimeDelta kMinimumRefreshDelay = base::TimeDelta::FromMinutes(1);
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AdvancedProtectionStatusManager
////////////////////////////////////////////////////////////////////////////////
AdvancedProtectionStatusManager::AdvancedProtectionStatusManager(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : AdvancedProtectionStatusManager(pref_service,
                                      identity_manager,
                                      kMinimumRefreshDelay) {}

void AdvancedProtectionStatusManager::Initialize() {
  SubscribeToSigninEvents();
}

void AdvancedProtectionStatusManager::MaybeRefreshOnStartUp() {
  // Retrieves advanced protection service status from primary account's info.
  CoreAccountInfo core_info =
      identity_manager_->GetUnconsentedPrimaryAccountInfo();
  if (core_info.account_id.empty())
    return;

  is_under_advanced_protection_ = core_info.is_under_advanced_protection;

  if (pref_service_->HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs)) {
    last_refreshed_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(pref_service_->GetInt64(
            prefs::kAdvancedProtectionLastRefreshInUs)));
    if (is_under_advanced_protection_)
      ScheduleNextRefresh();
  } else {
    // User's advanced protection status is unknown, refresh in
    // |minimum_delay_|.
    timer_.Start(
        FROM_HERE, minimum_delay_, this,
        &AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus);
  }
}

void AdvancedProtectionStatusManager::Shutdown() {
  CancelFutureRefresh();
  UnsubscribeFromSigninEvents();
}

AdvancedProtectionStatusManager::~AdvancedProtectionStatusManager() {}

void AdvancedProtectionStatusManager::SubscribeToSigninEvents() {
  identity_manager_->AddObserver(this);
}

void AdvancedProtectionStatusManager::UnsubscribeFromSigninEvents() {
  identity_manager_->RemoveObserver(this);
}

bool AdvancedProtectionStatusManager::IsRefreshScheduled() {
  return timer_.IsRunning();
}

void AdvancedProtectionStatusManager::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  // Ignore update if the updated account is not the primary account.
  if (!IsUnconsentedPrimaryAccount(info))
    return;

  if (info.is_under_advanced_protection) {
    // User just enrolled into advanced protection.
    OnAdvancedProtectionEnabled();
  } else {
    // User's no longer in advanced protection.
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // If user signed out primary account, cancel refresh.
  CoreAccountId unconsented_primary_account_id =
      GetUnconsentedPrimaryAccountId();
  if (!unconsented_primary_account_id.empty() &&
      unconsented_primary_account_id == info.account_id) {
    is_under_advanced_protection_ = false;
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& account_info) {
  // TODO(crbug.com/926204): remove IdentityManager ensures that primary account
  // always has valid refresh token when it is set.
  if (account_info.is_under_advanced_protection)
    OnAdvancedProtectionEnabled();
  else
    OnAdvancedProtectionDisabled();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionEnabled() {
  is_under_advanced_protection_ = true;
  UpdateLastRefreshTime();
  ScheduleNextRefresh();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionDisabled() {
  is_under_advanced_protection_ = false;
  UpdateLastRefreshTime();
  CancelFutureRefresh();
}

void AdvancedProtectionStatusManager::OnAccessTokenFetchComplete(
    CoreAccountId account_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  DCHECK(access_token_fetcher_);

  if (is_under_advanced_protection_) {
    // Those already known to be under AP should have much lower error rates.
    UMA_HISTOGRAM_ENUMERATION(
        "SafeBrowsing.AdvancedProtection.APTokenFetchStatus", error.state(),
        GoogleServiceAuthError::NUM_STATES);
  }

  if (error.state() == GoogleServiceAuthError::NONE)
    OnGetIDToken(account_id, token_info.id_token);

  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.AdvancedProtection.TokenFetchStatus",
                            error.state(), GoogleServiceAuthError::NUM_STATES);

  access_token_fetcher_.reset();

  // If failure is transient, we'll retry in 5 minutes.
  if (error.IsTransientError()) {
    timer_.Start(
        FROM_HERE, kRetryDelay, this,
        &AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus);
  }
}

void AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CoreAccountId unconsented_primary_account_id =
      GetUnconsentedPrimaryAccountId();
  if (!identity_manager_ || unconsented_primary_account_id.empty())
    return;

  // If there's already a request going on, do nothing.
  if (access_token_fetcher_)
    return;

  // Refresh OAuth access token.
  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "advanced_protection_status_manager", identity_manager_, scopes,
          base::BindOnce(
              &AdvancedProtectionStatusManager::OnAccessTokenFetchComplete,
              base::Unretained(this), unconsented_primary_account_id),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void AdvancedProtectionStatusManager::ScheduleNextRefresh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelFutureRefresh();
  base::Time now = base::Time::Now();
  const base::TimeDelta time_since_last_refresh =
      now > last_refreshed_ ? now - last_refreshed_ : base::TimeDelta::Max();
  base::TimeDelta delay =
      time_since_last_refresh > kRefreshAdvancedProtectionDelay
          ? minimum_delay_
          : std::max(minimum_delay_,
                     kRefreshAdvancedProtectionDelay - time_since_last_refresh);
  timer_.Start(
      FROM_HERE, delay, this,
      &AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus);
}
void AdvancedProtectionStatusManager::CancelFutureRefresh() {
  if (timer_.IsRunning())
    timer_.Stop();
}

void AdvancedProtectionStatusManager::UpdateLastRefreshTime() {
  last_refreshed_ = base::Time::Now();
  pref_service_->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_refreshed_.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

bool AdvancedProtectionStatusManager::RequestsAdvancedProtectionVerdicts() {
  return is_under_advanced_protection();
}

bool AdvancedProtectionStatusManager::IsUnconsentedPrimaryAccount(
    const CoreAccountInfo& account_info) {
  return !account_info.account_id.empty() &&
         account_info.account_id == GetUnconsentedPrimaryAccountId();
}

void AdvancedProtectionStatusManager::OnGetIDToken(
    const CoreAccountId& account_id,
    const std::string& id_token) {
  // Skips if the ID token is not for the primary account. Or user is no longer
  // signed in.
  CoreAccountId unconsented_primary_account_id =
      GetUnconsentedPrimaryAccountId();
  if (unconsented_primary_account_id.empty() ||
      account_id != unconsented_primary_account_id)
    return;

  gaia::TokenServiceFlags service_flags = gaia::ParseServiceFlags(id_token);

  // If there's a change in advanced protection status, updates account info.
  // This also triggers |OnAccountUpdated()|.
  if (is_under_advanced_protection_ !=
      service_flags.is_under_advanced_protection) {
    identity_manager_->GetAccountsMutator()->UpdateAccountInfo(
        GetUnconsentedPrimaryAccountId(), false,
        service_flags.is_under_advanced_protection);
  } else if (service_flags.is_under_advanced_protection) {
    OnAdvancedProtectionEnabled();
  } else {
    OnAdvancedProtectionDisabled();
  }
}

AdvancedProtectionStatusManager::AdvancedProtectionStatusManager(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    const base::TimeDelta& min_delay)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      is_under_advanced_protection_(false),
      minimum_delay_(min_delay) {
  DCHECK(identity_manager_);
  DCHECK(pref_service_);

  Initialize();
  MaybeRefreshOnStartUp();
}

CoreAccountId AdvancedProtectionStatusManager::GetUnconsentedPrimaryAccountId()
    const {
  return identity_manager_ ? identity_manager_->GetUnconsentedPrimaryAccountId()
                           : CoreAccountId();
}

}  // namespace safe_browsing
