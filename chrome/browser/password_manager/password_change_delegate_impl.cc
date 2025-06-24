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
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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
  new_web_contents->Resize({0, 0, 1024, 768});

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
      profile_(Profile::FromBrowserContext(originator_->GetBrowserContext())),
      ui_controller_(
          std::make_unique<PasswordChangeUIController>(this, tab_interface)),
      last_committed_url_(originator_->GetLastCommittedURL()) {
  tab_will_detach_subscription_ = tab_interface->RegisterWillDetach(
      base::BindRepeating(&PasswordChangeDelegateImpl::OnTabWillDetach,
                          weak_ptr_factory_.GetWeakPtr()));

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STARTED);
  }

  UpdateState(IsPrivacyNoticeAcknowledged() ? State::kOfferingPasswordChange
                                            : State::kWaitingForAgreement);
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() {
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
  UpdateState(State::kWaitingForChangePasswordForm);

  executor_ = CreateWebContents(profile_, change_password_url_);
  CHECK(executor_);
  logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(executor_.get());
  form_finder_ = std::make_unique<ChangePasswordFormFinder>(
      executor_.get(),
      base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormFound,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeDelegateImpl::CancelPasswordChangeFlow() {
  submission_verifier_.reset();
  form_finder_.reset();
  executor_.reset();

  UpdateState(State::kCanceled);
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
  submission_verifier_->FillChangePasswordForm(
      form_manager, username_, original_password_, generated_password_);
  UpdateState(State::kChangingPassword);
}

void PasswordChangeDelegateImpl::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    // Reset pointers immediately to avoid keeping dangling pointer to the tab.
    originator_ = nullptr;
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

  form_finder_.reset();
  submission_verifier_.reset();

  UpdateState(State::kOtpDetected);
}

void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  CHECK(originator_);
  auto* tab_interface = tabs::TabInterface::GetFromContents(originator_);
  CHECK(tab_interface);

  auto* tabs_strip =
      tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
  tabs_strip->AppendWebContents(std::move(executor_), /*foreground*/ true);
}

void PasswordChangeDelegateImpl::OpenPasswordDetails() {
  CHECK(originator_);

  if (last_committed_url_ == originator_->GetLastCommittedURL()) {
    ManagePasswordsUIController::FromWebContents(originator_)
        ->ShowChangePasswordBubble();
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
  StartPasswordChangeFlow();
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
  base::UmaHistogramMediumTimes("PasswordManager.PasswordChangeTimeOverall",
                                base::Time::Now() - flow_start_time_);
  if (!result) {
    UpdateState(State::kPasswordChangeFailed);
  } else {
    // Password change was successful. Save new password with an original
    // username.
    submission_verifier_->SavePassword(username_);
    NotifyPasswordChangeFinishedSuccessfully(originator_);
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
