// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"

namespace metrics {

DesktopProfileSessionDurationsService::DesktopProfileSessionDurationsService(
    browser_sync::ProfileSyncService* sync_service,
    identity::IdentityManager* identity_manager,
    GaiaCookieManagerService* cookie_manager,
    DesktopSessionDurationTracker* tracker)
    : sync_service_(sync_service),
      identity_manager_(identity_manager),
      sync_observer_(this),
      identity_manager_observer_(this),
      gaia_cookie_observer_(this),
      session_duration_observer_(this) {
  gaia_cookie_observer_.Add(cookie_manager);
  session_duration_observer_.Add(tracker);

  if (tracker->in_session()) {
    // The session was started before this service was created. Let's start
    // tracking now.
    OnSessionStarted(base::TimeTicks::Now());
  }

  // sync_service can be null if sync is disabled by a command line flag.
  if (sync_service) {
    sync_observer_.Add(sync_service_);
  }
  identity_manager_observer_.Add(identity_manager_);

  // Since this is created after the profile itself is created, we need to
  // handle the initial state.
  HandleSyncAndAccountChange();

  // Check if we already know the signed in cookies. This will trigger a fetch
  // if we don't have them yet.
  std::vector<gaia::ListedAccount> signed_in;
  std::vector<gaia::ListedAccount> signed_out;
  if (cookie_manager->ListAccounts(&signed_in, &signed_out,
                                   "durations_metrics")) {
    OnGaiaAccountsInCookieUpdated(signed_in, signed_out,
                                  GoogleServiceAuthError());
  }

  DVLOG(1) << "Ready to track Session.TotalDuration metrics";
}

DesktopProfileSessionDurationsService::
    ~DesktopProfileSessionDurationsService() = default;

void DesktopProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  DVLOG(1) << "Session start";
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
  sync_account_session_timer_ = std::make_unique<base::ElapsedTimer>();
}

namespace {
base::TimeDelta SubtractInactiveTime(base::TimeDelta total_length,
                                     base::TimeDelta inactive_time) {
  // Substract any time the user was inactive from our session length. If this
  // ends up giving the session negative length, which can happen if the feature
  // state changed after the user became inactive, log the length as 0.
  base::TimeDelta session_length = total_length - inactive_time;
  if (session_length < base::TimeDelta()) {
    session_length = base::TimeDelta();
  }
  return session_length;
}
}  // namespace

void DesktopProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length) {
  DVLOG(1) << "Session end";

  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }

  base::TimeDelta inactivity_at_session_end =
      total_session_timer_->Elapsed() - session_length;
  LogSigninDuration(SubtractInactiveTime(signin_session_timer_->Elapsed(),
                                         inactivity_at_session_end));
  LogSyncAndAccountDuration(SubtractInactiveTime(
      sync_account_session_timer_->Elapsed(), inactivity_at_session_end));
  total_session_timer_.reset();
  signin_session_timer_.reset();
  sync_account_session_timer_.reset();
}

