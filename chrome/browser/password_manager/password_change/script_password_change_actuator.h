// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SCRIPT_PASSWORD_CHANGE_ACTUATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SCRIPT_PASSWORD_CHANGE_ACTUATOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/detached_web_contents.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class CrossOriginNavigationObserver;
class Profile;

// Actuator implementation that uses DOM/script-based form finding, filling,
// submission, and verification.
class ScriptPasswordChangeActuator : public PasswordChangeActuator {
 public:
  ScriptPasswordChangeActuator(
      GURL change_password_url,
      password_manager::PasswordForm password_form_info,
      Profile* profile,
      ModelQualityLogsUploader* logs_uploader);
  ~ScriptPasswordChangeActuator() override;

  ScriptPasswordChangeActuator(const ScriptPasswordChangeActuator&) = delete;
  ScriptPasswordChangeActuator& operator=(const ScriptPasswordChangeActuator&) =
      delete;

  // PasswordChangeActuator:
  void Start() override;
  void Cancel() override;
  content::WebContents* GetExecutorWebContents() const override;
  void OpenPasswordChangeTab(content::WebContents* originator) override;
  std::u16string GetGeneratedPassword() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

#if defined(UNIT_TEST)
  ChangePasswordFormFinder* GetFormFinderForTesting() {
    return form_finder_.get();
  }
  CrossOriginNavigationObserver* GetNavigationObserverForTesting() {
    return navigation_observer_.get();
  }
  ModelQualityLogsUploader* GetLogsUploaderForTesting() {
    return logs_uploader_.get();
  }
#endif

 private:
  void OnPasswordChangeFormFound(
      password_manager::PasswordFormManager* form_manager);
  void OnPasswordChangeFormNotFound(
      ChangePasswordFormFinder::ErrorCase error_case);
  void OnChangeFormSubmitted(
      ChangePasswordFormFillingSubmissionHelper::SubmissionResult result);
  void OnChangeFormSubmissionVerified(
      PasswordChangeSubmissionVerifier::SubmissionVerificationResult result);
  void OnCrossOriginNavigationDetected();
  void ReportFlowInterruption(ModelQualityLogsUploader::QualityStatus status);
  void NotifyStateChanged(PasswordChangeDelegate::State new_state);
  void ResetInternalState();

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;
  std::u16string generated_password_;

  const raw_ptr<Profile> profile_ = nullptr;
  const raw_ptr<ModelQualityLogsUploader> logs_uploader_ = nullptr;

  std::unique_ptr<DetachedWebContents> hidden_executor_;
  std::unique_ptr<CrossOriginNavigationObserver> navigation_observer_;
  std::unique_ptr<ChangePasswordFormFinder> form_finder_;
  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper>
      form_submission_helper_;
  std::unique_ptr<PasswordChangeSubmissionVerifier> submission_verifier_;
  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;

  base::ObserverList<PasswordChangeActuator::Observer, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<ScriptPasswordChangeActuator> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SCRIPT_PASSWORD_CHANGE_ACTUATOR_H_
