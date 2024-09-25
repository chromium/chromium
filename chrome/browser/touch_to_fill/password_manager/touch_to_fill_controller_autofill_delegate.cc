// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_autofill_delegate.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
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
    base::PassKey<class TouchToFillControllerAutofillTest>,
    password_manager::PasswordManagerClient* password_client,
    content::WebContents* web_contents,
    std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator,
    base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
        webauthn_delegate,
    std::unique_ptr<password_manager::PasswordCredentialFiller> filler,
    const password_manager::PasswordForm* form_to_fill,
    autofill::FieldRendererId focused_field_renderer_id,
    ShowHybridOption should_show_hybrid_option,
    ShowPasswordMigrationWarningCallback show_password_migration_warning,
    std::unique_ptr<PasswordAccessLossWarningBridge> data_loss_warning_bridge)
    : password_client_(password_client),
      web_contents_(web_contents),
      authenticator_(std::move(authenticator)),
      webauthn_delegate_(webauthn_delegate),
      filler_(std::move(filler)),
      form_to_fill_(form_to_fill),
      focused_field_renderer_id_(focused_field_renderer_id),
      should_show_hybrid_option_(should_show_hybrid_option),
      show_password_migration_warning_(
          std::move(show_password_migration_warning)),
      access_loss_warning_bridge_(std::move(data_loss_warning_bridge)) {}

TouchToFillControllerAutofillDelegate::TouchToFillControllerAutofillDelegate(
    ChromePasswordManagerClient* password_client,
    std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator,
    base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
        webauthn_delegate,
    std::unique_ptr<password_manager::PasswordCredentialFiller> filler,
    const password_manager::PasswordForm* form_to_fill,
    autofill::FieldRendererId focused_field_renderer_id,
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
      form_to_fill_(form_to_fill),
      focused_field_renderer_id_(focused_field_renderer_id),
      should_show_hybrid_option_(should_show_hybrid_option),
      show_password_migration_warning_(
          base::BindRepeating(&local_password_migration::ShowWarning)),
      access_loss_warning_bridge_(
          std::make_unique<PasswordAccessLossWarningBridgeImpl>()),
      source_id_(password_client->web_contents()
                     ->GetPrimaryMainFrame()
                     ->GetPageUkmSourceId()) {}

TouchToFillControllerAutofillDelegate::
    ~TouchToFillControllerAutofillDelegate() {
  if (authenticator_) {
    // This is a noop if no auth triggered by Touch To Fill is in progress.
    authenticator_->Cancel();
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
  if (!password_client_->IsReauthBeforeFillingRequired(authenticator_.get())) {
    FillCredential(credential);
    return;
  }
  // `this` notifies the authenticator when it is destructed, resulting in
  // the callback being reset by the authenticator. Therefore, it is safe
  // to use base::Unretained.
  authenticator_->AuthenticateWithMessage(
      u"",
      base::BindOnce(&TouchToFillControllerAutofillDelegate::OnReauthCompleted,
                     base::Unretained(this), credential));
}

void TouchToFillControllerAutofillDelegate::OnPasskeyCredentialSelected(
    const password_manager::PasskeyCredential& credential,
    base::OnceClosure action_complete) {
  if (!webauthn_delegate_) {
    return;
  }

  webauthn_delegate_->SelectPasskey(
      base::Base64Encode(credential.credential_id()), base::DoNothing());

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

  webauthn_delegate_->LaunchSecurityKeyOrHybridFlow();

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

void TouchToFillControllerAutofillDelegate::OnCredManDismissed(
    base::OnceClosure action_completed) {
  if (!filler_) {
    return;
  }
  filler_->Dismiss(ToShowVirtualKeyboard(false));
  std::move(action_completed).Run();
}

GURL TouchToFillControllerAutofillDelegate::GetFrameUrl() {
  CHECK(filler_);
  return filler_->GetFrameUrl();
}

bool TouchToFillControllerAutofillDelegate::ShouldShowTouchToFill() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordSuggestionBottomSheetV2)) {
    // For password suggesion bottom sheet version 1 all the conditions for
    // showing TTF are checked in the renderer (see
    // `PasswordAutofillAgent::TryToShowKeyboardReplacingSurface`). That's why
    // no additional checks are needed here.
    return true;
  }

  if (!form_to_fill_) {
    return false;
  }

  // Always show TTF for a current password field.
  if (focused_field_renderer_id_ ==
      form_to_fill_->password_element_renderer_id) {
    return true;
  }

  // Do not show TTF if it's not a current password and not a username field.
  if (focused_field_renderer_id_ !=
      form_to_fill_->username_element_renderer_id) {
    return false;
  }

  // Show TTF if the form has a current password field or if it's a single
  // username form.
  if (form_to_fill_->HasPasswordElement() ||
      form_to_fill_->IsSingleUsername()) {
    return true;
  }

  return false;
}

bool TouchToFillControllerAutofillDelegate::ShouldTriggerSubmission() {
  return filler_->ShouldTriggerSubmission();
}

bool TouchToFillControllerAutofillDelegate::ShouldShowHybridOption() {
  return should_show_hybrid_option_.value();
}

bool TouchToFillControllerAutofillDelegate::
    ShouldShowNoPasskeysSheetIfRequired() {
  return false;
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
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  filler_->UpdateTriggerSubmission(
      ShouldTriggerSubmission() &&
      !local_password_migration::ShouldShowWarning(profile) &&
      !access_loss_warning_bridge_->ShouldShowAccessLossNoticeSheet(
          prefs, /*called_at_startup=*/false));
  filler_->FillUsernameAndPassword(credential.username(),
                                   credential.password());
  if (access_loss_warning_bridge_->ShouldShowAccessLossNoticeSheet(
          prefs, /*called_at_startup=*/false)) {
    access_loss_warning_bridge_->MaybeShowAccessLossNoticeSheet(
        prefs, web_contents_->GetTopLevelNativeWindow(), profile,
        /*called_at_startup=*/false,
        password_manager_android_util::PasswordAccessLossWarningTriggers::
            kTouchToFill);
  } else {
    // TODO: crbug.com/340437382 - Deprecate the migration warning sheet.
    ShowPasswordMigrationWarningIfNeeded();
  }

  if (ShouldTriggerSubmission()) {
    password_client_->StartSubmissionTrackingAfterTouchToFill(
        credential.username());
  }

  CleanUpFillerAndReportOutcome(TouchToFillOutcome::kCredentialFilled,
                                /*show_virtual_keyboard=*/false);
  std::move(action_complete_).Run();
}

void TouchToFillControllerAutofillDelegate::CleanUpFillerAndReportOutcome(
    TouchToFillOutcome outcome,
    bool show_virtual_keyboard) {
  // User action is complete which indicates that the user has been informed
  // about any shared unnotified password. Report that to the client to mark
  // them as notified.
  // If the render frame host has been destroyed already, the url will be empty
  // and the credentials cache in the `password_client_` has been cleared. In
  // this case it's not possible to mark credentials as notitied. If the user
  // has properly interact with the touch to fill UI, the client would have been
  // notified properly.
  GURL url = GetFrameUrl();
  if (!url.is_empty()) {
    password_client_->MarkSharedCredentialsAsNotified(url);
  }
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
