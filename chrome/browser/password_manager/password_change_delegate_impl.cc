// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"
#include "chrome/browser/password_manager/password_change/detached_web_contents.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/password_manager/password_field_classification_model_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
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

constexpr base::TimeDelta kToastDisplayTime = base::Seconds(8);

std::u16string GeneratePassword(
    const password_manager::PasswordForm& form,
    password_manager::PasswordGenerationFrameHelper* generation_helper) {
  auto iter = std::ranges::find(form.form_data.fields(),
                                form.new_password_element_renderer_id,
                                &autofill::FormFieldData::renderer_id);
  CHECK(iter != form.form_data.fields().end());

  return generation_helper->GeneratePassword(
      form.url,
      autofill::password_generation::PasswordGenerationType::kAutomatic,
      autofill::CalculateFormSignature(form.form_data),
      autofill::CalculateFieldSignatureForField(*iter), iter->max_length());
}

void NotifyPasswordChangeFinishedSuccessfully(
    content::WebContents* web_contents) {
  if (web_contents) {
    ManagePasswordsUIController::FromWebContents(web_contents)
        ->OnPasswordChangeFinishedSuccessfully();
  }
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

  // Manually create ChromeAutofillClient and ChromePasswordManagerClient
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(
      detached_web_contents->GetWebContents());
  ChromePasswordManagerClient::CreateForWebContents(
      detached_web_contents->GetWebContents());

  // Apply client side predictions for WebContents where password change is
  // happening. This helps with correctly identifying change password form.
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

void FocusPasswordChangeTab(content::WebContents* executor) {
  auto* tab_interface = tabs::TabInterface::GetFromContents(executor);
  TabStripModel* tab_strip_model =
      tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
  int index = tab_strip_model->GetIndexOfWebContents(executor);
  CHECK(index != TabStripModel::kNoTab);
  tab_strip_model->ActivateTabAt(index);
}

PasswordChangeDelegate::CoarseFinalPasswordChangeState GetCoarseState(
    PasswordChangeDelegate::State state) {
  switch (state) {
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      return PasswordChangeDelegate::CoarseFinalPasswordChangeState::kOffered;

    case PasswordChangeDelegate::State::kCanceled:
    // Password change is "ongoing", but since the metric is recorded on
    // destruction of PasswordChangeDelegateImpl it means user canceled password
    // change implicitly by closing the tab.
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kLoginFormDetected:
      return PasswordChangeDelegate::CoarseFinalPasswordChangeState::kCanceled;

    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return PasswordChangeDelegate::CoarseFinalPasswordChangeState::
          kSuccessful;

    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      return PasswordChangeDelegate::CoarseFinalPasswordChangeState::kFailed;

    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      return PasswordChangeDelegate::CoarseFinalPasswordChangeState::
          kFormNotDetected;

    case PasswordChangeDelegate::State::kOtpDetected:
      return PasswordChangeDelegate::CoarseFinalPasswordChangeState::
          kOtpDetected;
    case PasswordChangeDelegate::State::kNoState:
      NOTREACHED();
  }
}

void OnLeakDialogHidden(base::WeakPtr<PasswordsModelDelegate> model_delegate) {
  if (model_delegate) {
    model_delegate->GetPasswordsLeakDialogDelegate()->OnLeakDialogHidden();
  }
}

}  // namespace

char PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram[] =
    "PasswordManager.FinalPasswordChangeStatus";
char PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram[] =
    "PasswordManager.CoarseFinalPasswordChangeStatus";
char PasswordChangeDelegateImpl::kPasswordChangeTimeOverallHistogram[] =
    "PasswordManager.PasswordChangeTimeOverall2";

