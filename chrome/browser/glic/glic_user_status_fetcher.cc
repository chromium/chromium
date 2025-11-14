// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_user_status_fetcher.h"

#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_request.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "url/gurl.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("gemini_user_status", R"(
    semantics {
      sender: "Gemini in Chrome"
      description:
          "To decide whether an Enterprise primary account can access the "
          "Gemini feature and see the Gemini UI surfaces (e.g. tab strip "
          "button), Chrome sends a request to the Gemini API which is a "
          "Google API to obtain the user status regarding Gemini. The "
          "request is sent when an Enterprise primary account is signed in "
          "or when Chrome is launched with an Enterprise primary account "
          "signed in and every 23 hours afterwards. The access token of "
          "the account is used for Oauth2 authentication."
      trigger:
          "Upon an Enterprise primary account sign-in or Chrome launch with "
          "a signed-in Enterprise primary account and periodically "
          "afterwards"
      data:
          "None, other than the access token of the primary account used for"
          "auth"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          owners: "//chrome/browser/glic/OWNERS"
        }
      }
      user_data {
        type: ACCESS_TOKEN
      }
      last_reviewed: "2025-04-22"
    }
    policy {
      cookies_allowed: NO
      setting: "This feature can be disabled via GeminiSettings."
      chrome_policy {
        GeminiSettings {
            GeminiSettings: 1
        }
        GenAiDefaultSettings {
          GenAiDefaultSettings: 2
        }
      }
    }
   )");
}  // namespace

namespace glic {

GlicUserStatusFetcher::GlicUserStatusFetcher(Profile* profile,
                                             base::RepeatingClosure callback)
    : profile_(profile), callback_(std::move(callback)) {
  endpoint_ = GURL(features::kGlicUserStatusUrl.Get());
  oauth2_scope_ = features::kGeminiOAuth2Scope.Get();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  identity_manager_observation_.Observe(identity_manager);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ::prefs::kGeminiSettings,
      base::BindRepeating(&GlicUserStatusFetcher::OnGeminiSettingsChanged,
                          base::Unretained(this)));
  cached_gemini_settings_value_ = glic::prefs::SettingsPolicyState{
      profile_->GetPrefs()->GetInteger(::prefs::kGeminiSettings)};

  // if it has passed default delay after last update time,
  // run immediately. Otherwise, wait until the default delay.
  const base::Time now = base::Time::Now();
  base::Time next_update_time;
  if (auto cached_user_status =
          GlicUserStatusFetcher::GetCachedUserStatus(profile);
      cached_user_status.has_value()) {
    // If next_update_time turns out to be later than now + 24hrs, we cap it at
    // now + 24hrs.
    next_update_time = std::min(cached_user_status->last_updated +
                                    features::kGlicUserStatusRequestDelay.Get(),
                                now + base::Hours(24));
  }

  // If when Chrome starts and user has already signed-in, we also send a
  // request to check user status.
  if (next_update_time <= now) {
    UpdateUserStatusAndScheduleNextRefresh();
  } else {
    ScheduleUserStatusUpdate(next_update_time - now);
  }
}

GlicUserStatusFetcher::~GlicUserStatusFetcher() = default;

// static.
std::optional<CachedUserStatus> GlicUserStatusFetcher::GetCachedUserStatus(
    Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return std::nullopt;
  }

  const base::Value::Dict& pref_dict =
      profile->GetPrefs()->GetDict(glic::prefs::kGlicUserStatus);

  if (pref_dict.empty()) {
    // Return nullptr if no previous status found.
    return std::nullopt;
  }

  std::optional<signin::GaiaIdHash> primary_account_hash =
      GetGaiaIdHashForPrimaryAccount(profile);
  if (const auto* stored_account_hash = pref_dict.FindString(kAccountId);
      !stored_account_hash || !primary_account_hash.has_value() ||
      *stored_account_hash != primary_account_hash->ToBase64()) {
    // Return nullptr if previous status is for a different account.
    return std::nullopt;
  }

  // Use the prior cached result.
  // The default value of last_updated is to make it expire.
  auto last_updated_default_value =
      pref_dict.FindDouble(kUpdatedAt).has_value()
          ? base::Time::FromSecondsSinceUnixEpoch(
                pref_dict.FindDouble(kUpdatedAt).value())
          : base::Time::Now() - features::kGlicUserStatusRequestDelay.Get() -
                base::Days(1);

  return CachedUserStatus{
      UserStatusCode(pref_dict.FindInt(kUserStatus).value_or(0)),
      pref_dict.FindBool(kIsEnterpriseAccountDataProtected).value_or(false),
      last_updated_default_value};
}

void GlicUserStatusFetcher::InvalidateCachedStatus() {
  profile_->GetPrefs()->ClearPref(glic::prefs::kGlicUserStatus);
}

void GlicUserStatusFetcher::UpdateUserStatusIfNeeded() {
  if (is_user_status_waiting_for_refresh_token_) {
    is_user_status_waiting_for_refresh_token_ = false;
    UpdateUserStatus();
  }
}

