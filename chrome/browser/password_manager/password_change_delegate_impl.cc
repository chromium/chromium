// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
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

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#endif

namespace {

using password_manager::BrowserSavePasswordProgressLogger;

void LogPasswordFormDetectedMetric(bool form_detected,
                                   base::TimeDelta time_delta) {
  base::UmaHistogramBoolean("PasswordManager.ChangePasswordFormDetected",
                            form_detected);
  if (form_detected) {
    base::UmaHistogramMediumTimes(
        "PasswordManager.ChangePasswordFormDetectionTime", time_delta);
  }
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
    base::WeakPtr<content::WebContents> web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  if (web_contents) {
    ManagePasswordsUIController::FromWebContents(web_contents.get())
        ->OnPasswordChangeFinishedSuccessfully();
  }
#endif
}

void DisplayChangePasswordBubbleAutomatically(
    base::WeakPtr<content::WebContents> web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  if (!web_contents) {
    return;
  }
  if (auto* manage_controller =
          ManagePasswordsUIController::FromWebContents(web_contents.get())) {
    manage_controller->ShowChangePasswordBubble();
  }
#endif
}

std::unique_ptr<BrowserSavePasswordProgressLogger> GetLoggerIfAvailable(
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents.get()));
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
  new_web_contents->Resize({0, 0, 1024, 768});

  return new_web_contents;
}

}  // namespace

PasswordChangeDelegateImpl::PasswordChangeDelegateImpl(
    GURL change_password_url,
    std::u16string username,
    std::u16string password,
    content::WebContents* originator)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(username)),
      original_password_(std::move(password)),
      profile_(Profile::FromBrowserContext(originator->GetBrowserContext())),
      originator_(originator->GetWeakPtr()) {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STARTED);
  }
  Observe(originator);
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() {
  base::UmaHistogramEnumeration(kFinalPasswordChangeStatusHistogram,
                                current_state_);
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogBoolean(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_FINISHED,
        current_state_ == State::kPasswordSuccessfullyChanged);
  }
}

void PasswordChangeDelegateImpl::OfferPasswordChangeUi() {
  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);
}

void PasswordChangeDelegateImpl::StartPasswordChangeFlow() {
  if (IsPrivacyNoticeAcknowledged()) {
    StartPasswordChange();
    return;
  }
  UpdateState(State::kWaitingForAgreement);
}

void PasswordChangeDelegateImpl::StartPasswordChange() {
  flow_start_time_ = base::Time::Now();
  UpdateState(State::kWaitingForChangePasswordForm);

  executor_ = CreateWebContents(profile_, change_password_url_);
  CHECK(executor_);
  logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(executor_.get());
  form_finder_ = std::make_unique<ChangePasswordFormFinder>(
      executor_.get(),
      base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormFound,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeDelegateImpl::OnPasswordChangeFormFound(
    password_manager::PasswordFormManager* form_manager) {
  form_finder_.reset();

  LogPasswordFormDetectedMetric(/*form_detected=*/form_manager,
                                base::Time::Now() - flow_start_time_);
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
          executor_.get(), logs_uploader_.get(),
          base::BindOnce(
              &PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified,
              weak_ptr_factory_.GetWeakPtr()));
  submission_verifier_->FillChangePasswordForm(form_manager, original_password_,
                                               generated_password_);
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);
}

void PasswordChangeDelegateImpl::WebContentsDestroyed() {
  // PasswordFormManager keeps raw pointers to PasswordManagerClient reset it
  // immediately to avoid keeping dangling pointer.
  submission_verifier_.reset();
  Stop();
}


bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ && originator_.get() == web_contents) ||
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

void PasswordChangeDelegateImpl::Restart() {
  CHECK_EQ(State::kChangePasswordFormNotFound, current_state_);
  CHECK(!submission_verifier_);

  StartPasswordChange();
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

  form_finder_.reset();
  submission_verifier_.reset();

  UpdateState(State::kOtpDetected);
}

#if !BUILDFLAG(IS_ANDROID)
void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  CHECK(originator_);
  auto* tab_interface = tabs::TabInterface::GetFromContents(originator_.get());
  CHECK(tab_interface);

  auto* tabs_strip =
      tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
  tabs_strip->AppendWebContents(std::move(executor_), /*foreground*/ true);
}
#endif

void PasswordChangeDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordChangeDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::u16string PasswordChangeDelegateImpl::GetDisplayOrigin() const {
  GURL url = submission_verifier_ ? submission_verifier_->GetURL()
                                  : change_password_url_;
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

const std::u16string& PasswordChangeDelegateImpl::GetUsername() const {
  return username_;
}

const std::u16string& PasswordChangeDelegateImpl::GetGeneratedPassword() const {
  return generated_password_;
}

void PasswordChangeDelegateImpl::OnPrivacyNoticeAccepted() {
  // Enable via the Optimization Guide's pref.
  profile_->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kPasswordChangeSubmission),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  StartPasswordChange();
}

void PasswordChangeDelegateImpl::UpdateState(
    PasswordChangeDelegate::State new_state) {
  if (new_state == current_state_) {
    return;
  }
  current_state_ = new_state;
  observers_.Notify(&PasswordChangeDelegate::Observer::OnStateChanged,
                    new_state);

  switch (current_state_) {
    case State::kWaitingForChangePasswordForm:
    case State::kChangingPassword:
      return;
    case State::kPasswordSuccessfullyChanged:
      NotifyPasswordChangeFinishedSuccessfully(originator_);
      // Fallthrough to trigger bubble display.
      [[fallthrough]];
    case State::kChangePasswordFormNotFound:
    case State::kOfferingPasswordChange:
    case State::kWaitingForAgreement:
    case State::kPasswordChangeFailed:
    case State::kOtpDetected:
      DisplayChangePasswordBubbleAutomatically(originator_);
      break;
  }

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogNumber(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STATE_CHANGED,
        static_cast<int>(new_state));
  }
}

void PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified(bool result) {
  base::UmaHistogramMediumTimes("PasswordManager.PasswordChangeTimeOverall",
                                base::Time::Now() - flow_start_time_);
  if (!result) {
    UpdateState(State::kPasswordChangeFailed);
  } else {
    // Password change was successful. Save new password with an original
    // username.
    submission_verifier_->SavePassword(username_);
    UpdateState(State::kPasswordSuccessfullyChanged);
  }
  // TODO(crbug.com/407503334): Upload final log on destructor.
  logs_uploader_->UploadFinalLog();
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

base::WeakPtr<PasswordChangeDelegate> PasswordChangeDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
