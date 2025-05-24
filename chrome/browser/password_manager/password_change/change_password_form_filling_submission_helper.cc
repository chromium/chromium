// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        content::WebContents* web_contents,
        ModelQualityLogsUploader* logs_uploader,
        base::OnceCallback<void(bool)> callback)
    : web_contents_(web_contents->GetWeakPtr()),
      callback_(std::move(callback)),
      logs_uploader_(logs_uploader) {}

ChangePasswordFormFillingSubmissionHelper::
    ~ChangePasswordFormFillingSubmissionHelper() = default;

void ChangePasswordFormFillingSubmissionHelper::FillChangePasswordForm(
    password_manager::PasswordFormManager* form_manager,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  CHECK(form_manager);
  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  form_manager_ = form_manager->Clone();
  // PostTask is required because if the form is filled immediately the fields
  // might be cleared by PasswordAutofillAgent if there were no credentials to
  // fill during SendFillInformationToRenderer call.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChangePasswordFormFillingSubmissionHelper::TriggerFilling,
                     weak_ptr_factory_.GetWeakPtr(),
                     *form_manager->GetParsedObservedForm(),
                     form_manager->GetDriver(), old_password, new_password));

  // Proceed with verifying password on timeout, in case submission was not
  // captured.
  timeout_timer_.Start(
      FROM_HERE,
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout,
      this,
      &ChangePasswordFormFillingSubmissionHelper::
          OnSubmissionDetectedOrTimeout);
}

void ChangePasswordFormFillingSubmissionHelper::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (!submission_verifier_) {
    return;
  }
  if (!web_contents_) {
    return;
  }
  if (web_contents != web_contents_.get()) {
    return;
  }
  if (std::exchange(submission_detected_, true)) {
    return;
  }
  timeout_timer_.FireNow();
}

void ChangePasswordFormFillingSubmissionHelper::SavePassword(
    const std::u16string& username) {
  CHECK(!callback_);
  CHECK(form_manager_);
  form_manager_->OnUpdateUsernameFromPrompt(username);
  form_manager_->Save();
}

GURL ChangePasswordFormFillingSubmissionHelper::GetURL() const {
  CHECK(form_manager_);
  return form_manager_->GetURL();
}

void ChangePasswordFormFillingSubmissionHelper::TriggerFilling(
    const password_manager::PasswordForm& form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  CHECK(form_manager_);
  if (!driver) {
    return;
  }

  driver->FillChangePasswordForm(
      form.password_element_renderer_id, form.new_password_element_renderer_id,
      form.confirmation_password_element_renderer_id, old_password,
      new_password,
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::ChangePasswordFormFilled,
          weak_ptr_factory_.GetWeakPtr(), driver,
          form.new_password_element_renderer_id));

  form_manager_->PresaveGeneratedPassword(form.form_data, new_password);
}

void ChangePasswordFormFillingSubmissionHelper::ChangePasswordFormFilled(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    autofill::FieldRendererId field_id,
    const std::optional<autofill::FormData>& submitted_form) {
  if (!driver) {
    return;
  }

  if (!submitted_form) {
    // TODO(crbug.com/398754700): Change password form disappeared, consider
    // searching for change-pwd form again.
    return;
  }

  form_manager_->ProvisionallySave(
      submitted_form.value(), form_manager_->GetDriver().get(),
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          password_manager::kMaxSingleUsernameFieldsToStore));
  driver->SubmitFormWithEnter(
      field_id, base::BindOnce(
                    &ChangePasswordFormFillingSubmissionHelper::OnFormSubmitted,
                    weak_ptr_factory_.GetWeakPtr(), driver));
}

void ChangePasswordFormFillingSubmissionHelper::OnFormSubmitted(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    bool success) {
  if (!success) {
    // TODO(crbug.com/407487665): Attempt to submit change password form by
    // looking for a submit button.
    return;
  }
  submission_verifier_ = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents_.get(), logs_uploader_);
}

void ChangePasswordFormFillingSubmissionHelper::
    OnSubmissionDetectedOrTimeout() {
  if (!submission_verifier_) {
    CHECK(callback_);
    std::move(callback_).Run(false);
    return;
  }

  base::UmaHistogramBoolean(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically",
      submission_detected_);

  submission_verifier_->CheckSubmissionOutcome(std::move(callback_));
}
