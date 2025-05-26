// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_user_status_fetcher.h"

#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
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
      sender: "Chrome Gemini"
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
          "upon an Enterprise primary account sign-in or Chrome launch with "
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
      }
    }
   )");

constexpr char kUserStatus[] = "user_status";
constexpr char kUpdatedAt[] = "updated_at";
constexpr char kAccountId[] = "account_id";
}  // namespace

namespace glic {

GlicUserStatusFetcher::GlicUserStatusFetcher(Profile* profile,
                                             base::RepeatingClosure callback)
    : profile_(profile), callback_(std::move(callback)) {
  endpoint_ = GURL(features::kGlicUserStatusUrl.Get());
  oauth2_scope_ = features::kGeminiOAuth2Scope.Get();

  // if it has passed default delay after last update time,
  // run immediately. Otherwise, wait until the default delay.
  base::Time next_update_time;
  if (auto cached_user_status =
          GlicUserStatusFetcher::GetCachedUserStatus(profile);
      cached_user_status.has_value()) {
    // If next_update_time turns out to be later than now + 24hrs, we cap it at
    // now + 24hrs.
    next_update_time = std::min(cached_user_status->last_updated +
                                    features::kGlicUserStatusRequestDelay.Get(),
                                base::Time::Now() + base::Hours(24));
  }

  // If when Chrome starts and user has already signed-in, we also send a
  // request to check user status.
  if (next_update_time <= base::Time::Now()) {
    UpdateUserStatus();
  } else {
    ScheduleUserStatusUpdate(next_update_time - base::Time::Now());
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
      last_updated_default_value};
}

// static.
bool GlicUserStatusFetcher::IsDisabled(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    if (auto cached_user_status =
            GlicUserStatusFetcher::GetCachedUserStatus(profile);
        cached_user_status.has_value()) {
      if (cached_user_status->user_status_code ==
              UserStatusCode::DISABLED_BY_ADMIN ||
          cached_user_status->user_status_code ==
              UserStatusCode::DISABLED_OTHER) {
        return true;
      }
    }
  }
  return false;
}

bool GlicUserStatusFetcher::IsEnterpriseAccount() {
  if (profile_->GetPrefs()->GetInteger(::prefs::kGeminiSettings) ==
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled)) {
    return false;
  }

  // Only update user status for enterprise accounts.
  policy::ManagementService* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile_);
  // It's possible in theory though very rare that IsAccountManaged returns
  // false because policy fetching is not complete. In this case we check if an
  // RPC was sent for this account previously.
  return (management_service && management_service->IsAccountManaged()) ||
         GlicUserStatusFetcher::GetCachedUserStatus(profile_).has_value();
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
  if (refresh_status_timer_.IsRunning()) {
    refresh_status_timer_.Stop();
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    return;
  }

  // only send user status request when primary account exists and refresh
  // token is available.
  if (identity_manager->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    // Only send the RPC for enterprise account.
    if (!IsEnterpriseAccount()) {
      return;
    }
    FetchNow();
  } else {
    is_user_status_waiting_for_refresh_token_ = true;
  }
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
      FROM_HERE, scheduled_time,
      base::BindOnce(&GlicUserStatusFetcher::UpdateUserStatus,
                     base::Unretained(this)));
}

std::optional<signin::GaiaIdHash>
GlicUserStatusFetcher::GetGaiaIdHashForPrimaryAccount(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);

  if (!identity_manager) {
    return std::nullopt;
  }

  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return std::nullopt;
  }
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  return signin::GaiaIdHash::FromGaiaId(primary_account.gaia);
}

void GlicUserStatusFetcher::FetchNow() {
  // Cancel any ongoing fetch. This will do nothing if the request is already
  // finished.
  CancelUserStatusUpdateIfNeeded();

  // schedule next run regardless.
  ScheduleUserStatusUpdate(features::kGlicUserStatusRequestDelay.Get());

  auto account_info = GetGaiaIdHashForPrimaryAccount(profile_);

  if (!account_info.has_value()) {
    return;
  }

  std::vector<std::string> scopes;
  scopes.push_back(oauth2_scope_);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

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
      request_sender_.get(), endpoint_,
      base::BindOnce(&GlicUserStatusFetcher::ProcessResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     account_info.value().ToBase64()));
  cancel_closure_ =
      request_sender_->StartRequestWithAuthRetry(std::move(request));
}

void GlicUserStatusFetcher::CancelUserStatusUpdateIfNeeded() {
  if (cancel_closure_) {
    std::move(cancel_closure_).Run();
  }
}

void GlicUserStatusFetcher::ProcessResponse(const std::string& account_id_hash,
                                            UserStatusCode result_code) {
  // We don't overwrite the previous GlicUserStatus when UserStatusCode is
  // SERVER_UNAVAILABLE.
  if (result_code != UserStatusCode::SERVER_UNAVAILABLE) {
    base::Value::Dict data;
    data.Set(kAccountId, account_id_hash);
    data.Set(kUserStatus, result_code);
    data.Set(kUpdatedAt, base::Time::Now().InSecondsFSinceUnixEpoch());
    profile_->GetPrefs()->SetDict(glic::prefs::kGlicUserStatus,
                                  std::move(data));
  }
  if (callback_) {
    callback_.Run();
  }
}

}  // namespace glic
