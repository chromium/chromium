// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/auth_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_cookie_synchronizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"

namespace glic {

namespace {
// TODO(crbug.com/391378260): Once the web client can request to sync auth
// reliably, we should not need any timeout.
base::TimeDelta kCookieSyncRepeatTime = base::Minutes(5);
}  // namespace

AuthController::AuthController(Profile* profile,
                               signin::IdentityManager* identity_manager,
                               bool use_for_fre)
    : profile_(profile),
      identity_manager_(identity_manager),
      cookie_synchronizer_(
          std::make_unique<GlicCookieSynchronizer>(profile,
                                                   identity_manager,
                                                   use_for_fre)),
      observation_(this) {
  observation_.Observe(identity_manager_);
}

AuthController::~AuthController() = default;

void AuthController::CheckAuthBeforeLoad(
    base::OnceCallback<void(bool)> callback) {
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(base::BindOnce(
      &AuthController::CookieSyncDone, GetWeakPtr(), std::move(callback)));
}

void AuthController::CheckAuthBeforeShow(
    base::OnceCallback<void(BeforeShowResult)> callback) {
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  // If the user is signed-out, Glic shouldn't be running. Return an error
  // to avoid crashing if sign-out happens while Glic is loading.
  if (account_id.empty()) {
    std::move(callback).Run(BeforeShowResult::kSyncFailed);
    return;
  }

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id)) {
    // TODO(crbug.com/394115674): There should be some kind of transition to
    // make it clear the sign-in is for Glic.
    signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
        profile_, signin_metrics::AccessPoint::kGlicLaunchButton);
    std::move(callback).Run(kShowingReauthSigninPage);
  } else {
    SyncCookiesIfRequired(base::BindOnce([](bool success) {
                            return success ? BeforeShowResult::kReady
                                           : BeforeShowResult::kSyncFailed;
                          }).Then(std::move(callback)));
  }
}

void AuthController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      last_cookie_sync_time_ = std::nullopt;
      break;
    // Ignore until primary account is set.
    case signin::PrimaryAccountChangeEvent::Type::kNone:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      break;
  }
}

void AuthController::ForceSyncCookies(base::OnceCallback<void(bool)> callback) {
  last_cookie_sync_time_ = std::nullopt;
  SyncCookiesIfRequired(std::move(callback));
}

void AuthController::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    return;
  }
  last_cookie_sync_time_ = std::nullopt;
}

void AuthController::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    return;
  }
  last_cookie_sync_time_ = std::nullopt;
}

void AuthController::SyncCookiesIfRequired(
    base::OnceCallback<void(bool)> callback) {
  // If cookies were synced successfully and recently, don't do it again.
  if (last_cookie_sync_time_ &&
      base::TimeTicks::Now() - *last_cookie_sync_time_ <
          kCookieSyncRepeatTime) {
    std::move(callback).Run(true);
    return;
  }
  last_cookie_sync_time_ = std::nullopt;
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(base::BindOnce(
      &AuthController::CookieSyncDone, GetWeakPtr(), std::move(callback)));
}

void AuthController::CookieSyncDone(base::OnceCallback<void(bool)> callback,
                                    bool sync_success) {
  if (sync_success) {
    last_cookie_sync_time_ = base::TimeTicks::Now();
  }
  std::move(callback).Run(sync_success);
}

void AuthController::SetCookieSynchronizerForTesting(
    std::unique_ptr<GlicCookieSynchronizer> synchronizer) {
  cookie_synchronizer_ = std::move(synchronizer);
}

}  // namespace glic