PasswordChangeDelegateImpl::PasswordChangeDelegateImpl(
    GURL change_password_url,
    password_manager::PasswordForm credentials,
    tabs::TabInterface* tab_interface)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(credentials.username_value)),
      original_password_(std::move(credentials.password_value)),
      password_form_info_(std::move(credentials)),
      originator_(tab_interface->GetContents()),
      profile_(Profile::FromBrowserContext(originator_->GetBrowserContext())),
      ukm_source_id_(originator_->GetPrimaryMainFrame()->GetPageUkmSourceId()) {
  tab_will_detach_subscription_ = tab_interface->RegisterWillDetach(
      base::BindRepeating(&PasswordChangeDelegateImpl::OnTabWillDetach,
                          weak_ptr_factory_.GetWeakPtr()));
  ui_controller_ =
      std::make_unique<PasswordChangeUIController>(this, tab_interface);

  // When the flow is started after a leak warning and the user just submitted
  // their credentials, the website may still be waiting for an OTP submission
  // in the `originator_` tab. In this case we need to wait for the OTP to be
  // entered and submitted.
  autofill::ContentAutofillClient* autofill_client =
      autofill::ContentAutofillClient::FromWebContents(originator_);
  autofill::OtpFieldDetector* otp_field_detector =
      autofill_client->GetOtpFieldDetector();
  if (!otp_field_detector->IsOtpFieldPresent()) {
    // Proceed with password change immediately if there is no OTP on a page.
    OnOtpNotFound();
    return;
  }

  // Don't show the dialog and don't start the flow if user navigates to a
  // different site instead of entering the OTP.
  navigation_observer_ = std::make_unique<CrossOriginNavigationObserver>(
      originator_.get(), AffiliationServiceFactory::GetForProfile(profile_),
      base::BindOnce(
          &PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected,
          weak_ptr_factory_.GetWeakPtr()));

  // Register a callback to resume the password flow when the the OTP fields are
  // submitted or gone, assuming that the user has entered and submitted the
  // OTP in this case.
  otp_fields_submitted_subscription_ =
      otp_field_detector->RegisterOtpFieldsSubmittedCallback(
          base::BindRepeating(&PasswordChangeDelegateImpl::OnOtpNotFound,
                              weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeDelegateImpl::OnOtpNotFound() {
  // Stop listening for the removal of an OTP field.
  otp_fields_submitted_subscription_ = {};

  if (navigation_observer_ && !navigation_observer_->IsSameOrAffiliatedDomain(
                                  originator_->GetLastCommittedURL())) {
    // We may have detected an OTP submission that is actually a cross domain
    // navigation. In this case we want to abort the flow because the user
    // probably did not submit the OTP but navigated somewhere else.
    OnCrossOriginNavigationDetected();
    return;
  }

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_OTP_DISAPPEARED);
  }

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STARTED);
  }

  UpdateState(IsPrivacyNoticeAcknowledged() ? State::kOfferingPasswordChange
                                            : State::kWaitingForAgreement);
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() {
  if (logs_uploader_) {
    logs_uploader_->UploadFinalLog();
  }
  base::UmaHistogramEnumeration(kFinalPasswordChangeStatusHistogram,
                                current_state_);
  if (current_state_ != State::kNoState) {
    base::UmaHistogramMediumTimes(kPasswordChangeTimeOverallHistogram,
                                  base::Time::Now() - flow_start_time_);
    base::UmaHistogramEnumeration(kCoarseFinalPasswordChangeStatusHistogram,
                                  GetCoarseState(current_state_));
    ukm::builders::PasswordManager_ChangeFlowOutcome(ukm_source_id_)
        .SetCoarseFinalPasswordChangeStatus(
            static_cast<int>(GetCoarseState(current_state_)))
        .Record(ukm::UkmRecorder::Get());
  }
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogBoolean(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_FINISHED,
        current_state_ == State::kPasswordSuccessfullyChanged);
  }
  switch (current_state_) {
    case State::kNoState:
    case State::kPasswordSuccessfullyChanged:
      break;
    case State::kOfferingPasswordChange:
    case State::kWaitingForAgreement:
    case State::kWaitingForChangePasswordForm:
    case State::kChangePasswordFormNotFound:
    case State::kChangingPassword:
    case State::kPasswordChangeFailed:
    case State::kOtpDetected:
    case State::kCanceled:
    case State::kLoginFormDetected:
      // Set time to throttle APC offering, as we don't want to overprompt in
      // case of a negative experience.
      profile_->GetPrefs()->SetTime(
          password_manager::prefs::kLastNegativePasswordChangeTimestamp,
          base::Time::Now());
  }
}

