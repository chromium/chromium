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
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
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

class CrossOriginNavigationObserver;
class ModelQualityLogsUploader;
struct LoginCheckResult;
class LoginStateChecker;
class Profile;

// This class controls password change process including acceptance of privacy
// notice, opening of a new tab, navigation to the change password url, password
// generation and form submission.
class PasswordChangeDelegateImpl : public PasswordChangeDelegate,
                                   public PasswordChangeActuator::Observer {
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
  LoginStateChecker* login_checker() { return login_state_checker_.get(); }
  PasswordChangeActuator* actuator() { return actuator_.get(); }
  PasswordChangeUIController* ui_controller() { return ui_controller_.get(); }
  ModelQualityLogsUploader* logs_uploader() { return logs_uploader_.get(); }
  std::u16string generated_password() {
    return actuator_ ? actuator_->GetGeneratedPassword() : std::u16string();
  }

  void SetCustomUIController(
      std::unique_ptr<PasswordChangeUIController> controller) {
    ui_controller_ = std::move(controller);
  }

  void inject_actuator_for_testing(
      std::unique_ptr<PasswordChangeActuator> actuator) {
    CHECK(!actuator_);
    actuator_ = std::move(actuator);
    if (actuator_) {
      actuator_->AddObserver(this);
    }
  }
#endif

  // PasswordChangeActuator::Observer impl
  void OnActuationStateChanged(State new_state) override;

  // PasswordChangeDelegate Impl
  void StartPasswordChangeFlow() override;
  void CancelPasswordChangeFlow() override;
  bool IsPasswordChangeOngoing(content::WebContents* web_contents) override;
  State GetCurrentState() const override;
  void Stop() override;
  void OpenPasswordChangeTab() override;
  void OpenPasswordDetails() override;
  void OnPrivacyNoticeAccepted() override;
  void OnPasswordChangeDeclined() override;
  void RetryLoginCheck() override;
  void AddObserver(PasswordChangeDelegate::Observer* observer) override;
  void RemoveObserver(PasswordChangeDelegate::Observer* observer) override;

 private:
  void OnOtpNotFound();

  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);

  void OnLoginStateCheckedWithoutPIResult(LoginCheckResult login_status);
  void OnLoginStateCheckedWithPIResult(LoginCheckResult login_status);
  // Updates `current_state_` and notifies `observers_`.
  void UpdateState(State new_state);

  bool IsPrivacyNoticeAcknowledged() const;

  std::u16string GetDisplayOrigin() const;

  void OnCrossOriginNavigationDetected();

  // Resets all helpers. `hidden_executor_` is kept as it is as user might want
  // to open it.
  void ResetInternalState();

  void ObserveCrossOriginNavigationInOriginator();

  void RecordLoggedInCheckQuality(LoginCheckResult result);

  const GURL change_password_url_;
  password_manager::PasswordForm password_form_info_;

  raw_ptr<content::WebContents> originator_ = nullptr;

  const raw_ptr<Profile> profile_ = nullptr;

  State current_state_ = State::kNoState;

  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;

  std::unique_ptr<PasswordChangeActuator> actuator_;

  // Helper class for checking the login state in the main tab.
  std::unique_ptr<LoginStateChecker> login_state_checker_;

  // Stores the login state check result when check is performed via Private
  // Inference before the user accepts the feature.
  std::unique_ptr<LoginCheckResult> login_check_result_;

  base::ObserverList<PasswordChangeDelegate::Observer, /*check_empty=*/true>
      observers_;

  // The time when the user started the password change flow.
  base::Time flow_start_time_;

  // The controller for password change views.
  std::unique_ptr<PasswordChangeUIController> ui_controller_;

  std::unique_ptr<CrossOriginNavigationObserver> navigation_observer_;

  base::CallbackListSubscription tab_will_detach_subscription_;
  // Subscription on the removal or submission of OTP fields in `originator_`.
  // The password change flow may be started directly after submitting a
  // username/password and can only proceed if the user submits an OTP in case
  // the website requires it. This subscription is only used before the password
  // change flow starts.
  base::CallbackListSubscription otp_fields_submitted_subscription_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  base::WeakPtrFactory<PasswordChangeDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
