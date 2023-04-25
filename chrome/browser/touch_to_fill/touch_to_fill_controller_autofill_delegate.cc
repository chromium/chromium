// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller_autofill_delegate.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using autofill::mojom::SubmissionReadinessState;
using password_manager::PasswordManagerDriver;
using password_manager::UiCredential;

// Infers whether a form should be submitted based on the feature's state and
// the form's structure (submission_readiness).
bool ShouldTriggerSubmission(SubmissionReadinessState submission_readiness) {
  switch (submission_readiness) {
    case SubmissionReadinessState::kNoInformation:
    case SubmissionReadinessState::kError:
    case SubmissionReadinessState::kNoUsernameField:
    case SubmissionReadinessState::kNoPasswordField:
    case SubmissionReadinessState::kFieldBetweenUsernameAndPassword:
    case SubmissionReadinessState::kFieldAfterPasswordField:
      return false;

    case SubmissionReadinessState::kEmptyFields:
    case SubmissionReadinessState::kMoreThanTwoFields:
    case SubmissionReadinessState::kTwoFields:
      return true;
  }
}

// Returns whether there is at least one credential with a non-empty username.
bool ContainsNonEmptyUsername(
    const base::span<const UiCredential>& credentials) {
  return base::ranges::any_of(credentials, [](const UiCredential& credential) {
    return !credential.username().empty();
  });
}

}  // namespace

TouchToFillControllerAutofillDelegate::TouchToFillControllerAutofillDelegate(
    base::PassKey<TouchToFillControllerAutofillTest>,
    password_manager::PasswordManagerClient* password_client,
    scoped_refptr<device_reauth::DeviceAuthenticator> authenticator,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    autofill::mojom::SubmissionReadinessState submission_readiness)
    : password_client_(password_client),
      authenticator_(std::move(authenticator)),
      driver_(std::move(driver)),
      submission_readiness_(submission_readiness) {}

TouchToFillControllerAutofillDelegate::TouchToFillControllerAutofillDelegate(
    ChromePasswordManagerClient* password_client,
    scoped_refptr<device_reauth::DeviceAuthenticator> authenticator,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    autofill::mojom::SubmissionReadinessState submission_readiness)
    : password_client_(password_client),
      authenticator_(std::move(authenticator)),
      driver_(driver),
      submission_readiness_(submission_readiness),
      source_id_(password_client->web_contents()
                     ->GetPrimaryMainFrame()
                     ->GetPageUkmSourceId()) {}

TouchToFillControllerAutofillDelegate::
    ~TouchToFillControllerAutofillDelegate() {
  if (authenticator_) {
    // This is a noop if no auth triggered by Touch To Fill is in progress.
    authenticator_->Cancel(device_reauth::DeviceAuthRequester::kTouchToFill);
  }
}

void TouchToFillControllerAutofillDelegate::OnShow(
    base::span<const password_manager::UiCredential> credentials,
    base::span<password_manager::PasskeyCredential> passkey_credentials) {
  DCHECK(driver_);

  trigger_submission_ = ::ShouldTriggerSubmission(submission_readiness_) &&
                        ContainsNonEmptyUsername(credentials);

  base::UmaHistogramEnumeration(
      "PasswordManager.TouchToFill.SubmissionReadiness", submission_readiness_);
  ukm::builders::TouchToFill_SubmissionReadiness(source_id_)
      .SetSubmissionReadiness(static_cast<int64_t>(submission_readiness_))
      .Record(ukm::UkmRecorder::Get());

  base::UmaHistogramCounts100("PasswordManager.TouchToFill.NumCredentialsShown",
                              credentials.size() + passkey_credentials.size());
}

void TouchToFillControllerAutofillDelegate::OnCredentialSelected(
    const UiCredential& credential,
    base::OnceClosure action_complete) {
  if (!driver_) {
    return;
  }

  action_complete_ = std::move(action_complete);
  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedCredential))
      .Record(ukm::UkmRecorder::Get());
  if (!password_manager_util::CanUseBiometricAuth(authenticator_.get(),
                                                  password_client_)) {
    FillCredential(credential);
    return;
  }
  // `this` notifies the authenticator when it is destructed, resulting in
  // the callback being reset by the authenticator. Therefore, it is safe
  // to use base::Unretained.
  authenticator_->Authenticate(
      device_reauth::DeviceAuthRequester::kTouchToFill,
      base::BindOnce(&TouchToFillControllerAutofillDelegate::OnReauthCompleted,
                     base::Unretained(this), credential),
      /*use_last_valid_auth=*/true);
}