void DesktopProfileSessionDurationsService::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Cookie state change. in: " << accounts.size()
           << " out: " << signed_out_accounts.size()
           << " err: " << error.ToString();

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Return early if there's an error. This should only happen if there's an
    // actual error getting the account list. If there are any auth errors with
    // the tokens, those accounts will be moved to signed_out_accounts instead.
    return;
  }

  if (accounts.empty()) {
    // No signed in account.
    if (signin_status_ == FeatureState::ON && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    signin_status_ = FeatureState::OFF;
  } else {
    // There is a signed in account.
    if (signin_status_ == FeatureState::OFF && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    signin_status_ = FeatureState::ON;
  }
}

void DesktopProfileSessionDurationsService::OnStateChanged(
    syncer::SyncService* sync) {
  DVLOG(1) << "Sync state change";
  HandleSyncAndAccountChange();
}

void DesktopProfileSessionDurationsService::OnRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info,
    bool is_valid) {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

void DesktopProfileSessionDurationsService::OnRefreshTokenRemovedForAccount(
    const std::string& account_id) {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

void DesktopProfileSessionDurationsService::OnRefreshTokensLoaded() {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

bool DesktopProfileSessionDurationsService::ShouldLogUpdate(
    FeatureState new_sync_status,
    FeatureState new_account_status) {
  bool status_change = (new_sync_status != sync_status_ ||
                        new_account_status != account_status_);
  bool was_unknown = sync_status_ == FeatureState::UNKNOWN ||
                     account_status_ == FeatureState::UNKNOWN;
  return sync_account_session_timer_ && status_change && !was_unknown;
}

void DesktopProfileSessionDurationsService::UpdateSyncAndAccountStatus(
    FeatureState new_sync_status,
    FeatureState new_account_status) {
  if (ShouldLogUpdate(new_sync_status, new_account_status)) {
    LogSyncAndAccountDuration(sync_account_session_timer_->Elapsed());
    sync_account_session_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  sync_status_ = new_sync_status;
  account_status_ = new_account_status;
}

void DesktopProfileSessionDurationsService::HandleSyncAndAccountChange() {
  // If sync is off, we can tell whether the user is signed in by just checking
  // if the token service has accounts, because the reconcilor will take care of
  // removing accounts in error state from that list.
  FeatureState non_sync_account_status =
      identity_manager_->GetAccountsWithRefreshTokens().empty()
          ? FeatureState::OFF
          : FeatureState::ON;
  if (sync_service_ && sync_service_->CanSyncFeatureStart()) {
    // Sync has potential to turn on, or get into account error state.
    if (sync_service_->GetAuthError().state() ==
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
      // Sync is enabled, but we have an account issue.
      UpdateSyncAndAccountStatus(FeatureState::ON, FeatureState::OFF);
    } else if (sync_service_->IsSyncFeatureActive() &&
               sync_service_->GetLastCycleSnapshot().is_initialized()) {
      // Sync is on and running, we must have an account too.
      UpdateSyncAndAccountStatus(FeatureState::ON, FeatureState::ON);
    } else {
      // We don't know yet if sync is going to work.
      // At least update the signin status, so that if we never learn
      // what the sync state is, we know the signin state.
      account_status_ = non_sync_account_status;
    }
  } else {
    // We know for sure that sync is off, so we just need to find out about the
    // account status.
    UpdateSyncAndAccountStatus(FeatureState::OFF, non_sync_account_status);
  }
}

void DesktopProfileSessionDurationsService::LogSigninDuration(
    base::TimeDelta session_length) {
  switch (signin_status_) {
    case FeatureState::ON:
      DVLOG(1) << "Logging Session.TotalDuration.WithAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.WithAccount",
                               session_length);
      break;
    case FeatureState::OFF:
    // Since the feature wasn't working for the user if we didn't know its
    // state, log the status as off.
    case FeatureState::UNKNOWN:
      DVLOG(1) << "Logging Session.TotalDuration.WithoutAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.WithoutAccount",
                               session_length);
  }
}

void DesktopProfileSessionDurationsService::LogSyncAndAccountDuration(
    base::TimeDelta session_length) {
  // TODO(feuunk): Distinguish between being NotOptedInToSync and
  // OptedInToSyncPaused.
  if (sync_status_ == FeatureState::ON) {
    if (account_status_ == FeatureState::ON) {
      DVLOG(1) << "Logging Session.TotalDuration.OptedInToSyncWithAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.OptedInToSyncWithAccount",
                               session_length);
    } else {
      DVLOG(1)
          << "Logging Session.TotalDuration.OptedInToSyncWithoutAccount of "
          << session_length;
      UMA_HISTOGRAM_LONG_TIMES(
          "Session.TotalDuration.OptedInToSyncWithoutAccount", session_length);
    }
  } else {
    if (account_status_ == FeatureState::ON) {
      DVLOG(1)
          << "Logging Session.TotalDuration.NotOptedInToSyncWithAccount of "
          << session_length;
      UMA_HISTOGRAM_LONG_TIMES(
          "Session.TotalDuration.NotOptedInToSyncWithAccount", session_length);
    } else {
      DVLOG(1)
          << "Logging Session.TotalDuration.NotOptedInToSyncWithoutAccount of "
          << session_length;
      UMA_HISTOGRAM_LONG_TIMES(
          "Session.TotalDuration.NotOptedInToSyncWithoutAccount",
          session_length);
    }
  }
}

void DesktopProfileSessionDurationsService::Shutdown() {
  session_duration_observer_.RemoveAll();
  gaia_cookie_observer_.RemoveAll();
  sync_observer_.RemoveAll();
  identity_manager_observer_.RemoveAll();
}

}  // namespace metrics
