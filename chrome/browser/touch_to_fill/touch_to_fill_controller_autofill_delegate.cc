// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller_autofill_delegate.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/android/window_android.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace {

using ToShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ToShowVirtualKeyboard;
using password_manager::UiCredential;

// Returns whether there is at least one credential with a non-empty username.
bool ContainsNonEmptyUsername(
    const base::span<const UiCredential>& credentials) {
  return base::ranges::any_of(credentials, [](const UiCredential& credential) {
    return !credential.username().empty();
  });
}

}  // namespace

// No-op constructor for tests.
TouchToFillControllerAutofillDelegate::TouchToFillControllerAutofillDelegate(
    base::PassKey<TouchToFillControllerAutofillTest>,
    password_manager::PasswordManagerClient* password_client,
    content::WebContents* web_contents,
    scoped_refptr<device_reauth::DeviceAuthenticator> authenticator,
    base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
        webauthn_delegate,
    std::unique_ptr<password_manager::PasswordCredentialFiller> filler,
    ShowHybridOption should_show_hybrid_option,
    ShowPasswordMigrationWarningCallback show_password_migration_warning)
    : password_client_(password_client),
      web_contents_(web_contents),
      authenticator_(std::move(authenticator)),
      webauthn_delegate_(webauthn_delegate),
      filler_(std::move(filler)),
      show_password_migration_warning_(
          std::move(show_password_migration_warning)),
      should_show_hybrid_option_(should_show_hybrid_option) {}

TouchToFillControllerAutofillDelegate::TouchToFillControllerAutofillDelegate(
    ChromePasswordManagerClient* password_client,
    scoped_refptr<device_reauth::DeviceAuthenticator> authenticator,
    base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
        webauthn_delegate,
    std::unique_ptr<password_manager::PasswordCredentialFiller> filler,
    ShowHybridOption should_show_hybrid_option)
    : password_client_(password_client),
      // |TouchToFillControllerTest| doesn't provide an instance of
      // |ChromePasswordManagerClient|, so the test-only constructor should
      // be used there.
      web_contents_(static_cast<ChromePasswordManagerClient*>(password_client_)
                        ->web_contents()),
      authenticator_(std::move(authenticator)),
      webauthn_delegate_(webauthn_delegate),
      filler_(std::move(filler)),
      show_password_migration_warning_(
          base::BindRepeating(&local_password_migration::ShowWarning)),
      should_show_hybrid_option_(should_show_hybrid_option),
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
  CHECK(filler_);

  filler_->UpdateTriggerSubmission(ShouldTriggerSubmission() &&
                                   ContainsNonEmptyUsername(credentials));

  base::UmaHistogramEnumeration(
      "PasswordManager.TouchToFill.SubmissionReadiness",
      filler_->GetSubmissionReadinessState());
  ukm::builders::TouchToFill_SubmissionReadiness(source_id_)
      .SetSubmissionReadiness(
          static_cast<int64_t>(filler_->GetSubmissionReadinessState()))
      .Record(ukm::UkmRecorder::Get());
}

void TouchToFillControllerAutofillDelegate::OnCredentialSelected(
    const UiCredential& credential,
    base::OnceClosure action_complete) {
  if (!filler_) {
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
  if (!webauthn_delegate_) {
    return;
  }

  webauthn_delegate_->SelectPasskey(
      base::Base64Encode(credential.credential_id()));

  CleanUpFillerAndReportOutcome(TouchToFillOutcome::kPasskeyCredentialSelected,
                                /*show_virtual_keyboard=*/false);
  std::move(action_complete).Run();
}

void TouchToFillControllerAutofillDelegate::OnManagePasswordsSelected(
    bool passkeys_shown,
    base::OnceClosure action_complete) {
  if (!filler_) {
    return;
  }

  CleanUpFillerAndReportOutcome(TouchToFillOutcome::kManagePasswordsSelected,
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

void TouchToFillControllerAutofillDelegate::OnHybridSignInSelected(
    base::OnceClosure action_complete) {
  if (!webauthn_delegate_) {
    return;
  }

  webauthn_delegate_->ShowAndroidHybridSignIn();

  CleanUpFillerAndReportOutcome(TouchToFillOutcome::kHybridSignInSelected,
                                /*show_virtual_keyboard=*/false);

  std::move(action_complete).Run();
}

void TouchToFillControllerAutofillDelegate::OnDismiss(
    base::OnceClosure action_complete) {
  if (!filler_) {
    return;
  }

  CleanUpFillerAndReportOutcome(TouchToFillOutcome::kSheetDismissed,
                                /*show_virtual_keyboard=*/true);
  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kDismissed))
      .Record(ukm::UkmRecorder::Get());
  std::move(action_complete).Run();
}

const GURL& TouchToFillControllerAutofillDelegate::GetFrameUrl() {
  CHECK(filler_);
  return filler_->GetFrameUrl();
}

bool TouchToFillControllerAutofillDelegate::ShouldTriggerSubmission() {
  return filler_->ShouldTriggerSubmission();
}

bool TouchToFillControllerAutofillDelegate::ShouldShowHybridOption() {
  return should_show_hybrid_option_.value();
}

gfx::NativeView TouchToFillControllerAutofillDelegate::GetNativeView() {
  return web_contents_->GetNativeView();
}

void TouchToFillControllerAutofillDelegate::OnReauthCompleted(
    UiCredential credential,
    bool auth_successful) {
  CHECK(action_complete_);
  if (!filler_) {
    return;
  }

  if (!auth_successful) {
    CleanUpFillerAndReportOutcome(TouchToFillOutcome::kReauthenticationFailed,
                                  /*show_virtual_keyboard=*/true);
    std::move(action_complete_).Run();
    return;
  }

  FillCredential(credential);
}

void TouchToFillControllerAutofillDelegate::FillCredential(
    const UiCredential& credential) {
  CHECK(action_complete_);
  CHECK(filler_);

  // Do not trigger autosubmission if the password migration warning is being
  // shown because it interrupts the nomal workflow.
  filler_->UpdateTriggerSubmission(
      ShouldTriggerSubmission() &&
      !local_password_migration::ShouldShowWarning(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())));
  filler_->FillUsernameAndPassword(credential.username(),
                                   credential.password());
  ShowPasswordMigrationWarningIfNeeded();

  if (ShouldTriggerSubmission()) {
    password_client_->StartSubmissionTrackingAfterTouchToFill(
        credential.username());
  }

  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                TouchToFillOutcome::kCredentialFilled);
  std::move(action_complete_).Run();
}

void TouchToFillControllerAutofillDelegate::CleanUpFillerAndReportOutcome(
    TouchToFillOutcome outcome,
    bool show_virtual_keyboard) {
  filler_->Dismiss(ToShowVirtualKeyboard(show_virtual_keyboard));
  filler_.reset();
  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome", outcome);
}

void TouchToFillControllerAutofillDelegate::
    ShowPasswordMigrationWarningIfNeeded() {
  if (!local_password_migration::ShouldShowWarning(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))) {
    return;
  }
  show_password_migration_warning_.Run(
      web_contents_->GetTopLevelNativeWindow(),
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      password_manager::metrics_util::PasswordMigrationWarningTriggers::
          kTouchToFill);
}