void TouchToFillControllerAutofillDelegate::OnPasskeyCredentialSelected(
    const password_manager::PasskeyCredential& credential,
    base::OnceClosure action_complete) {
  if (!driver_)
    return;

  password_client_->GetWebAuthnCredentialsDelegateForDriver(driver_.get())
      ->SelectPasskey(base::Base64Encode(credential.credential_id()));

  CleanUpDriverAndReportOutcome(TouchToFillOutcome::kPasskeyCredentialSelected,
                                /*show_virtual_keyboard=*/false);
  std::move(action_complete).Run();
}

void TouchToFillControllerAutofillDelegate::OnManagePasswordsSelected(
    bool passkeys_shown,
    base::OnceClosure action_complete) {
  if (!driver_)
    return;

  CleanUpDriverAndReportOutcome(TouchToFillOutcome::kManagePasswordsSelected,
                                /*show_virtual_keyboard=*/false);

  if (passkeys_shown) {
    // On Android there is no passkey management available in Chrome settings
    // password management. This will attempt to launch the GMS password
    // manager where passkeys can be seen.
    password_client_->NavigateToManagePasskeysPage(
        password_manager::ManagePasswordsReferrer::kTouchToFill);
  } else {
    password_client_->NavigateToManagePasswordsPage(
        password_manager::ManagePasswordsReferrer::kTouchToFill);
  }

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedManagePasswords))
      .Record(ukm::UkmRecorder::Get());
  std::move(action_complete).Run();
}

void TouchToFillControllerAutofillDelegate::OnDismiss(
    base::OnceClosure action_complete) {
  if (!driver_)
    return;

  CleanUpDriverAndReportOutcome(TouchToFillOutcome::kSheetDismissed,
                                /*show_virtual_keyboard=*/true);
  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kDismissed))
      .Record(ukm::UkmRecorder::Get());
  std::move(action_complete).Run();
}

const GURL& TouchToFillControllerAutofillDelegate::GetFrameUrl() {
  return driver_->GetLastCommittedURL();
}

bool TouchToFillControllerAutofillDelegate::ShouldTriggerSubmission() {
  return trigger_submission_;
}

gfx::NativeView TouchToFillControllerAutofillDelegate::GetNativeView() {
  // It is not a |ChromePasswordManagerClient| only in
  // TouchToFillControllerTest.
  return static_cast<ChromePasswordManagerClient*>(password_client_)
      ->web_contents()
      ->GetNativeView();
}

void TouchToFillControllerAutofillDelegate::OnReauthCompleted(
    UiCredential credential,
    bool auth_successful) {
  DCHECK(action_complete_);
  if (!driver_)
    return;

  if (!auth_successful) {
    CleanUpDriverAndReportOutcome(TouchToFillOutcome::kReauthenticationFailed,
                                  /*show_virtual_keyboard=*/true);
    std::move(action_complete_).Run();
    return;
  }

  FillCredential(credential);
}

void TouchToFillControllerAutofillDelegate::FillCredential(
    const UiCredential& credential) {
  DCHECK(action_complete_);
  DCHECK(driver_);

  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      credential.is_affiliation_based_match().value());
  driver_->TouchToFillClosed(ShowVirtualKeyboard(false));

  driver_->FillSuggestion(credential.username(), credential.password());

  trigger_submission_ &= !credential.username().empty();
  if (trigger_submission_) {
    // TODO(crbug.com/1283004): As auto-submission has been launched, measuring
    // the time between filling by TTF and submisionn is not crucial. Remove
    // this call, the method itself and the metrics if we are not going to use
    // all that for new launches, e.g. crbug.com/1393043.
    password_client_->StartSubmissionTrackingAfterTouchToFill(
        credential.username());
    driver_->TriggerFormSubmission();
  }
  driver_ = nullptr;

  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                TouchToFillOutcome::kCredentialFilled);
  std::move(action_complete_).Run();
}

void TouchToFillControllerAutofillDelegate::CleanUpDriverAndReportOutcome(
    TouchToFillOutcome outcome,
    bool show_virtual_keyboard) {
  std::exchange(driver_, nullptr)
      ->TouchToFillClosed(ShowVirtualKeyboard(show_virtual_keyboard));
  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome", outcome);
}
