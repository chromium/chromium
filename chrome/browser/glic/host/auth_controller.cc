// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/auth_controller.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/android/glic_navigation_utils_android.h"
#endif

namespace glic {

namespace {

bool IsAutomationEnabled() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(::switches::kGlicAutomation);
}

void RecordCheckAuthBeforeLoadOutcome(CheckAuthBeforeLoadOutcome outcome) {
  base::UmaHistogramEnumeration("Glic.Auth.CheckAuthBeforeLoadOutcome",
                                outcome);
}

}  // namespace

bool IsPrimaryAccountGoogleInternal(signin::IdentityManager& signin_manager) {
  return gaia::IsGoogleInternalAccountEmail(gaia::CanonicalizeEmail(
      signin_manager.GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email));
}

AuthController::AuthController(Profile* profile,
                               signin::IdentityManager* identity_manager)
    : profile_(profile),
      identity_manager_(identity_manager),
      cookie_synchronizer_(
          std::make_unique<GlicCookieSynchronizer>(profile, identity_manager)),
      observation_(this) {
  observation_.Observe(identity_manager_);
  auto primary_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          primary_account) != GoogleServiceAuthError::AuthErrorNone()) {
    profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync,
                                     true);
  }
}

AuthController::~AuthController() = default;

void AuthController::CheckAuthBeforeLoad(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback),
      mojom::PrepareForClientResult::kErrorResyncingCookies);
  // If automation is enabled skip auth check.
  if (IsAutomationEnabled()) {
    RecordCheckAuthBeforeLoadOutcome(
        CheckAuthBeforeLoadOutcome::kAutomationSkipped);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  mojom::PrepareForClientResult::kSuccess));
    return;
  }

  if (GetTokenState() == TokenState::kRequiresSignIn) {
    RecordCheckAuthBeforeLoadOutcome(
        CheckAuthBeforeLoadOutcome::kRequiresSignIn);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       mojom::PrepareForClientResult::kRequiresSignIn));
    return;
  }

  if (base::FeatureList::IsEnabled(features::kGlicCookieSyncOnTokenChange)) {
    bool needs_sync =
        profile_->GetPrefs()->GetBoolean(prefs::kGlicPartitionNeedsCookieSync);
    if (!needs_sync) {
      RecordCheckAuthBeforeLoadOutcome(
          CheckAuthBeforeLoadOutcome::kTokenChangeNoSyncNeeded);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    mojom::PrepareForClientResult::kSuccess));
      return;
    }
  } else if (base::FeatureList::IsEnabled(
                 features::kGlicSkipCookieSyncOnOpen)) {
    RecordCheckAuthBeforeLoadOutcome(CheckAuthBeforeLoadOutcome::kSkipOnOpen);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  mojom::PrepareForClientResult::kSuccess));
    return;
  }

  RecordCheckAuthBeforeLoadOutcome(CheckAuthBeforeLoadOutcome::kSyncAttempted);
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
      if (base::FeatureList::IsEnabled(
              features::kGlicCookieSyncOnTokenChange)) {
        ForceSyncCookies(base::DoNothing());
      }
      break;
    // Ignore until primary account is set.
    case signin::PrimaryAccountChangeEvent::Type::kNone:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      break;
  }
}

void AuthController::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    return;
  }
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync, true);
}

void AuthController::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kGlicCookieSyncOnTokenChange)) {
    profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync,
                                     true);
    ForceSyncCookies(base::DoNothing());
  }
}

void AuthController::ForceSyncCookies(
    base::OnceCallback<void(bool)> callback) {
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(base::BindOnce(
      &AuthController::CookieSyncDone, GetWeakPtr(), std::move(callback)));
}

void AuthController::OnClientError() {
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync, true);
}

void AuthController::OnClientTransientError(
    mojo_base::mojom::AbslStatusCode status_code) {
  switch (status_code) {
    case mojo_base::mojom::AbslStatusCode::kUnauthenticated:
    case mojo_base::mojom::AbslStatusCode::kInternal:
      profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync,
                                       true);
      break;
    default:
      break;
  }
}

bool AuthController::NeedsSyncForTesting() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kGlicPartitionNeedsCookieSync);
}

void AuthController::CookieSyncDone(base::OnceCallback<void(bool)> callback,
                                    bool sync_success) {
  if (sync_success) {
    profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync,
                                     false);
  }
  std::move(callback).Run(sync_success);
}

void AuthController::ShowReauthForAccount(content::WebContents* web_contents) {
  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
#if !BUILDFLAG(IS_ANDROID)
  signin_ui_util::ShowReauthForAccount(
      profile_, primary_account_info.email,
      signin_metrics::AccessPoint::kGlicLaunchButton);
#else
  glic::ShowSignIn(profile_, web_contents);
#endif
}

void AuthController::CookieSyncBeforeLoadDone(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback,
    bool sync_success) {
  if (sync_success) {
    profile_->GetPrefs()->SetBoolean(prefs::kGlicPartitionNeedsCookieSync,
                                     false);
    std::move(callback).Run(mojom::PrepareForClientResult::kSuccess);
  } else {
    std::move(callback).Run(
        GetTokenState() == TokenState::kRequiresSignIn
            ? mojom::PrepareForClientResult::kRequiresSignIn
            : mojom::PrepareForClientResult::kErrorResyncingCookies);
  }
}

void AuthController::SetCookieSynchronizerForTesting(
    std::unique_ptr<GlicCookieSynchronizer> synchronizer) {
  cookie_synchronizer_ = std::move(synchronizer);
}

}  // namespace glic
