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
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/password_manager/core/browser/password_form.h"

class AnnotatedPageContentCapturer;
class ModelQualityLogsUploader;

namespace content {
class WebContents;
}

// Helper class which verifies whether password change was successful or not.
class PasswordChangeSubmissionVerifier {
 public:
  static char kPasswordChangeVerificationTimeHistogram[];
  static char kSubmissionOutcomeHistogramName[];

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

  PasswordChangeSubmissionVerifier(content::WebContents* web_contents,
                                   ModelQualityLogsUploader* logs_uploader);
  ~PasswordChangeSubmissionVerifier();

  void CheckSubmissionOutcome(FormSubmissionResultCallback callback);

#if defined(UNIT_TEST)
  AnnotatedPageContentCapturer* capturer() { return capturer_.get(); }
#endif

 private:
  void CheckSubmissionSuccessful(
      optimization_guide::AIPageContentResultOrError page_content);
  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);
  void OnPageLoadCompleted();

  const base::Time creation_time_;
  const raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AnnotatedPageContentCapturer> capturer_;
  FormSubmissionResultCallback callback_;
  raw_ptr<ModelQualityLogsUploader> logs_uploader_;

  base::WeakPtrFactory<PasswordChangeSubmissionVerifier> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_SUBMISSION_VERIFIER_H_
