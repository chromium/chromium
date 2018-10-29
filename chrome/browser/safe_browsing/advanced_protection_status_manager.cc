// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_id_token_decoder.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

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
    Profile* profile)
    : profile_(profile),
      identity_manager_(nullptr),
      access_token_fetcher_(nullptr),
      account_tracker_service_(nullptr),
      is_under_advanced_protection_(false),
      minimum_delay_(kMinimumRefreshDelay) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (profile_->IsOffTheRecord())
    return;

  Initialize();
  MaybeRefreshOnStartUp();
}

void AdvancedProtectionStatusManager::Initialize() {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  account_tracker_service_ =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  SubscribeToSigninEvents();
}

void AdvancedProtectionStatusManager::MaybeRefreshOnStartUp() {
  // Retrieves advanced protection service status from primary account's info.
  AccountInfo info = identity_manager_->GetPrimaryAccountInfo();
  if (info.account_id.empty())
    return;

  is_under_advanced_protection_ = info.is_under_advanced_protection;

  if (profile_->GetPrefs()->HasPrefPath(
          prefs::kAdvancedProtectionLastRefreshInUs)) {
    last_refreshed_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(profile_->GetPrefs()->GetInt64(
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
  AccountTrackerServiceFactory::GetForProfile(profile_)->AddObserver(this);
  IdentityManagerFactory::GetForProfile(profile_)->AddObserver(this);
}

void AdvancedProtectionStatusManager::UnsubscribeFromSigninEvents() {
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  IdentityManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
}

bool AdvancedProtectionStatusManager::IsRefreshScheduled() {
  return timer_.IsRunning();
}

void AdvancedProtectionStatusManager::OnAccountUpdated(
    const AccountInfo& info) {
  // Ignore update if |profile_| is in incognito mode, or the updated account
  // is not the primary account.
  if (profile_->IsOffTheRecord() || !IsPrimaryAccount(info))
    return;

  if (info.is_under_advanced_protection) {
    // User just enrolled into advanced protection.
    OnAdvancedProtectionEnabled();
  } else {
    // User's no longer in advanced protection.
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnAccountRemoved(
    const AccountInfo& info) {
  if (profile_->IsOffTheRecord())
    return;

  // If user signed out primary account, cancel refresh.
  std::string primary_account_id = GetPrimaryAccountId();
  if (!primary_account_id.empty() && primary_account_id == info.account_id) {
    is_under_advanced_protection_ = false;
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnPrimaryAccountSet(
    const AccountInfo& account_info) {
  if (account_info.is_under_advanced_protection)
    OnAdvancedProtectionEnabled();
}

void AdvancedProtectionStatusManager::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
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
    std::string account_id,
    GoogleServiceAuthError error,
    identity::AccessTokenInfo token_info) {
  DCHECK(access_token_fetcher_);

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string primary_account_id = GetPrimaryAccountId();
  if (!identity_manager_ || primary_account_id.empty())
    return;

  // If there's already a request going on, do nothing.
  if (access_token_fetcher_)
    return;

  // Refresh OAuth access token.
  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);

  access_token_fetcher_ =
      std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
          "advanced_protection_status_manager", identity_manager_, scopes,
          base::BindOnce(
              &AdvancedProtectionStatusManager::OnAccessTokenFetchComplete,
              base::Unretained(this), primary_account_id),
          identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void AdvancedProtectionStatusManager::ScheduleNextRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_refreshed_.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// static
bool AdvancedProtectionStatusManager::IsUnderAdvancedProtection(
    Profile* profile) {
  Profile* original_profile =
      profile->IsOffTheRecord() ? profile->GetOriginalProfile() : profile;

  return original_profile &&
         AdvancedProtectionStatusManagerFactory::GetInstance()
             ->GetForBrowserContext(
                 static_cast<content::BrowserContext*>(original_profile))
             ->is_under_advanced_protection();
}

bool AdvancedProtectionStatusManager::IsPrimaryAccount(
    const AccountInfo& account_info) {
  return !account_info.account_id.empty() &&
         account_info.account_id == GetPrimaryAccountId();
}

void AdvancedProtectionStatusManager::OnGetIDToken(
    const std::string& account_id,
    const std::string& id_token) {
  // Skips if the ID token is not for the primary account. Or user is no longer
  // signed in.
  std::string primary_account_id = GetPrimaryAccountId();
  if (primary_account_id.empty() || account_id != primary_account_id)
    return;

  gaia::TokenServiceFlags service_flags = gaia::ParseServiceFlags(id_token);

  // If there's a change in advanced protection status, updates account info.
  // This also triggers |OnAccountUpdated()|.
  if (is_under_advanced_protection_ !=
      service_flags.is_under_advanced_protection) {
    account_tracker_service_->SetIsAdvancedProtectionAccount(
        GetPrimaryAccountId(), service_flags.is_under_advanced_protection);
  } else if (service_flags.is_under_advanced_protection) {
    OnAdvancedProtectionEnabled();
  } else {
    OnAdvancedProtectionDisabled();
  }
}

AdvancedProtectionStatusManager::AdvancedProtectionStatusManager(
    Profile* profile,
    const base::TimeDelta& min_delay)
    : profile_(profile),
      identity_manager_(nullptr),
      account_tracker_service_(nullptr),
      is_under_advanced_protection_(false),
      minimum_delay_(min_delay) {
  if (profile_->IsOffTheRecord())
    return;
  Initialize();
  MaybeRefreshOnStartUp();
}

std::string AdvancedProtectionStatusManager::GetPrimaryAccountId() const {
  return identity_manager_ ? identity_manager_->GetPrimaryAccountId()
                           : std::string();
}

}  // namespace safe_browsing
