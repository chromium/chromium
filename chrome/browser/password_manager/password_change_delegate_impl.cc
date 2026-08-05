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
#include "chrome/browser/password_manager/password_change/features.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/password_manager/password_change/script_password_change_actuator.h"
#include "chrome/browser/password_manager/password_field_classification_model_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
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

constexpr base::TimeDelta kToastDisplayTime = base::Seconds(8);

void NotifyPasswordChangeFinishedSuccessfully(
    content::WebContents* web_contents) {
  if (web_contents) {
    if (PasswordsLeakDialogDelegate* delegate =
            ManagePasswordsUIController::FromWebContents(web_contents)) {
      delegate->OnPasswordChangeFinishedSuccessfully();
    }
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
      password_form_info_(std::move(credentials)),
      originator_(tab_interface->GetContents()),
      profile_(Profile::FromBrowserContext(originator_->GetBrowserContext())),
      ukm_source_id_(originator_->GetPrimaryMainFrame()->GetPageUkmSourceId()) {
  tab_will_detach_subscription_ = tab_interface->RegisterWillDetach(
      base::BindRepeating(&PasswordChangeDelegateImpl::OnTabWillDetach,
                          weak_ptr_factory_.GetWeakPtr()));
  ui_controller_ =
      std::make_unique<PasswordChangeUIController>(this, tab_interface);

  if (base::FeatureList::IsEnabled(
          password_change::features::
              kPasswordChangeWithPrivateInferenceLoginCheck)) {
    // This creates `FieldClassificationModelHandler` and should trigger
    // download of a local ML model for field classification.
    // TODO(crbug.com/452883239): Clean this up when model is downloaded on
    // start-up for everybody.
    PasswordFieldClassificationModelHandlerFactory::GetForBrowserContext(
        originator_->GetBrowserContext());

    // Don't show the dialog and don't start the flow if the user navigates to a
    // different site during login check.
    ObserveCrossOriginNavigationInOriginator();
    login_state_checker_ = std::make_unique<LoginStateChecker>(
        originator_.get(),
        ChromePasswordManagerClient::FromWebContents(originator_),
        optimization_guide::ModelExecutionServiceType::kPrivateAi,
        base::BindRepeating(
            &PasswordChangeDelegateImpl::OnLoginStateCheckedWithPIResult,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

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
  ObserveCrossOriginNavigationInOriginator();
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
  if (actuator_) {
    actuator_->RemoveObserver(this);
  }
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

void PasswordChangeDelegateImpl::StartPasswordChangeFlow() {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_START_FLOW);
  }
  flow_start_time_ = base::Time::Now();

  logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(
      originator_, change_password_url_);
  logs_uploader_->SetLoginPasswordFormInfo(password_form_info_);

  if (!actuator_) {
    actuator_ = std::make_unique<ScriptPasswordChangeActuator>(
        change_password_url_, password_form_info_, profile_,
        logs_uploader_.get());
    actuator_->AddObserver(this);
  }

  if (base::FeatureList::IsEnabled(
          password_change::features::
              kPasswordChangeWithPrivateInferenceLoginCheck)) {
    CHECK(login_check_result_);
    CHECK(actuator_);

    // Record login check result after user has accepted the feature.
    RecordLoggedInCheckQuality(std::move(*login_check_result_));
    login_check_result_.reset();

    actuator_->Start();
    return;
  }

  UpdateState(State::kWaitingForChangePasswordForm);
  login_state_checker_ = std::make_unique<LoginStateChecker>(
      originator_.get(),
      ChromePasswordManagerClient::FromWebContents(originator_),
      optimization_guide::ModelExecutionServiceType::kDefault,
      base::BindRepeating(
          &PasswordChangeDelegateImpl::OnLoginStateCheckedWithoutPIResult,
          weak_ptr_factory_.GetWeakPtr()));

  // This creates FieldClassificationModelHandler and should trigger
  // download of a local ML model for field classification.
  // TODO(452883239): Clean this up when model is downloaded on start-up for
  // everybody.
  PasswordFieldClassificationModelHandlerFactory::GetForBrowserContext(
      originator_->GetBrowserContext());
}

void PasswordChangeDelegateImpl::OnLoginStateCheckedWithoutPIResult(
    LoginCheckResult result) {
  CHECK(actuator_);

  switch (result.status) {
    case LoginCheckResult::Status::kLoggedIn:
      login_state_checker_.reset();
      navigation_observer_.reset();
      // User is logged in, start password change process.
      actuator_->Start();
      break;
    case LoginCheckResult::Status::kLoggedOut:
      UpdateState(State::kLoginFormDetected);
      break;
    case LoginCheckResult::Status::kError:
      login_state_checker_.reset();
      navigation_observer_.reset();
      UpdateState(State::kChangePasswordFormNotFound);
      break;
  }
  RecordLoggedInCheckQuality(std::move(result));
}

void PasswordChangeDelegateImpl::OnLoginStateCheckedWithPIResult(
    LoginCheckResult result) {
  switch (result.status) {
    case LoginCheckResult::Status::kLoggedIn:
      login_check_result_ =
          std::make_unique<LoginCheckResult>(std::move(result));
      login_state_checker_.reset();
      navigation_observer_.reset();

      // Offer the feature.
      UpdateState(IsPrivacyNoticeAcknowledged() ? State::kOfferingPasswordChange
                                                : State::kWaitingForAgreement);
      return;
    case LoginCheckResult::Status::kLoggedOut:
      if (!login_state_checker_->ReachedAttemptsLimit()) {
        return;
      }
      // Reached max attempt, treat as an error.
      [[fallthrough]];
    case LoginCheckResult::Status::kError:
      Stop();
      ResetInternalState();
      return;
  }
}

void PasswordChangeDelegateImpl::RecordLoggedInCheckQuality(
    LoginCheckResult result) {
  if (!logs_uploader_) {
    return;
  }
  logs_uploader_->SetLoggedInCheckQuality(result.state_checks_count,
                                          std::move(result.logging_data));
  logs_uploader_->SetStepDuration(
      ModelQualityLogsUploader::FlowStep::
          PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP,
      result.duration);
}

void PasswordChangeDelegateImpl::CancelPasswordChangeFlow() {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_CANCEL_FLOW);
  }
  if (actuator_) {
    actuator_->Cancel();
  }
  ResetInternalState();
  UpdateState(State::kCanceled);
}

void PasswordChangeDelegateImpl::OnActuationStateChanged(
    PasswordChangeDelegate::State new_state) {
  if (new_state == State::kPasswordSuccessfullyChanged) {
    NotifyPasswordChangeFinishedSuccessfully(originator_);
  }
  UpdateState(new_state);
}

void PasswordChangeDelegateImpl::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  if (reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(BrowserSavePasswordProgressLogger::
                           STRING_AUTOMATED_PASSWORD_CHANGE_TAB_DETACH);
  }
  base::UmaHistogramEnumeration("PasswordManager.PasswordChange.UserClosedTab",
                                current_state_);

  if (actuator_) {
    // Notify actuator to record and treat it as cancelation.
    actuator_->Cancel();
  }

  // Reset pointers immediately to avoid keeping dangling pointer to the tab.
  ResetInternalState();
  originator_ = nullptr;
  ui_controller_.reset();
  Stop();
}

bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ == web_contents) ||
         (actuator_ && actuator_->GetExecutorWebContents() == web_contents);
}

