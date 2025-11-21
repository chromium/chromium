// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
}  // namespace password_manager

class ChangePasswordFormFillingSubmissionHelper;
class ChangePasswordFormFinder;
class CrossOriginNavigationObserver;
enum class LoginCheckResult;
class LoginStateChecker;
class PasswordChangeHats;
class Profile;

// This class controls password change process including acceptance of privacy
// notice, opening of a new tab, navigation to the change password url, password
// generation and form submission.
class PasswordChangeDelegateImpl : public PasswordChangeDelegate {
 public:
  static char kFinalPasswordChangeStatusHistogram[];
  static char kCoarseFinalPasswordChangeStatusHistogram[];
  static char kPasswordChangeTimeOverallHistogram[];

  PasswordChangeDelegateImpl(GURL change_password_url,
                             password_manager::PasswordForm credentials,
                             tabs::TabInterface* tab_interface);
  ~PasswordChangeDelegateImpl() override;

  PasswordChangeDelegateImpl(const PasswordChangeDelegateImpl&) = delete;
  PasswordChangeDelegateImpl& operator=(const PasswordChangeDelegateImpl&) =
      delete;

  base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() override;

#if defined(UNIT_TEST)
  ModelQualityLogsUploader* logs_uploader() { return logs_uploader_.get(); }
  LoginStateChecker* login_checker() { return login_state_checker_.get(); }
  ChangePasswordFormFinder* form_finder() { return form_finder_.get(); }
  PasswordChangeUIController* ui_controller() { return ui_controller_.get(); }
  std::u16string generated_password() { return generated_password_; }
  ChangePasswordFormFillingSubmissionHelper* submission_verifier() {
    return submission_verifier_.get();
  }

  void SetCustomUIController(
      std::unique_ptr<PasswordChangeUIController> controller) {
    ui_controller_ = std::move(controller);
  }
#endif

  // Called by the OtpFieldDetector if an OTP field is detected in any relevant
  // frame of executor_. Visible for testing.
  void OnOtpFieldDetected();

  // Returns the web contents, on which the password change is run.
  content::WebContents* executor() {
    return hidden_executor_ ? hidden_executor_.get() : visible_executor_.get();
  }

 private:
  // PasswordChangeDelegate Impl
  void StartPasswordChangeFlow() override;
  void CancelPasswordChangeFlow() override;
  bool IsPasswordChangeOngoing(content::WebContents* web_contents) override;
  State GetCurrentState() const override;
  void Stop() override;
  void OpenPasswordChangeTab() override;
  void OpenPasswordDetails() override;
  void OnPasswordFormSubmission(content::WebContents* web_contents) override;
  void OnPrivacyNoticeAccepted() override;
  void OnPasswordChangeDeclined() override;
  void RetryLoginCheck() override;
  void AddObserver(PasswordChangeDelegate::Observer* observer) override;
  void RemoveObserver(PasswordChangeDelegate::Observer* observer) override;

  void ProceedToChangePassword();

  void OnOtpNotFound();

  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);

  void OnLoginStateCheckResult(LoginCheckResult login_status);
  // Updates `current_state_` and notifies `observers_`.
  void UpdateState(State new_state);

  void OnPasswordChangeFormFound(
      password_manager::PasswordFormManager* form_manager);

  void OnChangeFormSubmissionVerified(bool result);

  bool IsPrivacyNoticeAcknowledged() const;

  std::u16string GetDisplayOrigin() const;

  void OnCrossOriginNavigationDetected();

  void ReportFlowInterruption(ModelQualityLogsUploader::QualityStatus status);

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;
  password_manager::PasswordForm password_form_info_;

  std::u16string generated_password_;

  raw_ptr<content::WebContents> originator_ = nullptr;
  // If the password change tab is visible to the user, hidden_executor_ will be
  // null, if it's hidden, visible_executor_ will be null.
  std::unique_ptr<content::WebContents> hidden_executor_;
  raw_ptr<content::WebContents> visible_executor_ = nullptr;

  const raw_ptr<Profile> profile_ = nullptr;

  // Helper class which uploads model quality logs.
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;

  State current_state_ = State::kNoState;

  // Helper class which looks for a change password form.
  std::unique_ptr<ChangePasswordFormFinder> form_finder_;

  // Helper class which submits a form and verifies submission.
  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper>
      submission_verifier_;

  // Helper class for checking the login state in the main tab.
  std::unique_ptr<LoginStateChecker> login_state_checker_;

  base::ObserverList<PasswordChangeDelegate::Observer, /*check_empty=*/true>
      observers_;

  // The time when the user started the password change flow.
  base::Time flow_start_time_;

  // The controller for password change views.
  std::unique_ptr<PasswordChangeUIController> ui_controller_;

  // Helper class for handling happiness tracking surveys.
  std::unique_ptr<PasswordChangeHats> password_change_hats_;

  std::unique_ptr<CrossOriginNavigationObserver> navigation_observer_;

  base::CallbackListSubscription tab_will_detach_subscription_;
  // Subscription on the removal or submission of OTP fields in `originator_`.
  // The password change flow may be started directly after submitting a
  // username/password and can only proceed if the user submits an OTP in case
  // the website requires it. This subscription is only used before the password
  // change flow starts.
  base::CallbackListSubscription otp_fields_submitted_subscription_;
  // Subscription on adding OTP fields in `executor_` in case the user is
  // interrupted to enter an OTP while the password change flow happens.
  base::CallbackListSubscription otp_fields_detected_subscription_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Whether a blocking challenge (e.g. an OTP) was detected in the main tab.
  bool blocking_challenge_detected_ = false;

  base::WeakPtrFactory<PasswordChangeDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
