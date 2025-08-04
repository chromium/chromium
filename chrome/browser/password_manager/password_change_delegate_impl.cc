// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/otp_detection_helper.h"
#include "chrome/browser/password_manager/password_change/password_change_hats.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

constexpr char kLeakDialogTimeSpentHistogram[] =
    "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent";

void LogPasswordFormDetectedMetric(bool form_detected,
                                   base::TimeDelta time_delta) {
  base::UmaHistogramBoolean("PasswordManager.ChangePasswordFormDetected",
                            form_detected);
  if (form_detected) {
    base::UmaHistogramMediumTimes(
        "PasswordManager.ChangePasswordFormDetectionTime", time_delta);
  }
}

void LogLeakDialogTimeSpent(PasswordChangeDelegate::State state,
                            base::TimeDelta time_delta) {
  CHECK(state == PasswordChangeDelegate::State::kWaitingForAgreement ||
        state == PasswordChangeDelegate::State::kOfferingPasswordChange);

  std::string suffix =
      state == PasswordChangeDelegate::State::kWaitingForAgreement
          ? ".WithPrivacyNotice"
          : ".WithoutPrivacyNotice";
  base::UmaHistogramMediumTimes(
      base::StrCat({kLeakDialogTimeSpentHistogram, suffix}), time_delta);
}

// Logs whether user had any passwords saved for the website where the change
// password was offered.
void LogPasswordSavedOnStart(content::WebContents* web_contents) {
  CHECK(web_contents);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  if (!manage_passwords_ui_controller) {
    return;
  }

  base::UmaHistogramBoolean(
      "PasswordManager.PasswordChange.UserHasPasswordSavedOnAPCLaunch",
      !manage_passwords_ui_controller->GetCurrentForms().empty());
}

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

std::unique_ptr<content::WebContents> CreateWebContents(Profile* profile,
                                                        const GURL& url) {
  scoped_refptr<content::SiteInstance> initial_site_instance_for_new_contents =
      tab_util::GetSiteInstanceForNewTab(profile, url);
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          profile, initial_site_instance_for_new_contents));

  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(new_web_contents.get());
  ChromePasswordManagerClient::CreateForWebContents(new_web_contents.get());

  new_web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
  // Provide more height so that the change password button is visible on
  // screen.
  new_web_contents->Resize({0, 0, 1024, 768 * 2});

  return new_web_contents;
}

}  // namespace