content::WebContents* PasswordChangeDelegateImpl::executor() const {
  return hidden_executor_ ? hidden_executor_->GetWebContents()
                          : visible_executor_.get();
}

void PasswordChangeDelegateImpl::StartPasswordChangeFlow() {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_START_FLOW);
  }
  flow_start_time_ = base::Time::Now();
  UpdateState(State::kWaitingForChangePasswordForm);
  logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(
      originator_.get(), change_password_url_);
  logs_uploader_->SetLoginPasswordFormInfo(password_form_info_);
  login_state_checker_ = std::make_unique<LoginStateChecker>(
      originator_.get(), logs_uploader_.get(),
      ChromePasswordManagerClient::FromWebContents(originator_),
      base::BindRepeating(&PasswordChangeDelegateImpl::OnLoginStateCheckResult,
                          weak_ptr_factory_.GetWeakPtr()));

  // This creates FieldClassificationModelHandler and should trigger download of
  // a local ML model for field classification.
  // TODO(452883239): Clean this up when model is downloaded on start-up for
  // everybody.
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kProactivelyDownloadModelForPasswordChange)) {
    PasswordFieldClassificationModelHandlerFactory::GetForBrowserContext(
        originator_->GetBrowserContext());
  }
}

void PasswordChangeDelegateImpl::OnLoginStateCheckResult(
    LoginCheckResult login_status) {
  switch (login_status) {
    case LoginCheckResult::kLoggedIn:
      // User is logged in, start password change process.
      ProceedToChangePassword();
      return;
    case LoginCheckResult::kLoggedOut:
      UpdateState(State::kLoginFormDetected);
      return;
    case LoginCheckResult::kError:
      UpdateState(State::kChangePasswordFormNotFound);
      login_state_checker_.reset();
      return;
  }
}

void PasswordChangeDelegateImpl::CancelPasswordChangeFlow() {
  if (auto logger = GetLoggerIfAvailable(executor())) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_CANCEL_FLOW);
  }
  ReportFlowInterruption(
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
  ResetInternalState();
  UpdateState(State::kCanceled);
}

void PasswordChangeDelegateImpl::OnPasswordChangeFormFound(
    password_manager::PasswordFormManager* form_manager) {
  form_finder_.reset();

  CHECK(form_manager);
  generated_password_ = GeneratePassword(
      *form_manager->GetParsedObservedForm(),
      form_manager->GetDriver()->GetPasswordGenerationHelper());

  CHECK(executor());
  CHECK(!form_submission_helper_);
  form_submission_helper_ =
      std::make_unique<ChangePasswordFormFillingSubmissionHelper>(
          executor(), ChromePasswordManagerClient::FromWebContents(executor()),
          logs_uploader_.get(),
          base::BindOnce(&PasswordChangeDelegateImpl::OnChangeFormSubmitted,
                         weak_ptr_factory_.GetWeakPtr()));
  form_submission_helper_->FillChangePasswordForm(
      form_manager, username_, original_password_, generated_password_);
  UpdateState(State::kChangingPassword);
}

void PasswordChangeDelegateImpl::OnPasswordChangeFormNotFound(
    ChangePasswordFormFinder::ErrorCase error_case) {
  form_finder_.reset();

  switch (error_case) {
    case ChangePasswordFormFinder::ErrorCase::kInterruptionDetected:
      UpdateState(State::kOtpDetected);
      break;
    case ChangePasswordFormFinder::ErrorCase::kFailedToCapturePageContent:
    case ChangePasswordFormFinder::ErrorCase::kFailedToParseResponse:
    case ChangePasswordFormFinder::ErrorCase::kNoButtonToClick:
    case ChangePasswordFormFinder::ErrorCase::kFailedToClickButton:
    case ChangePasswordFormFinder::ErrorCase::kFormNotFound:
      UpdateState(State::kChangePasswordFormNotFound);
      break;
  }
}

