// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_FORM_SUBMISSION_VERIFIER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_FORM_SUBMISSION_VERIFIER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
class PasswordManagerDriver;
}  // namespace password_manager

namespace optimization_guide {
class ModelQualityLogEntry;
}

// Helper class which submits a form and verifies submission result. Upon
// completion invokes FormSubmissionResultCallback callback to notify the result
// of submission.
class ChangeFormSubmissionVerifier {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SubmissionOutcome)
  enum class SubmissionOutcome {
    kUncategorizedError = 0,
    // Successful outcome, new password saved in credentials.
    kSuccess = 1,
    // Unknown outcome, password is pre-saved.
    kUnknown = 2,
    // Failure cases, password is not saved.
    kErrorOldPasswordIncorrect = 3,
    kErrorOldPasswordDoNotMatch = 4,
    kErrorNewPasswordIncorrect = 5,
    kPageError = 6,
    kNoResponse = 7,
    kCouldNotParse = 8,
    kMaxValue = kCouldNotParse,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:SubmissionOutcome)

  using FormSubmissionResultCallback = base::OnceCallback<void(bool)>;
  static constexpr base::TimeDelta kSubmissionWaitingTimeout =
      base::Seconds(10);
  static constexpr char kPasswordChangeVerificationTimeHistogram[] =
      "PasswordManager.PasswordChangeVerificationTime";
  static constexpr char kPasswordChangeSubmittedHistogram[] =
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically";

  ChangeFormSubmissionVerifier(content::WebContents* web_contents,
                               FormSubmissionResultCallback callback);
  ~ChangeFormSubmissionVerifier();

  // Starts chain of actions:
  // * fills and submits a change password form observed by `form_manager`,
  // * pre-saves a credential with new_password,
  // * provisionally saves submitted password.
  void FillChangePasswordForm(
      password_manager::PasswordFormManager* form_manager,
      const std::u16string& old_password,
      const std::u16string& new_password);

  // Triggers verification if `web_contents` is the same as initial WebContents.
  void OnPasswordFormSubmission(content::WebContents* web_contents);

  // Saves a password with a given `username`. Must be called only after
  // `callback_` was invoked.
  void SavePassword(const std::u16string& username);

  // Returns current URL from the `form_manager_`.
  GURL GetURL() const;

 private:
  void TriggerFilling(
      const password_manager::PasswordForm& form,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      const std::u16string& old_password,
      const std::u16string& new_password);

  void ChangePasswordFormFilled(
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      const autofill::FormData& submitted_form);

  void RequestAXTree();

  void ProcessTree(ui::AXTreeUpdate& ax_tree_update);
  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  base::OneShotTimer timeout_timer_;
  base::WeakPtr<content::WebContents> web_contents_;
  FormSubmissionResultCallback callback_;
  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;

  bool submission_detected_ = false;
  bool password_filled_ = false;

  base::WeakPtrFactory<ChangeFormSubmissionVerifier> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_FORM_SUBMISSION_VERIFIER_H_
