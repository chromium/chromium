// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/script_password_change_actuator.h"

#include <algorithm>
#include <utility>

#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

using ::password_manager::BrowserSavePasswordProgressLogger;
using FlowStep = ModelQualityLogsUploader::FlowStep;
using QualityStatus = ModelQualityLogsUploader::QualityStatus;
using SubmissionError =
    ChangePasswordFormFillingSubmissionHelper::SubmissionError;
using SubmissionVerificationResult =
    PasswordChangeSubmissionVerifier::SubmissionVerificationResult;

std::u16string GeneratePassword(
    const password_manager::PasswordForm& form,
    password_manager::PasswordGenerationFrameHelper* generation_helper,
    ModelQualityLogsUploader* logs_uploader) {
  auto iter = std::ranges::find(form.form_data.fields(),
                                form.new_password_element_renderer_id,
                                &autofill::FormFieldData::renderer_id);
  CHECK(iter != form.form_data.fields().end());

  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(form.form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureForField(*iter);

  autofill::PasswordRequirementsSpec spec =
      generation_helper->GetPasswordRequirementsSpec(
          form.url,
          autofill::password_generation::PasswordGenerationType::kAutomatic,
          form_signature, field_signature, iter->max_length());

  if (logs_uploader) {
    logs_uploader->SetPasswordRequirementsSpec(spec);
  }

  return autofill::GeneratePassword(spec);
}

std::unique_ptr<BrowserSavePasswordProgressLogger> GetLoggerIfAvailable(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<BrowserSavePasswordProgressLogger>(log_manager);
  }

  return nullptr;
}

std::unique_ptr<DetachedWebContents> CreateDetachedWebContents(
    Profile* profile,
    const GURL& url) {
  auto detached_web_contents =
      std::make_unique<DetachedWebContents>(profile, url);
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(
      detached_web_contents->GetWebContents());
  ChromePasswordManagerClient::CreateForWebContents(
      detached_web_contents->GetWebContents());

  ChromePasswordManagerClient::FromWebContents(
      detached_web_contents->GetWebContents())
      ->ApplyClientSidePredictionOverride();
  return detached_web_contents;
}

void AddPasswordChangeToTabStrip(
    content::WebContents* originator,
    std::unique_ptr<content::WebContents> password_change_contents) {
  CHECK(originator);
  auto* tab_interface = tabs::TabInterface::GetFromContents(originator);
  CHECK(tab_interface);
  TabStripModel* tab_strip_model =
      tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
  CHECK(tab_strip_model);
  tab_strip_model->AppendWebContents(std::move(password_change_contents),
                                     /*foreground=*/true);
}

}  // namespace

ScriptPasswordChangeActuator::ScriptPasswordChangeActuator(
    GURL change_password_url,
    password_manager::PasswordForm password_form_info,
    Profile* profile,
    ModelQualityLogsUploader* logs_uploader)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(password_form_info.username_value)),
      original_password_(password_form_info.password_value.value()),
      profile_(profile),
      logs_uploader_(logs_uploader) {}

ScriptPasswordChangeActuator::~ScriptPasswordChangeActuator() = default;

void ScriptPasswordChangeActuator::Start() {
  if (!hidden_executor_) {
    hidden_executor_ =
        CreateDetachedWebContents(profile_, change_password_url_);
  }
  CHECK(GetExecutorWebContents());
  auto* client =
      ChromePasswordManagerClient::FromWebContents(GetExecutorWebContents());

  navigation_observer_ = std::make_unique<CrossOriginNavigationObserver>(
      GetExecutorWebContents(), change_password_url_,
      AffiliationServiceFactory::GetForProfile(profile_),
      base::BindOnce(
          &ScriptPasswordChangeActuator::OnCrossOriginNavigationDetected,
          weak_ptr_factory_.GetWeakPtr()));
  form_finder_ = std::make_unique<ChangePasswordFormFinder>(
      GetExecutorWebContents(), client, logs_uploader_.get(),
      base::BindOnce(&ScriptPasswordChangeActuator::OnPasswordChangeFormFound,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &ScriptPasswordChangeActuator::OnPasswordChangeFormNotFound,
          weak_ptr_factory_.GetWeakPtr()));
  NotifyStateChanged(
      PasswordChangeActuator::State::kWaitingForChangePasswordForm);
}

