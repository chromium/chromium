// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/multi_profile_credentials_filter.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "components/autofill/core/browser/validation.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "google_apis/gaia/gaia_auth_util.h"

MultiProfileCredentialsFilter::MultiProfileCredentialsFilter(
    password_manager::PasswordManagerClient* client,
    DiceWebSigninInterceptor* dice_web_signin_interceptor)
    : password_manager::SyncCredentialsFilter(client),
      dice_web_signin_interceptor_(dice_web_signin_interceptor) {}

bool MultiProfileCredentialsFilter::ShouldSave(
    const password_manager::PasswordForm& form) const {
  if (!password_manager::SyncCredentialsFilter::ShouldSave(form)) {
    return false;
  }
  if (!dice_web_signin_interceptor_) {
    return true;  // This happens in incognito.
  }
  if (!password_manager::sync_util::IsGaiaCredentialPage(form.signon_realm)) {
    return true;
  }

  // Note: this function is only called for "Save" bubbles, but not for "Update"
  // bubbles.

  // Do not show password bubble if interception is initializing or already
  // shown on screen.
  if (dice_web_signin_interceptor_->is_interception_in_progress()) {
    return false;
  }

  std::string email =
      gaia::SanitizeEmail(base::UTF16ToUTF8(form.username_value));

  // Do not show password bubble for incomplete or invalid email address.
  if (!autofill::IsValidEmailAddress(base::UTF8ToUTF16(email))) {
    return false;
  }

  // On Gaia signin page, suppress the password save bubble if the multi profile
  // promo is shown, to avoid saving a password for an account that will be
  // moved to another profile. If the interception outcome is not available,
  // then signin interception is very likely, and the password bubble is
  // suppressed as well.
  std::optional<SigninInterceptionHeuristicOutcome> heuristic_outcome =
      dice_web_signin_interceptor_->GetHeuristicOutcome(
          // At this time, it's not possible to know whether the account is new
          // (whether it's a reauth). To be conservative and avoid showing both
          // bubbles, assume that it is new.
          /*is_new_account=*/true,
          /*is_sync_signin=*/false, email);
  if (!heuristic_outcome ||
      SigninInterceptionHeuristicOutcomeIsSuccess(*heuristic_outcome)) {
    return false;
  }

  return true;
}