void GlicUserStatusFetcher::UpdateUserStatus() {
  // Ensure that assumptions about when we do or do not update the cached user
  // status are not broken.
  // LINT.IfChange(GlicCachedUserStatusScope)

  // If the admin has disabled Gemini, we don't need to send the request.
  if (profile_->GetPrefs()->GetInteger(::prefs::kGeminiSettings) ==
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled)) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    return;
  }

  // only send user status request when primary account exists and refresh
  // token is available.
  if (!identity_manager->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    is_user_status_waiting_for_refresh_token_ = true;
    return;
  }

  // We may need to wait for the account managed status to be determined.
  // `OnAccountManagedStatusFound` will call `UpdateUserStatus` when it is
  // ready.
  if (features::kGlicUserStatusEnterpriseCheckStrategy.Get() ==
          features::GlicEnterpriseCheckStrategy::kManaged &&
      account_managed_status_ ==
          signin::AccountManagedStatusFinderOutcome::kPending &&
      !account_managed_status_finder_) {
    account_managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            identity_manager,
            identity_manager->GetPrimaryAccountInfo(
                signin::ConsentLevel::kSignin),
            base::BindOnce(&GlicUserStatusFetcher::OnAccountManagedStatusFound,
                           weak_ptr_factory_.GetWeakPtr()));
    account_managed_status_ = account_managed_status_finder_->GetOutcome();
    if (account_managed_status_ ==
        signin::AccountManagedStatusFinderOutcome::kPending) {
      return;
    }
    account_managed_status_finder_.reset();
  }

  // Only send the RPC for enterprise account.
  bool is_managed;
  switch (features::kGlicUserStatusEnterpriseCheckStrategy.Get()) {
    case features::GlicEnterpriseCheckStrategy::kManaged:
      switch (account_managed_status_) {
        case signin::AccountManagedStatusFinderOutcome::kPending:
        case signin::AccountManagedStatusFinderOutcome::kConsumerGmail:
        case signin::AccountManagedStatusFinderOutcome::kConsumerWellKnown:
        case signin::AccountManagedStatusFinderOutcome::kConsumerNotWellKnown:
          is_managed = false;
          break;
        case signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom:
        case signin::AccountManagedStatusFinderOutcome::kEnterprise:
          is_managed = true;
          break;
        case signin::AccountManagedStatusFinderOutcome::kError:
        case signin::AccountManagedStatusFinderOutcome::kTimeout:
          // kError can only occur if the account is absent or has no refresh
          // token (both are checked above).
          // kTimeout can only occur if a finite timeout is used, which is not
          // the case.
          NOTREACHED(base::NotFatalUntil::M141)
              << "Unexpected account managed status: "
              << static_cast<int>(account_managed_status_);
          return;
      }
      break;
    case features::GlicEnterpriseCheckStrategy::kPolicy: {
      // Only update user status for enterprise accounts.
      policy::ManagementService* management_service =
          policy::ManagementServiceFactory::GetForProfile(profile_);
      // It's possible in theory though very rare that IsAccountManaged returns
      // false because policy fetching is not complete. In this case we check if
      // an RPC was sent for this account previously.
      is_managed =
          (management_service && management_service->IsAccountManaged()) ||
          GlicUserStatusFetcher::GetCachedUserStatus(profile_).has_value();
    }
  }
  if (!is_managed) {
    return;
  }

  // LINT.ThenChange(//chrome/browser/glic/public/glic_enabling.cc:GlicCachedUserStatusScope)

  // Cancel any ongoing fetch. This will do nothing if the request is already
  // finished.
  CancelUserStatusUpdateIfNeeded();

  auto gaia_id_hash = GetGaiaIdHashForPrimaryAccount(profile_);
  if (!gaia_id_hash.has_value()) {
    return;
  }

  auto callback = base::BindOnce(&GlicUserStatusFetcher::ProcessResponse,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 gaia_id_hash.value().ToBase64());

  if (fetch_override_for_test_) {
    fetch_override_for_test_.Run(std::move(callback));
    return;
  }

  request_sender_ = std::make_unique<google_apis::RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager,
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          profile_->GetURLLoaderFactory(),
          std::vector<std::string>{oauth2_scope_}),
      profile_->GetURLLoaderFactory(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/std::string(), kTrafficAnnotation);

  auto request = std::make_unique<GlicUserStatusRequest>(
      request_sender_.get(), profile_->GetVariationsClient(), endpoint_,
      std::move(callback));
  cancel_closure_ =
      request_sender_->StartRequestWithAuthRetry(std::move(request));
}

void GlicUserStatusFetcher::ScheduleUserStatusUpdate(
    base::TimeDelta time_to_next_update) {
  // Calculate a random offset for the delay. The range of the offset will be
  // from [1- jitter*random_multiplier, 1 + jitter*random_multiplier).
  // random_multiplier is in the range [-1,1) by (base::RandDouble() - 0.5)* 2.
  const double jitter_factor =
      1 + features::kGlicUserStatusRequestDelayJitter.Get() *
              (base::RandDouble() - 0.5) * 2;

  base::Time scheduled_time =
      base::Time::Now() + jitter_factor * time_to_next_update;

  refresh_status_timer_.Start(
      FROM_HERE, scheduled_time, this,
      &GlicUserStatusFetcher::UpdateUserStatusAndScheduleNextRefresh);
}

