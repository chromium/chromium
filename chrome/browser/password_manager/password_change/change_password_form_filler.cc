// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filler.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_logging_util.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/web_contents.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;
using password_change::LogBoolean;
using password_change::LogMessage;
using password_change::LogString;

constexpr optimization_guide::proto::PasswordChangeRequest::FlowStep
    kSubmitFormFlowStep = optimization_guide::proto::PasswordChangeRequest::
        FlowStep::PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP;

}  // namespace

ChangePasswordFormFiller::ChangePasswordFormFiller(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    ModelQualityLogsUploader* logs_uploader)
    : web_contents_(web_contents),
      client_(client),
      logs_uploader_(logs_uploader) {}

ChangePasswordFormFiller::~ChangePasswordFormFiller() = default;

void ChangePasswordFormFiller::FillForm(
    password_manager::PasswordFormManager* form_manager,
    const std::u16string& username,
    const std::u16string& login_password,
    const std::u16string& generated_password,
    base::OnceCallback<void(FillingResult)> callback) {
  CHECK(form_manager);
  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  username_ = username;
  login_password_ = login_password;
  generated_password_ = generated_password;
  callback_ = std::move(callback);

  const password_manager::PasswordForm& parsed_form =
      *form_manager->GetParsedObservedForm();

  form_manager_ = form_manager->Clone();

  if (logs_uploader_) {
    logs_uploader_->SetChangePasswordFormData(parsed_form);
  }

  const password_manager::StoredCredential* best_match =
      password_manager_util::FindCredentialByUsername(
          form_manager_->GetBestMatches(), username_);
  stored_password_ = best_match ? best_match->password_value : login_password_;

  TriggerFilling(parsed_form, form_manager->GetDriver());
}

void ChangePasswordFormFiller::TriggerFilling(
    const password_manager::PasswordForm& form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  CHECK(form_manager_);
  if (!driver) {
    OnFormFillingFailed();
    return;
  }

  observed_fields_.push_back(autofill::FieldGlobalId{
      form.form_data.host_frame(), form.new_password_element_renderer_id});

  LogString(client_,
            Logger::STRING_PASSWORD_CHANGE_CURRENT_PASSWORD_RENDERER_ID,
            base::NumberToString(form.password_element_renderer_id.value()));
  LogString(
      client_, Logger::STRING_PASSWORD_CHANGE_NEW_PASSWORD_RENDERER_ID,
      base::NumberToString(form.new_password_element_renderer_id.value()));
  LogString(client_,
            Logger::STRING_PASSWORD_CHANGE_CONFIRMATION_PASSWORD_RENDERER_ID,
            base::NumberToString(
                form.confirmation_password_element_renderer_id.value()));

  auto filling_callback =
      base::BindOnce(&ChangePasswordFormFiller::ChangePasswordFormFilled,
                     weak_ptr_factory_.GetWeakPtr());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &password_manager::PasswordManagerDriver::FillChangePasswordForm,
          driver, form.password_element_renderer_id,
          form.new_password_element_renderer_id,
          form.confirmation_password_element_renderer_id, login_password_,
          generated_password_, std::move(filling_callback)));

  password_manager::PasswordForm form_to_save(form);
  form_to_save.username_value = username_;
  form_to_save.password_value = stored_password_;
  password_manager::PasswordFormManager::PresaveGeneratedPasswordAsBackup(
      *form_manager_, form_to_save, generated_password_);
  form_manager_->GetFormFetcher()->Fetch();
}

void ChangePasswordFormFiller::ChangePasswordFormFilled(
    const std::optional<autofill::FormData>& submitted_form) {
  bool provisionally_saved = false;
  if (submitted_form) {
    provisionally_saved = form_manager_->ProvisionallySave(
        submitted_form.value(), form_manager_->GetDriver().get(),
        base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                       password_manager::PossibleUsernameData>(
            password_manager::kMaxSingleUsernameFieldsToStore));
  }

  LogBoolean(client_, Logger::STRING_PASSWORD_CHANGE_FORM_FILLING_RESULT,
             provisionally_saved);

  if (!provisionally_saved) {
    if (logs_uploader_) {
      logs_uploader_->SetFlowInterrupted(
          kSubmitFormFlowStep,
          ModelQualityLogsUploader::QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_FILLING_FAILED);
    }
    form_waiter_ =
        ChangePasswordFormWaiter::Builder(
            web_contents_, client_,
            base::BindOnce(&ChangePasswordFormFiller::OnChangePasswordFormFound,
                           weak_ptr_factory_.GetWeakPtr()))
            .SetTimeoutCallback(
                base::BindOnce(&ChangePasswordFormFiller::OnFormFillingFailed,
                               weak_ptr_factory_.GetWeakPtr()))
            .SetFieldsToIgnore(observed_fields_)
            .Build();
    return;
  }

  CHECK_EQ(form_manager_->GetPendingCredentials().password_value,
           generated_password_);
  form_manager_->UpdateBackupPassword(stored_password_);

  std::move(callback_).Run(std::move(form_manager_));
}

void ChangePasswordFormFiller::OnFormFillingFailed() {
  if (logs_uploader_) {
    logs_uploader_->SetFlowInterrupted(
        kSubmitFormFlowStep,
        ModelQualityLogsUploader::QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_FILLING_FAILED);
  }
  std::move(callback_).Run(
      base::unexpected(SubmissionError::kFailedToFillForm));
}

void ChangePasswordFormFiller::OnChangePasswordFormFound(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();
  CHECK(form_manager);

  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  LogMessage(client_, Logger::STRING_AUTOMATED_PASSWORD_CHANGE_FORM_FOUND);

  const password_manager::PasswordForm& parsed_form =
      *form_manager->GetParsedObservedForm();
  base::WeakPtr<password_manager::PasswordManagerDriver> driver =
      form_manager->GetDriver();

  form_manager_ = form_manager->Clone();
  TriggerFilling(parsed_form, driver);
}
