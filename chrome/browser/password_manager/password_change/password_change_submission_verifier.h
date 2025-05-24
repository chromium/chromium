// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_SUBMISSION_VERIFIER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_SUBMISSION_VERIFIER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {
class WebContents;
}
class ModelQualityLogsUploader;
class OptimizationGuideKeyedService;

// Helper class which verifies whether password change was successful or not.
class PasswordChangeSubmissionVerifier {
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

  static constexpr char kPasswordChangeVerificationTimeHistogram[] =
      "PasswordManager.PasswordChangeVerificationTime";

  PasswordChangeSubmissionVerifier(content::WebContents* web_contents,
                                   ModelQualityLogsUploader* logs_uploader);
  ~PasswordChangeSubmissionVerifier();

  void CheckSubmissionOutcome(FormSubmissionResultCallback callback);

#if defined(UNIT_TEST)
  void set_annotated_page_callback(
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
          callback) {
    capture_annotated_page_content_ = std::move(callback);
  }
#endif

 private:
  void OnAnnotatedPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> page_content);
  void OnAxTreeReceived(ui::AXTreeUpdate& ax_tree_update);
  void CheckSubmissionSuccessful();
  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  OptimizationGuideKeyedService* GetOptimizationService() const;

  base::WeakPtr<content::WebContents> web_contents_;
  base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
      capture_annotated_page_content_;
  FormSubmissionResultCallback callback_;
  raw_ptr<ModelQualityLogsUploader> logs_uploader_;
  // TODO(crbug.com/409946698): Delete this when removing support for AX tree
  // prompts.
  optimization_guide::proto::PasswordChangeRequest
      check_submission_successful_request_;

  base::Time server_request_start_time_;

  base::WeakPtrFactory<PasswordChangeSubmissionVerifier> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_SUBMISSION_VERIFIER_H_
