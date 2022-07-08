// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_webauthn_credential.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/url_formatter/elide_url.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using autofill::mojom::SubmissionReadinessState;
using password_manager::PasswordManagerDriver;
using password_manager::UiCredential;

std::vector<UiCredential> SortCredentials(
    base::span<const UiCredential> credentials) {
  std::vector<UiCredential> result(credentials.begin(), credentials.end());
  // Sort `credentials` according to the following criteria:
  // 1) Prefer non-PSL matches over PSL matches.
  // 2) Prefer credentials that were used recently over others.
  //
  // Note: This ordering matches password_manager_util::FindBestMatches().
  base::ranges::sort(result, std::greater<>{}, [](const UiCredential& cred) {
    return std::make_pair(!cred.is_public_suffix_match(), cred.last_used());
  });

  return result;
}

// Infers whether a form should be submitted based on the feature's state and
// the form's structure (submission_readiness).
bool ShouldTriggerSubmission(SubmissionReadinessState submission_readiness,
                             bool* ready_for_submission) {
  bool submission_enabled = base::FeatureList::IsEnabled(
      password_manager::features::kTouchToFillPasswordSubmission);
  bool allow_non_conservative_heuristics =
      submission_enabled &&
      !base::GetFieldTrialParamByFeatureAsBool(
          password_manager::features::kTouchToFillPasswordSubmission,
          password_manager::features::
              kTouchToFillPasswordSubmissionWithConservativeHeuristics,
          false);

  switch (submission_readiness) {
    case SubmissionReadinessState::kNoInformation:
    case SubmissionReadinessState::kError:
    case SubmissionReadinessState::kNoUsernameField:
    case SubmissionReadinessState::kFieldBetweenUsernameAndPassword:
    case SubmissionReadinessState::kFieldAfterPasswordField:
      *ready_for_submission = false;
      return false;

    case SubmissionReadinessState::kEmptyFields:
    case SubmissionReadinessState::kMoreThanTwoFields:
      *ready_for_submission = true;
      return allow_non_conservative_heuristics;

    case SubmissionReadinessState::kTwoFields:
      *ready_for_submission = true;
      return submission_enabled;
  }
}

// Returns whether there is at least one credential with a non-empty username.
bool ContainsNonEmptyUsername(
    const base::span<const UiCredential>& credentials) {
  return std::any_of(credentials.begin(), credentials.end(),
                     [](const UiCredential& credential) {
                       return !credential.username().empty();
                     });
}

}  // namespace

TouchToFillController::TouchToFillController(
    base::PassKey<TouchToFillControllerTest>,
    password_manager::PasswordManagerClient* password_client,
    scoped_refptr<device_reauth::BiometricAuthenticator> authenticator)
    : password_client_(password_client),
      authenticator_(std::move(authenticator)) {}

TouchToFillController::TouchToFillController(
    ChromePasswordManagerClient* password_client,
    scoped_refptr<device_reauth::BiometricAuthenticator> authenticator)
    : password_client_(password_client),
      authenticator_(std::move(authenticator)),
      source_id_(password_client->web_contents()
                     ->GetPrimaryMainFrame()
                     ->GetPageUkmSourceId()) {}

TouchToFillController::~TouchToFillController() {
  if (authenticator_) {
    // This is a noop if no auth triggered by Touch To Fill is in progress.
    authenticator_->Cancel(device_reauth::BiometricAuthRequester::kTouchToFill);
  }
}

void TouchToFillController::Show(
    base::span<const UiCredential> credentials,
    base::span<TouchToFillWebAuthnCredential> webauthn_credentials,
    base::WeakPtr<PasswordManagerDriver> driver,
    SubmissionReadinessState submission_readiness) {
  DCHECK(!driver_ || driver_.get() == driver.get());
  driver_ = std::move(driver);

  trigger_submission_ =
      ShouldTriggerSubmission(submission_readiness, &ready_for_submission_) &&
      ContainsNonEmptyUsername(credentials);
  ready_for_submission_ &= ContainsNonEmptyUsername(credentials);

  base::UmaHistogramEnumeration(
      "PasswordManager.TouchToFill.SubmissionReadiness", submission_readiness);
  ukm::builders::TouchToFill_SubmissionReadiness(source_id_)
      .SetSubmissionReadiness(static_cast<int64_t>(submission_readiness))
      .Record(ukm::UkmRecorder::Get());

  base::UmaHistogramCounts100("PasswordManager.TouchToFill.NumCredentialsShown",
                              credentials.size() + webauthn_credentials.size());
  if (credentials.empty() && webauthn_credentials.empty()) {
    // Ideally this should never happen. However, in case we do end up invoking
    // Show() without credentials, we should not show Touch To Fill to the user
    // and treat this case as dismissal, in order to restore the soft keyboard.
    OnDismiss();
    return;
  }

  if (!view_)
    view_ = TouchToFillViewFactory::Create(this);

  const GURL& url = driver_->GetLastCommittedURL();
  // TODO(https://crbug.com/1318942): Currently WebAuthn credentials are not
  // displayed in any particular order, and always appear after password
  // credentials. This needs to be evaluated by UX.
  view_->Show(
      url,
      TouchToFillView::IsOriginSecure(
          network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url))),
      SortCredentials(credentials), webauthn_credentials, trigger_submission_);
}

