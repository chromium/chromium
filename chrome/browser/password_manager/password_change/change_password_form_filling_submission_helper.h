// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FILLING_SUBMISSION_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FILLING_SUBMISSION_HELPER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/password_manager/core/browser/password_form.h"

namespace content {
class WebContents;
}
namespace password_manager {
class PasswordFormManager;
class PasswordManagerDriver;
}  // namespace password_manager

class PasswordChangeSubmissionVerifier;
class ModelQualityLogsUploader;

// Helper class which fills a form, submits it and verifies submission result.
// Upon completion invokes `result_callback` to notify the result of submission.
class ChangePasswordFormFillingSubmissionHelper {
 public:
  static constexpr base::TimeDelta kSubmissionWaitingTimeout =
      base::Seconds(10);

  ChangePasswordFormFillingSubmissionHelper(
      content::WebContents* web_contents,
      ModelQualityLogsUploader* logs_uploader,
      base::OnceCallback<void(bool)> result_callback);

  // Test constructor (allows to mock `capture_annotated_page_content`).
  ChangePasswordFormFillingSubmissionHelper(
      base::PassKey<class ChangePasswordFormFillingSubmissionHelperTest>,
      content::WebContents* web_contents,
      ModelQualityLogsUploader* logs_uploader,
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
          capture_annotated_page_content,
      base::OnceCallback<void(bool)> result_callback);
  ~ChangePasswordFormFillingSubmissionHelper();

  // Starts chain of actions:
  // * fills and submits a change password form observed by `form_manager`,
  // * pre-saves the new_password as a backup,
  // * provisionally saves submitted password.
  void FillChangePasswordForm(
      password_manager::PasswordFormManager* form_manager,
      const std::u16string& username,
      const std::u16string& old_password,
      const std::u16string& new_password);

  // Triggers verification if `web_contents` is the same as initial WebContents.
  void OnPasswordFormSubmission(content::WebContents* web_contents);

  // Saves a password with a given `username`. Must be called only after
  // `callback_` was invoked.
  void SavePassword(const std::u16string& username);

  // Returns current URL from the `form_manager_`.
  GURL GetURL() const;

#if defined(UNIT_TEST)
  PasswordChangeSubmissionVerifier* submission_verifier() {
    return submission_verifier_.get();
  }

  ButtonClickHelper* click_helper() { return click_helper_.get(); }

  password_manager::PasswordFormManager* form_manager() {
    return form_manager_.get();
  }
#endif

 private:
  void TriggerFilling(
      const password_manager::PasswordForm& form,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      const std::u16string& username,
      const std::u16string& old_password,
      const std::u16string& new_password);

  void ChangePasswordFormFilled(
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      autofill::FieldRendererId field_id,
      const std::u16string& backup_password,
      const std::optional<autofill::FormData>& submitted_form);

  void OnSubmitWithEnterResult(
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      bool success);

  void OnPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> content);

  OptimizationGuideKeyedService* GetOptimizationService();

  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  void OnFormSubmitted();

  void OnButtonClicked(bool result);

  void OnSubmissionDetectedOrTimeout();

  base::OneShotTimer timeout_timer_;
  const raw_ptr<content::WebContents> web_contents_;
  base::OnceCallback<void(bool)> callback_;
  raw_ptr<ModelQualityLogsUploader> logs_uploader_;
  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;

  bool submission_detected_ = false;

  std::unique_ptr<PasswordChangeSubmissionVerifier> submission_verifier_;
  base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
      capture_annotated_page_content_;

  std::unique_ptr<ButtonClickHelper> click_helper_;

  base::WeakPtrFactory<ChangePasswordFormFillingSubmissionHelper>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FILLING_SUBMISSION_HELPER_H_