PasswordChangeDelegate::State PasswordChangeDelegateImpl::GetCurrentState()
    const {
  return current_state_;
}

void PasswordChangeDelegateImpl::Stop() {
  observers_.Notify(&PasswordChangeDelegate::Observer::OnPasswordChangeStopped,
                    this);
}

void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  CHECK(actuator_);
  actuator_->OpenPasswordChangeTab(originator_);
}

void PasswordChangeDelegateImpl::OpenPasswordDetails() {
  CHECK(originator_);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kShowTabWithPasswordChangeOnSuccess)) {
    OpenPasswordChangeTab();
    return;
  }

  bool is_same_domain = affiliations::IsExtendedPublicSuffixDomainMatch(
      change_password_url_, originator_->GetLastCommittedURL(), {});

  if (is_same_domain) {
    ManagePasswordsUIController::FromWebContents(originator_)
        ->ShowChangePasswordBubble(
            password_form_info_.username_value,
            actuator_ ? actuator_->GetGeneratedPassword() : std::u16string());
  } else {
    auto* tab = tabs::TabInterface::GetFromContents(originator_);
    BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
    NavigateToPasswordDetailsPage(
        browser,
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

void PasswordChangeDelegateImpl::UpdateState(State new_state) {
  if (new_state == current_state_) {
    return;
  }
  current_state_ = new_state;
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

bool PasswordChangeDelegateImpl::IsPrivacyNoticeAcknowledged() const {
  const OptimizationGuideKeyedService* const opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  return opt_guide_keyed_service != nullptr &&
         opt_guide_keyed_service->ShouldFeatureBeCurrentlyEnabledForUser(
             optimization_guide::UserVisibleFeatureKey::
                 kPasswordChangeSubmission);
}

std::u16string PasswordChangeDelegateImpl::GetDisplayOrigin() const {
  return url_formatter::FormatUrlForSecurityDisplay(
      change_password_url_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

void PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected() {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::
            STRING_AUTOMATED_PASSWORD_CHANGE_CROSS_ORIGIN_NAVIGATION);
  }
  navigation_observer_.reset();

  Stop();
  ResetInternalState();
}

base::WeakPtr<PasswordChangeDelegate> PasswordChangeDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordChangeDelegateImpl::ResetInternalState() {
  navigation_observer_.reset();
  login_state_checker_.reset();
  login_check_result_.reset();
  if (actuator_) {
    actuator_->RemoveObserver(this);
    actuator_.reset();
  }
  otp_fields_submitted_subscription_ = {};
}

void PasswordChangeDelegateImpl::ObserveCrossOriginNavigationInOriginator() {
  if (!originator_ || !originator_->GetURL().is_valid() ||
      !originator_->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  navigation_observer_ = std::make_unique<CrossOriginNavigationObserver>(
      originator_.get(), originator_->GetURL(),
      AffiliationServiceFactory::GetForProfile(profile_),
      base::BindOnce(
          &PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected,
          weak_ptr_factory_.GetWeakPtr()));
}
