// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_id_token_decoder.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

const base::TimeDelta kRefreshAdvancedProtectionDelay = base::Days(1);
const base::TimeDelta kRetryDelay = base::Minutes(5);
const base::TimeDelta kMinimumRefreshDelay = base::Minutes(1);

const char kForceTreatUserAsAdvancedProtection[] =
    "safe-browsing-treat-user-as-advanced-protection";

void RecordUMA(AdvancedProtectionStatusManager::UmaEvent event) {
  base::UmaHistogramEnumeration("SafeBrowsing.AdvancedProtection.Enabled",
                                event);
}

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

void AdvancedProtectionStatusManager::AddObserver(
    StatusChangedObserver* observer) {
  observers_.AddObserver(observer);
}

void AdvancedProtectionStatusManager::RemoveObserver(
    StatusChangedObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AdvancedProtectionStatusManager::NotifyStatusChanged() {
  for (StatusChangedObserver& observer : observers_) {
    observer.OnAdvancedProtectionStatusChanged(is_under_advanced_protection_);
  }
}

void AdvancedProtectionStatusManager::MaybeRefreshOnStartUp() {
  // Retrieves advanced protection service status from primary account's info.
  CoreAccountInfo core_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_info.account_id.empty()) {
    return;
  }

  is_under_advanced_protection_ = core_info.is_under_advanced_protection;
  RecordUMA(is_under_advanced_protection_ ? UmaEvent::kEnabled
                                          : UmaEvent::kDisabled);
  NotifyStatusChanged();

  if (pref_service_->HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs)) {
    last_refreshed_ = base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
        pref_service_->GetInt64(prefs::kAdvancedProtectionLastRefreshInUs)));
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
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // TODO(crbug.com/41437854): remove IdentityManager ensures that primary
      // account always has valid refresh token when it is set.
      if (event.GetCurrentState().primary_account.is_under_advanced_protection)
        OnAdvancedProtectionEnabled();
      else
        OnAdvancedProtectionDisabled();
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      OnAdvancedProtectionDisabled();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionEnabled() {
  if (!is_under_advanced_protection_) {
    RecordUMA(UmaEvent::kEnabledAfterDisabled);
  }
  is_under_advanced_protection_ = true;
  UpdateLastRefreshTime();
  ScheduleNextRefresh();
  NotifyStatusChanged();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionDisabled() {
  if (is_under_advanced_protection_) {
    RecordUMA(UmaEvent::kDisabledAfterEnabled);
  }
  is_under_advanced_protection_ = false;
  UpdateLastRefreshTime();
  CancelFutureRefresh();
  NotifyStatusChanged();
}

void AdvancedProtectionStatusManager::OnAccessTokenFetchComplete(
    CoreAccountId account_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  DCHECK(access_token_fetcher_);

  if (error.state() == GoogleServiceAuthError::NONE)
    OnGetIDToken(account_id, token_info.id_token);

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

  // Refresh OAuth access token. This class isn't actually interested in the
  // access token itself, but the account's "under advanced protection" status
  // can be determined from the "service flags" contained in the response.
  // Note that the (quite powerful) `kOAuth1LoginScope` is required for the
  // server to return the service flags.
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "advanced_protection_status_manager", identity_manager_, scopes,
          base::BindOnce(
              &AdvancedProtectionStatusManager::OnAccessTokenFetchComplete,
              base::Unretained(this), unconsented_primary_account_id),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
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

bool AdvancedProtectionStatusManager::IsUnderAdvancedProtection() const {
  if (!pref_service_->GetBoolean(prefs::kAdvancedProtectionAllowed))
    return false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kForceTreatUserAsAdvancedProtection))
    return true;

  return is_under_advanced_protection_;
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
        GetUnconsentedPrimaryAccountId(),
        /*is_child_account=*/signin::Tribool::kUnknown,
        service_flags.is_under_advanced_protection ? signin::Tribool::kTrue
                                                   : signin::Tribool::kFalse);
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
      minimum_delay_(min_delay) {
  DCHECK(identity_manager_);
  DCHECK(pref_service_);

  Initialize();
  MaybeRefreshOnStartUp();
}

CoreAccountId AdvancedProtectionStatusManager::GetUnconsentedPrimaryAccountId()
    const {
  return identity_manager_ ? identity_manager_->GetPrimaryAccountId(
                                 signin::ConsentLevel::kSignin)
                           : CoreAccountId();
}

void AdvancedProtectionStatusManager::SetAdvancedProtectionStatusForTesting(
    bool enrolled) {
  is_under_advanced_protection_ = enrolled;
  NotifyStatusChanged();
}

}  // namespace safe_browsing