std::optional<signin::GaiaIdHash>
GlicUserStatusFetcher::GetGaiaIdHashForPrimaryAccount(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return std::nullopt;
  }

  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return signin::GaiaIdHash::FromGaiaId(primary_account.gaia);
}

void GlicUserStatusFetcher::CancelUserStatusUpdateIfNeeded() {
  if (cancel_closure_) {
    std::move(cancel_closure_).Run();
  }
}

void GlicUserStatusFetcher::UpdateUserStatusAndScheduleNextRefresh() {
  UpdateUserStatus();
  ScheduleUserStatusUpdate(features::kGlicUserStatusRequestDelay.Get());
}

void GlicUserStatusFetcher::UpdateUserStatusWithThrottling() {
  // If it has been less than the throttle interval since the last update,
  // don't update the user status immediately but schedule it to run after
  // the throttle interval.
  //
  // This limits the rate at which certain kinds of potentially frequent
  // events can cause requests to be sent, while still making the update
  // immediate most of the time.
  if (!throttle_timer_.IsRunning()) {
    UpdateUserStatus();

    const base::TimeDelta throttle_interval =
        features::kGlicUserStatusThrottleInterval.Get();
    base::OnceClosure update_cb = base::BindOnce(
        [](GlicUserStatusFetcher* self) {
          if (self->update_was_throttled_) {
            self->update_was_throttled_ = false;
            self->UpdateUserStatusWithThrottling();
          }
        },
        base::Unretained(this));
    throttle_timer_.Start(FROM_HERE, throttle_interval, std::move(update_cb));
  } else {
    update_was_throttled_ = true;
  }
}

void GlicUserStatusFetcher::OnAccountManagedStatusFound() {
  account_managed_status_ = account_managed_status_finder_->GetOutcome();
  account_managed_status_finder_.reset();
  UpdateUserStatus();
}

void GlicUserStatusFetcher::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      account_managed_status_ =
          signin::AccountManagedStatusFinderOutcome::kPending;
      account_managed_status_finder_.reset();
      UpdateUserStatus();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      account_managed_status_ =
          signin::AccountManagedStatusFinderOutcome::kPending;
      account_managed_status_finder_.reset();
      InvalidateCachedStatus();
      CancelUserStatusUpdateIfNeeded();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void GlicUserStatusFetcher::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // It happens that when the request is sent upon sign-in, the refresh token is
  // not available yet, the request would hence be cancelled. In such cases, we
  // resend the request when refresh token becomes available.
  if (is_user_status_waiting_for_refresh_token_) {
    is_user_status_waiting_for_refresh_token_ = false;
    UpdateUserStatus();
  }
}

void GlicUserStatusFetcher::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    CancelUserStatusUpdateIfNeeded();
  }
}

void GlicUserStatusFetcher::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  account_managed_status_finder_.reset();
  CancelUserStatusUpdateIfNeeded();
}

void GlicUserStatusFetcher::OnGeminiSettingsChanged() {
  // If the policy changed from either not set or Disabled to Enabled, trigger a
  // rpc fetch to update the possible user status change sooner.
  glic::prefs::SettingsPolicyState updated_gemini_settings_value{
      profile_->GetPrefs()->GetInteger(::prefs::kGeminiSettings)};
  if (cached_gemini_settings_value_ !=
          glic::prefs::SettingsPolicyState::kEnabled &&
      updated_gemini_settings_value ==
          glic::prefs::SettingsPolicyState::kEnabled) {
    UpdateUserStatus();
  }
  cached_gemini_settings_value_ = updated_gemini_settings_value;
}

void GlicUserStatusFetcher::ProcessResponse(
    const std::string& account_id_hash,
    const CachedUserStatus& user_status) {
  // We don't overwrite the previous GlicUserStatus when UserStatusCode is
  // SERVER_UNAVAILABLE.
  if (user_status.user_status_code != UserStatusCode::SERVER_UNAVAILABLE) {
    base::Value::Dict data;
    data.Set(kAccountId, account_id_hash);
    data.Set(kUserStatus, user_status.user_status_code);
    data.Set(kUpdatedAt, user_status.last_updated.InSecondsFSinceUnixEpoch());
    data.Set(kIsEnterpriseAccountDataProtected,
             user_status.is_enterprise_account_data_protected);

    profile_->GetPrefs()->SetDict(glic::prefs::kGlicUserStatus,
                                  std::move(data));

    // Given this was a successful refresh, we can delay the normal refresh.
    ScheduleUserStatusUpdate(features::kGlicUserStatusRequestDelay.Get());
  }
  if (callback_) {
    callback_.Run();
  }
}

}  // namespace glic