void PasswordChangeDelegateImpl::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    if (auto logger = GetLoggerIfAvailable(originator_)) {
      logger->LogMessage(BrowserSavePasswordProgressLogger::
                             STRING_AUTOMATED_PASSWORD_CHANGE_TAB_DETACH);
    }
    base::UmaHistogramEnumeration(
        "PasswordManager.PasswordChange.UserClosedTab", current_state_);
    ReportFlowInterruption(
        QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
    // Reset pointers immediately to avoid keeping dangling pointer to the tab.
    ResetInternalState();
    originator_ = nullptr;
    visible_executor_ = nullptr;
    hidden_executor_.reset();
    ui_controller_.reset();
    Stop();
  }
}

bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ == web_contents) ||
         (executor() && executor() == web_contents);
}

PasswordChangeDelegate::State PasswordChangeDelegateImpl::GetCurrentState()
    const {
  return current_state_;
}

void PasswordChangeDelegateImpl::Stop() {
  observers_.Notify(&PasswordChangeDelegate::Observer::OnPasswordChangeStopped,
                    this);
}

void PasswordChangeDelegateImpl::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (form_submission_helper_) {
    form_submission_helper_->OnPasswordFormSubmission(web_contents);
  }
}

void PasswordChangeDelegateImpl::OnOtpFieldDetected() {
  if (auto logger = GetLoggerIfAvailable(executor())) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_OTP_DETECTED);
  }
  ReportFlowInterruption(
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED);

  ResetInternalState();
  UpdateState(State::kOtpDetected);
}

void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  content::WebContents* web_contents = executor();
  if (!web_contents) {
    web_contents = originator_->OpenURL(
        content::OpenURLParams(GURL(change_password_url_), content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               /* is_renderer_initiated= */ false),
        /*navigation_handle_callback=*/{});
    CHECK(web_contents);
  } else if (!visible_executor_) {
    AddPasswordChangeToTabStrip(
        originator_,
        DetachedWebContents::ReleaseWebContents(std::move(hidden_executor_)));
  } else {
    FocusPasswordChangeTab(web_contents);
  }

  if (current_state_ == State::kOtpDetected && form_manager_) {
    CHECK(base::FeatureList::IsEnabled(
        password_manager::features::kUserInterventionForPasswordChange));
    // If user decided to take over control when interruption is detected we
    // assume they will complete the password change process, thus the new
    // password must be saved.
    form_manager_->OnUpdateUsernameFromPrompt(username_);
    form_manager_->Save();
    form_manager_.reset();
  }
}

void PasswordChangeDelegateImpl::OpenPasswordDetails() {
  CHECK(originator_);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kShowTabWithPasswordChangeOnSuccess)) {
    OpenPasswordChangeTab();
    return;
  }

  if (navigation_observer_->IsSameOrAffiliatedDomain(
          originator_->GetLastCommittedURL())) {
    ManagePasswordsUIController::FromWebContents(originator_)
        ->ShowChangePasswordBubble(username_, generated_password_);
  } else {
    NavigateToPasswordDetailsPage(
        chrome::FindBrowserWithTab(originator_),
        base::UTF16ToUTF8(GetDisplayOrigin()),
        password_manager::ManagePasswordsReferrer::kPasswordChangeInfoBubble);
  }
}

void PasswordChangeDelegateImpl::OnPrivacyNoticeAccepted() {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::
            STRING_AUTOMATED_PASSWORD_CHANGE_PRIVACY_NOTICE_ACCEPTED);
  }
  // Enable via the Optimization Guide's pref.
  profile_->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kPasswordChangeSubmission),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  StartPasswordChangeFlow();
}