void ScriptPasswordChangeActuator::Cancel() {
  ReportFlowInterruption(
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
  ResetInternalState();
}

content::WebContents* ScriptPasswordChangeActuator::GetExecutorWebContents()
    const {
  return hidden_executor_ ? hidden_executor_->GetWebContents() : nullptr;
}

void ScriptPasswordChangeActuator::OpenPasswordChangeTab(
    content::WebContents* originator) {
  if (form_manager_) {
    // If user decided to take over control when interruption is detected we
    // assume they will complete the password change process, thus the new
    // password must be saved.
    form_manager_->OnUpdateUsernameFromPrompt(username_);
    form_manager_->Save();
    form_manager_.reset();
  }

  content::WebContents* web_contents = GetExecutorWebContents();
  if (!web_contents) {
    web_contents = originator->OpenURL(
        content::OpenURLParams(GURL(change_password_url_), content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               /* is_renderer_initiated= */ false),
        /*navigation_handle_callback=*/{});
    CHECK(web_contents);
  } else if (hidden_executor_) {
    AddPasswordChangeToTabStrip(
        originator,
        DetachedWebContents::ReleaseWebContents(std::move(hidden_executor_)));
  }
}

std::u16string ScriptPasswordChangeActuator::GetGeneratedPassword() const {
  return generated_password_;
}

void ScriptPasswordChangeActuator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScriptPasswordChangeActuator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScriptPasswordChangeActuator::OnPasswordChangeFormFound(
    password_manager::PasswordFormManager* form_manager) {
  form_finder_.reset();

  CHECK(form_manager);
  generated_password_ =
      GeneratePassword(*form_manager->GetParsedObservedForm(),
                       form_manager->GetDriver()->GetPasswordGenerationHelper(),
                       logs_uploader_.get());

  CHECK(GetExecutorWebContents());
  CHECK(!form_submission_helper_);
  form_submission_helper_ =
      std::make_unique<ChangePasswordFormFillingSubmissionHelper>(
          GetExecutorWebContents(),
          ChromePasswordManagerClient::FromWebContents(
              GetExecutorWebContents()),
          logs_uploader_.get(),
          base::BindOnce(&ScriptPasswordChangeActuator::OnChangeFormSubmitted,
                         weak_ptr_factory_.GetWeakPtr()));
  form_submission_helper_->FillChangePasswordForm(
      form_manager, username_, original_password_, generated_password_);
  NotifyStateChanged(PasswordChangeActuator::State::kChangingPassword);
}

void ScriptPasswordChangeActuator::OnPasswordChangeFormNotFound(
    ChangePasswordFormFinder::ErrorCase error_case) {
  form_finder_.reset();

  switch (error_case) {
    case ChangePasswordFormFinder::ErrorCase::kInterruptionDetected:
      NotifyStateChanged(PasswordChangeActuator::State::kOtpDetected);
      break;
    case ChangePasswordFormFinder::ErrorCase::kFailedToCapturePageContent:
    case ChangePasswordFormFinder::ErrorCase::kFailedToParseResponse:
    case ChangePasswordFormFinder::ErrorCase::kNoButtonToClick:
    case ChangePasswordFormFinder::ErrorCase::kFailedToClickButton:
    case ChangePasswordFormFinder::ErrorCase::kFormNotFound:
      NotifyStateChanged(
          PasswordChangeActuator::State::kChangePasswordFormNotFound);
      break;
  }
}

void ScriptPasswordChangeActuator::OnChangeFormSubmitted(
    ChangePasswordFormFillingSubmissionHelper::SubmissionResult result) {
  form_submission_helper_.reset();
  if (result.has_value()) {
    form_manager_ = std::move(result).value();
    submission_verifier_ = std::make_unique<PasswordChangeSubmissionVerifier>(
        GetExecutorWebContents(),
        ChromePasswordManagerClient::FromWebContents(GetExecutorWebContents()),
        logs_uploader_.get(),
        base::BindOnce(
            &ScriptPasswordChangeActuator::OnChangeFormSubmissionVerified,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  switch (result.error()) {
    case SubmissionError::kFailedToFillForm:
    case SubmissionError::kTimeout:
    case SubmissionError::kFailedToCaptureContent:
    case SubmissionError::kFailedToParseResponse:
    case SubmissionError::kSubmitButtonNotFound:
    case SubmissionError::kFailedToClickSubmit:
      NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
      break;
    case SubmissionError::kInterventionDetected:
      NotifyStateChanged(PasswordChangeActuator::State::kOtpDetected);
      break;
  }
}

void ScriptPasswordChangeActuator::OnChangeFormSubmissionVerified(
    SubmissionVerificationResult result) {
  submission_verifier_.reset();
  switch (result) {
    case SubmissionVerificationResult::kUserInterventionNeeded:
      if (auto logger = GetLoggerIfAvailable(GetExecutorWebContents())) {
        logger->LogBoolean(
            BrowserSavePasswordProgressLogger::
                STRING_AUTOMATED_PASSWORD_CHANGE_USER_INTERVENTION_AFTER_SUBMISSION,
            /*truth_value=*/true);
      }
      NotifyStateChanged(PasswordChangeActuator::State::kOtpDetected);
      break;
    case SubmissionVerificationResult::kFailure:
      form_manager_.reset();
      if (auto logger = GetLoggerIfAvailable(GetExecutorWebContents())) {
        logger->LogBoolean(
            BrowserSavePasswordProgressLogger::
                STRING_AUTOMATED_PASSWORD_CHANGE_SUBMISSION_VERIFIED,
            /*truth_value=*/false);
      }
      NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
      break;
    case SubmissionVerificationResult::kSuccess:
      if (auto logger = GetLoggerIfAvailable(GetExecutorWebContents())) {
        logger->LogBoolean(
            BrowserSavePasswordProgressLogger::
                STRING_AUTOMATED_PASSWORD_CHANGE_SUBMISSION_VERIFIED,
            /*truth_value=*/true);
      }
      CHECK(form_manager_);
      form_manager_->OnUpdateUsernameFromPrompt(username_);
      form_manager_->Save();
      form_manager_.reset();
      NotifyStateChanged(
          PasswordChangeActuator::State::kPasswordSuccessfullyChanged);
      break;
  }
}

void ScriptPasswordChangeActuator::OnCrossOriginNavigationDetected() {
  if (auto logger = GetLoggerIfAvailable(GetExecutorWebContents())) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::
            STRING_AUTOMATED_PASSWORD_CHANGE_CROSS_ORIGIN_NAVIGATION);
  }
  navigation_observer_.reset();

  ReportFlowInterruption(
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_CROSE_ORIGIN_NAVIGATION);

  if (form_finder_) {
    NotifyStateChanged(
        PasswordChangeActuator::State::kChangePasswordFormNotFound);
  } else if (form_submission_helper_ || submission_verifier_) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
  }

  ResetInternalState();
}

void ScriptPasswordChangeActuator::ReportFlowInterruption(
    QualityStatus status) {
  if (!logs_uploader_) {
    return;
  }

  if (form_finder_) {
    logs_uploader_->SetFlowInterrupted(
        FlowStep::PasswordChangeRequest_FlowStep_OPEN_FORM_STEP, status);
    return;
  }

  if (form_submission_helper_) {
    logs_uploader_->SetFlowInterrupted(
        form_submission_helper_->IsPasswordFormSubmitted()
            ? FlowStep::PasswordChangeRequest_FlowStep_VERIFY_SUBMISSION_STEP
            : FlowStep::PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP,
        status);
    return;
  }

  if (submission_verifier_) {
    logs_uploader_->SetFlowInterrupted(
        FlowStep::PasswordChangeRequest_FlowStep_VERIFY_SUBMISSION_STEP,
        status);
    return;
  }

  logs_uploader_->SetFlowInterrupted(
      FlowStep::PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP, status);
}

void ScriptPasswordChangeActuator::NotifyStateChanged(
    PasswordChangeActuator::State new_state) {
  observers_.Notify(&PasswordChangeActuator::Observer::OnActuationStateChanged,
                    new_state);
}

void ScriptPasswordChangeActuator::ResetInternalState() {
  navigation_observer_.reset();
  form_finder_.reset();
  form_submission_helper_.reset();
  submission_verifier_.reset();
  form_manager_.reset();
}