PasswordChangeDelegateImpl::PasswordChangeDelegateImpl(
    GURL change_password_url,
    std::u16string username,
    std::u16string password,
    tabs::TabInterface* tab_interface)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(username)),
      original_password_(std::move(password)),
      originator_(tab_interface->GetContents()),
      profile_(Profile::FromBrowserContext(originator_->GetBrowserContext())) {
  tab_will_detach_subscription_ = tab_interface->RegisterWillDetach(
      base::BindRepeating(&PasswordChangeDelegateImpl::OnTabWillDetach,
                          weak_ptr_factory_.GetWeakPtr()));
  ui_controller_ =
      std::make_unique<PasswordChangeUIController>(this, tab_interface);

  auto* client = ChromePasswordManagerClient::FromWebContents(originator_);
  if (!OtpDetectionHelper::IsOtpPresent(originator_, client)) {
    // Proceed with password change immediately if there is no OTP on a page.
    OnOtpNotFound();
    return;
  }
  otp_detection_ = std::make_unique<OtpDetectionHelper>(
      originator_, client,
      base::BindOnce(&PasswordChangeDelegateImpl::OnOtpNotFound,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeDelegateImpl::OnOtpNotFound() {
  otp_detection_.reset();

  password_change_hats_ = std::make_unique<PasswordChangeHats>(
      HatsServiceFactory::GetForProfile(profile_,
                                        /*create_if_necessary=*/true),
      ProfilePasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS)
          .get());
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STARTED);
  }

  UpdateState(IsPrivacyNoticeAcknowledged() ? State::kOfferingPasswordChange
                                            : State::kWaitingForAgreement);
  leak_dialog_display_time_ = base::Time::Now();
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() {
  if (logs_uploader_) {
    logs_uploader_->UploadFinalLog();
  }
  base::UmaHistogramEnumeration(kFinalPasswordChangeStatusHistogram,
                                current_state_);
  if (auto logger = GetLoggerIfAvailable(executor_.get())) {
    logger->LogBoolean(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_FINISHED,
        current_state_ == State::kPasswordSuccessfullyChanged);
  }
}

void PasswordChangeDelegateImpl::StartPasswordChangeFlow() {
  flow_start_time_ = base::Time::Now();
  LogLeakDialogTimeSpent(current_state_,
                         flow_start_time_ - leak_dialog_display_time_);
  LogPasswordSavedOnStart(originator_);
  UpdateState(State::kWaitingForChangePasswordForm);

  executor_ = CreateWebContents(profile_, change_password_url_);
  CHECK(executor_);
  navigation_observer_ = std::make_unique<CrossOriginNavigationObserver>(
      executor_.get(), AffiliationServiceFactory::GetForProfile(profile_),
      base::BindOnce(
          &PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected,
          weak_ptr_factory_.GetWeakPtr()));
  logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(executor_.get());
  form_finder_ = std::make_unique<ChangePasswordFormFinder>(
      executor_.get(),
      ChromePasswordManagerClient::FromWebContents(executor_.get()),
      logs_uploader_.get(), change_password_url_,
      base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormFound,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PasswordChangeDelegateImpl::OnLoginFormFound,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeDelegateImpl::CancelPasswordChangeFlow() {
  if (logs_uploader_) {
    logs_uploader_->SetFlowInterrupted();
  }
  navigation_observer_.reset();
  submission_verifier_.reset();
  form_finder_.reset();
  executor_.reset();

  UpdateState(State::kCanceled);
  password_change_hats_->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeCanceled,
      /*password_change_duration=*/base::Time::Now() - flow_start_time_,
      originator_);
}

void PasswordChangeDelegateImpl::OnPasswordChangeFormFound(
    password_manager::PasswordFormManager* form_manager) {
  form_finder_.reset();

  change_password_form_found_time_ = base::Time::Now();
  LogPasswordFormDetectedMetric(
      /*form_detected=*/form_manager,
      change_password_form_found_time_ - flow_start_time_);
  if (!form_manager) {
    UpdateState(State::kChangePasswordFormNotFound);
    return;
  }

  CHECK(!submission_verifier_);
  CHECK(executor_);
  generated_password_ = GeneratePassword(
      *form_manager->GetParsedObservedForm(),
      form_manager->GetDriver()->GetPasswordGenerationHelper());

  submission_verifier_ =
      std::make_unique<ChangePasswordFormFillingSubmissionHelper>(
          executor_.get(),
          ChromePasswordManagerClient::FromWebContents(executor_.get()),
          logs_uploader_.get(),
          base::BindOnce(
              &PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified,
              weak_ptr_factory_.GetWeakPtr()));
  submission_verifier_->FillChangePasswordForm(
      form_manager, username_, original_password_, generated_password_);
  UpdateState(State::kChangingPassword);
}

void PasswordChangeDelegateImpl::OnLoginFormFound() {
  UpdateState(State::kLoginFormDetected);
}

void PasswordChangeDelegateImpl::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    base::UmaHistogramEnumeration(
        "PasswordManager.PasswordChange.UserClosedTab", current_state_);
    if (logs_uploader_) {
      logs_uploader_->SetFlowInterrupted();
    }
    // Reset pointers immediately to avoid keeping dangling pointer to the tab.
    originator_ = nullptr;
    navigation_observer_.reset();
    submission_verifier_.reset();
    ui_controller_.reset();
    form_finder_.reset();
    submission_verifier_.reset();
    Stop();
  }
}

bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ == web_contents) ||
         (executor_ && executor_.get() == web_contents);
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
  if (submission_verifier_) {
    submission_verifier_->OnPasswordFormSubmission(web_contents);
  }
}

void PasswordChangeDelegateImpl::OnOtpFieldDetected(
    content::WebContents* web_contents) {
  if (!executor_ || web_contents != executor_.get()) {
    return;
  }

  // OTP is relevant only when the change password flow is "ongoing", other
  // states should be disregarded.
  if (current_state_ != State::kChangingPassword &&
      current_state_ != State::kWaitingForChangePasswordForm) {
    return;
  }

  if (logs_uploader_) {
    logs_uploader_->SetOtpDetected();
  }

  form_finder_.reset();
  submission_verifier_.reset();

  UpdateState(State::kOtpDetected);
}

void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  CHECK(originator_);
  auto* tab_interface = tabs::TabInterface::GetFromContents(originator_);
  CHECK(tab_interface);
  TabStripModel* tab_strip_model =
      tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
  CHECK(tab_strip_model);

  content::WebContents* web_contents = executor_.get();
  tab_strip_model->AppendWebContents(std::move(executor_), /*foreground=*/true);
  password_change_hats_->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeError,
      /*password_change_duration=*/base::Time::Now() - flow_start_time_,
      web_contents);
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

void PasswordChangeDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordChangeDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordChangeDelegateImpl::OnPrivacyNoticeAccepted() {
  // Enable via the Optimization Guide's pref.
  profile_->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kPasswordChangeSubmission),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  StartPasswordChangeFlow();
}

void PasswordChangeDelegateImpl::OnPasswordChangeDeclined() {
  password_change_hats_->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeCanceled,
      /*password_change_duration=*/base::TimeDelta(), originator_);
}

void PasswordChangeDelegateImpl::UpdateState(State new_state) {
  if (new_state == current_state_) {
    return;
  }
  current_state_ = new_state;
  observers_.Notify(&Observer::OnStateChanged, new_state);
  ui_controller_->UpdateState(new_state);

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogNumber(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STATE_CHANGED,
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

void PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified(bool result) {
  base::Time time_now = base::Time::Now();
  base::TimeDelta password_change_duration_overall =
      time_now - flow_start_time_;
  base::UmaHistogramMediumTimes(
      "PasswordManager.ChangingPasswordToast.TimeSpent",
      time_now - change_password_form_found_time_);
  base::UmaHistogramMediumTimes("PasswordManager.PasswordChangeTimeOverall",
                                password_change_duration_overall);

  if (!result) {
    UpdateState(State::kPasswordChangeFailed);
  } else {
    // Password change was successful. Save new password with an original
    // username.
    submission_verifier_->SavePassword(username_);
    NotifyPasswordChangeFinishedSuccessfully(originator_);
    UpdateState(State::kPasswordSuccessfullyChanged);
    password_change_hats_->MaybeLaunchSurvey(
        kHatsSurveyTriggerPasswordChangeSuccess,
        password_change_duration_overall, originator_);
  }
  submission_verifier_.reset();
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
  GURL url = submission_verifier_ ? submission_verifier_->GetURL()
                                  : change_password_url_;
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

void PasswordChangeDelegateImpl::OnCrossOriginNavigationDetected() {
  navigation_observer_.reset();

  // Navigation happened when looking for a change password form, password
  // change can be terminated safely with `kChangePasswordFormNotFound`.
  if (form_finder_) {
    OnPasswordChangeFormFound(/*form_manager=*/nullptr);
    return;
  }
  // Navigation happened when submitting the form. Terminate flow with a failure
  // message.
  if (submission_verifier_) {
    OnChangeFormSubmissionVerified(false);
    return;
  }

  // This shouldn't happen, just stop the flow immediately.
  Stop();
}

base::WeakPtr<PasswordChangeDelegate> PasswordChangeDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