void PasswordChangeDelegateImpl::OnPasswordChangeDeclined() {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::
            STRING_AUTOMATED_PASSWORD_CHANGE_PASSWORD_CHANGE_DECLINED);
  }
  // Post task as otherwise ManagePasswordsUIController won't show a bubble
  // until password change has finished.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OnLeakDialogHidden,
                     ManagePasswordsUIController::FromWebContents(originator_)
                         ->GetModelDelegateProxy()));
}

void PasswordChangeDelegateImpl::RetryLoginCheck() {
  CHECK(login_state_checker_);
  login_state_checker_->RetryLoginCheck();
  UpdateState(State::kWaitingForChangePasswordForm);
}

void PasswordChangeDelegateImpl::AddObserver(
    PasswordChangeDelegate::Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordChangeDelegateImpl::RemoveObserver(
    PasswordChangeDelegate::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordChangeDelegateImpl::ProceedToChangePassword() {
  login_state_checker_.reset();
  UpdateState(State::kWaitingForChangePasswordForm);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kRunPasswordChangeInBackgroundTab)) {
    visible_executor_ = originator_->OpenURL(
        content::OpenURLParams(GURL(change_password_url_), content::Referrer(),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  } else {
    hidden_executor_ =
        CreateDetachedWebContents(profile_, change_password_url_);
  }
  CHECK(executor());
  auto* client = ChromePasswordManagerClient::FromWebContents(executor());

  navigation_observer_ = std::make_unique<CrossOriginNavigationObserver>(
      executor(), AffiliationServiceFactory::GetForProfile(profile_),
      base::BindOnce(
          &PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected,
          weak_ptr_factory_.GetWeakPtr()));
  form_finder_ = std::make_unique<ChangePasswordFormFinder>(
      executor(), client, logs_uploader_.get(),
      base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormFound,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormNotFound,
                     weak_ptr_factory_.GetWeakPtr()));

  // When interruptions (including OTPs) are detected on a server there is no
  // need to use local ML model for OTP detection.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kUserInterventionForPasswordChange)) {
    // Even though the user is assumed to be fully signed in by this point in
    // time, they may still see an OTP during the password change flow, so watch
    // for this.
    autofill::ContentAutofillClient* autofill_client =
        autofill::ContentAutofillClient::FromWebContents(executor());
    autofill::OtpFieldDetector* otp_field_detector =
        autofill_client->GetOtpFieldDetector();
    otp_fields_detected_subscription_ =
        otp_field_detector->RegisterOtpFieldsDetectedCallback(
            base::BindRepeating(&PasswordChangeDelegateImpl::OnOtpFieldDetected,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void PasswordChangeDelegateImpl::UpdateState(State new_state) {
  if (new_state == current_state_) {
    return;
  }
  current_state_ = new_state;
  observers_.Notify(&PasswordChangeDelegate::Observer::OnStateChanged,
                    new_state);
  ui_controller_->UpdateState(new_state);

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogNumber(BrowserSavePasswordProgressLogger::
                          STRING_AUTOMATED_PASSWORD_CHANGE_STATE_CHANGED,
                      static_cast<int>(new_state));
  }

  // In case the password change was canceled or finished successfully, the flow
  // and the respective UI should be stopped after a specified timeout.
  if (current_state_ == State::kCanceled ||
      current_state_ == State::kPasswordSuccessfullyChanged) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PasswordChangeDelegate::Stop, AsWeakPtr()),
        kToastDisplayTime);
  }
}