void TouchToFillController::OnCredentialSelected(
    const UiCredential& credential) {
  view_.reset();
  if (!driver_)
    return;

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedCredential))
      .Record(ukm::UkmRecorder::Get());
  if (!password_manager_util::CanUseBiometricAuth(
          authenticator_.get(),
          device_reauth::BiometricAuthRequester::kTouchToFill)) {
    FillCredential(credential);
    return;
  }
  // `this` notifies the authenticator when it is destructed, resulting in
  // the callback being reset by the authenticator. Therefore, it is safe
  // to use base::Unretained.
  authenticator_->Authenticate(
      device_reauth::BiometricAuthRequester::kTouchToFill,
      base::BindOnce(&TouchToFillController::OnReauthCompleted,
                     base::Unretained(this), credential),
      /*use_last_valid_auth=*/true);
}

void TouchToFillController::OnWebAuthnCredentialSelected(
    const TouchToFillWebAuthnCredential& credential) {
  view_.reset();
  if (!driver_)
    return;

  CleanUpDriverAndReportOutcome(TouchToFillOutcome::kWebAuthnCredentialSelected,
                                /*show_virtual_keyboard=*/false);

  password_client_->GetWebAuthnCredentialsDelegate()->SelectWebAuthnCredential(
      credential.id().value());
}

void TouchToFillController::OnManagePasswordsSelected() {
  view_.reset();
  if (!driver_)
    return;

  CleanUpDriverAndReportOutcome(TouchToFillOutcome::kManagePasswordsSelected,
                                /*show_virtual_keyboard=*/false);

  password_client_->NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer::kTouchToFill);

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedManagePasswords))
      .Record(ukm::UkmRecorder::Get());
}

void TouchToFillController::OnDismiss() {
  view_.reset();
  if (!driver_)
    return;

  CleanUpDriverAndReportOutcome(TouchToFillOutcome::kSheetDismissed,
                                /*show_virtual_keyboard=*/true);
  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kDismissed))
      .Record(ukm::UkmRecorder::Get());
}

gfx::NativeView TouchToFillController::GetNativeView() {
  // It is not a |ChromePasswordManagerClient| only in
  // TouchToFillControllerTest.
  return static_cast<ChromePasswordManagerClient*>(password_client_)
      ->web_contents()
      ->GetNativeView();
}

void TouchToFillController::OnReauthCompleted(UiCredential credential,
                                              bool auth_successful) {
  if (!driver_)
    return;

  if (!auth_successful) {
    CleanUpDriverAndReportOutcome(TouchToFillOutcome::kReauthenticationFailed,
                                  /*show_virtual_keyboard=*/true);
    return;
  }

  FillCredential(credential);
}

void TouchToFillController::FillCredential(const UiCredential& credential) {
  DCHECK(driver_);

  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      credential.is_affiliation_based_match().value());
  driver_->TouchToFillClosed(ShowVirtualKeyboard(false));

  driver_->FillSuggestion(credential.username(), credential.password());

  trigger_submission_ &= !credential.username().empty();
  ready_for_submission_ &= !credential.username().empty();
  if (ready_for_submission_) {
    password_client_->StartSubmissionTrackingAfterTouchToFill(
        credential.username());
    if (trigger_submission_)
      driver_->TriggerFormSubmission();
  } else {
    DCHECK(!trigger_submission_) << "Form is not ready for submission. "
                                    "|trigger_submission_| cannot be true";
  }
  driver_ = nullptr;

  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                TouchToFillOutcome::kCredentialFilled);
}

void TouchToFillController::CleanUpDriverAndReportOutcome(
    TouchToFillOutcome outcome,
    bool show_virtual_keyboard) {
  std::exchange(driver_, nullptr)
      ->TouchToFillClosed(ShowVirtualKeyboard(show_virtual_keyboard));
  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome", outcome);
}
