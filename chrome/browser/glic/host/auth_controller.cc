// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/auth_controller.h"

#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"

namespace glic {

namespace {
// TODO(crbug.com/391378260): Once the web client can request to sync auth
// reliably, we should not need any timeout.
base::TimeDelta kCookieSyncRepeatTime = base::Minutes(5);

bool IsAutomationEnabled() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(::switches::kGlicAutomation);
}

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

bool AuthController::CheckAuthBeforeShowSync(base::OnceClosure after_signin) {
  if (IsAutomationEnabled()) {
    return true;
  }
  switch (GetTokenState()) {
    case TokenState::kRequiresSignIn:
      ShowReauthForAccount(std::move(after_signin));
      return false;
    case TokenState::kUnknownError:
    case TokenState::kOk:
    default:
      return true;
  }
}

void AuthController::CheckAuthBeforeLoad(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback) {
  // If automation is enabled skip auth check.
  if (IsAutomationEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  mojom::PrepareForClientResult::kSuccess));
    return;
  }

  if (GetTokenState() == TokenState::kRequiresSignIn) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       mojom::PrepareForClientResult::kRequiresSignIn));
    return;
  }
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(
      base::BindOnce(&AuthController::CookieSyncBeforeLoadDone, GetWeakPtr(),
                     std::move(callback)));
}

AuthController::TokenState AuthController::GetTokenState() const {
  // If automation is enabled skip auth check.
  if (IsAutomationEnabled()) {
    return TokenState::kOk;
  }

  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  // If the user is signed-out, Glic shouldn't be running. Return an error
  // to avoid crashing if sign-out happens while Glic is loading.
  if (account_id.empty()) {
    return TokenState::kUnknownError;
  }

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id)) {
    return TokenState::kRequiresSignIn;
  }
  return TokenState::kOk;
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
  if (after_signin_callback_ &&
      after_signin_callback_expiration_time_ > base::TimeTicks::Now()) {
    if (GetTokenState() == TokenState::kOk) {
      std::move(after_signin_callback_).Run();
    }
  }
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
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

void AuthController::ShowReauthForAccount(base::OnceClosure after_signin) {
  after_signin_callback_ = std::move(after_signin);
  // TODO(crbug.com/396500584): Check what timeout is appropriate.
  after_signin_callback_expiration_time_ =
      base::TimeTicks::Now() + base::Minutes(5);
  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  signin_ui_util::ShowReauthForAccount(
      profile_, primary_account_info.email,
      signin_metrics::AccessPoint::kGlicLaunchButton);
}

void AuthController::OnGlicWindowOpened() {
  after_signin_callback_.Reset();
}

bool AuthController::RequiresSignIn() const {
  return GetTokenState() == TokenState::kRequiresSignIn;
}

void AuthController::CookieSyncBeforeLoadDone(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback,
    bool sync_success) {
  if (sync_success) {
    last_cookie_sync_time_ = base::TimeTicks::Now();
    std::move(callback).Run(mojom::PrepareForClientResult::kSuccess);
    return;
  }
  std::move(callback).Run(GetTokenState() == TokenState::kRequiresSignIn
                              ? mojom::PrepareForClientResult::kRequiresSignIn
                              : mojom::PrepareForClientResult::kUnknownError);
}

void AuthController::SetCookieSynchronizerForTesting(
    std::unique_ptr<GlicCookieSynchronizer> synchronizer) {
  cookie_synchronizer_ = std::move(synchronizer);
}

}  // namespace glic