void PasswordChangeDelegateImpl::OnChangeFormSubmitted(
    ChangePasswordFormFillingSubmissionHelper::SubmissionResult result) {
  form_submission_helper_.reset();
  if (result.has_value()) {
    form_manager_ = std::move(result).value();
    submission_verifier_ = std::make_unique<PasswordChangeSubmissionVerifier>(
        executor(), logs_uploader_.get(),
        base::BindOnce(
            &PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified,
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
      UpdateState(State::kPasswordChangeFailed);
      break;
    case SubmissionError::kInterventionDetected:
      UpdateState(State::kOtpDetected);
      break;
  }
}

void PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified(
    SubmissionVerificationResult result) {
  submission_verifier_.reset();
  switch (result) {
    case SubmissionVerificationResult::kUserInterventionNeeded:
      // The feature must be enabled to receive the User Intervention state.
      if (auto logger = GetLoggerIfAvailable(executor())) {
        logger->LogBoolean(
            BrowserSavePasswordProgressLogger::
                STRING_AUTOMATED_PASSWORD_CHANGE_USER_INTERVENTION_AFTER_SUBMISSION,
            /*truth_value=*/true);
      }
      UpdateState(State::kOtpDetected);
      break;
    case SubmissionVerificationResult::kFailure:
      if (auto logger = GetLoggerIfAvailable(executor())) {
        logger->LogBoolean(
            BrowserSavePasswordProgressLogger::
                STRING_AUTOMATED_PASSWORD_CHANGE_SUBMISSION_VERIFIED,
            /*truth_value=*/false);
      }
      UpdateState(State::kPasswordChangeFailed);
      break;
    case SubmissionVerificationResult::kSuccess:
      if (auto logger = GetLoggerIfAvailable(executor())) {
        logger->LogBoolean(
            BrowserSavePasswordProgressLogger::
                STRING_AUTOMATED_PASSWORD_CHANGE_SUBMISSION_VERIFIED,
            /*truth_value=*/true);
      }
      // Password change was successful. Save new password with an original
      // username.
      CHECK(form_manager_);
      form_manager_->OnUpdateUsernameFromPrompt(username_);
      form_manager_->Save();
      form_manager_.reset();
      NotifyPasswordChangeFinishedSuccessfully(originator_);
      UpdateState(State::kPasswordSuccessfullyChanged);
      break;
  }
}

bool PasswordChangeDelegateImpl::IsPrivacyNoticeAcknowledged() const {
  const OptimizationGuideKeyedService* const opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  return opt_guide_keyed_service != nullptr &&
         opt_guide_keyed_service->ShouldFeatureBeCurrentlyEnabledForUser(
             optimization_guide::UserVisibleFeatureKey::
                 kPasswordChangeSubmission);
}

std::u16string PasswordChangeDelegateImpl::GetDisplayOrigin() const {
  GURL url = form_manager_ ? form_manager_->GetURL() : change_password_url_;
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

void PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected() {
  if (auto logger = GetLoggerIfAvailable(executor())) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::
            STRING_AUTOMATED_PASSWORD_CHANGE_CROSS_ORIGIN_NAVIGATION);
  }
  navigation_observer_.reset();

  ReportFlowInterruption(
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_CROSE_ORIGIN_NAVIGATION);

  // Navigation happened when looking for a change password form, password
  // change can be terminated safely with `kChangePasswordFormNotFound`.
  if (form_finder_) {
    UpdateState(State::kChangePasswordFormNotFound);
  } else if (form_submission_helper_ || submission_verifier_) {
    // Navigation happened when submitting the form. Terminate flow with a
    // failure message.
    UpdateState(State::kPasswordChangeFailed);
  } else {
    // This shouldn't happen, just stop the flow immediately.
    Stop();
  }

  ResetInternalState();
}

void PasswordChangeDelegateImpl::ReportFlowInterruption(QualityStatus status) {
  if (!logs_uploader_) {
    return;
  }

  if (login_state_checker_) {
    logs_uploader_->SetFlowInterrupted(
        FlowStep::PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP, status);
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
}

base::WeakPtr<PasswordChangeDelegate> PasswordChangeDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordChangeDelegateImpl::ResetInternalState() {
  navigation_observer_.reset();
  login_state_checker_.reset();
  form_finder_.reset();
  form_submission_helper_.reset();
  submission_verifier_.reset();
  form_manager_.reset();
  otp_fields_detected_subscription_ = {};
}
