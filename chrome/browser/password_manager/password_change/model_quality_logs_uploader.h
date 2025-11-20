// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_MODEL_QUALITY_LOGS_UPLOADER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_MODEL_QUALITY_LOGS_UPLOADER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

class Profile;
namespace content {
class WebContents;
}

namespace password_manager {
enum class LogInWithChangedPasswordOutcome;
struct PasswordForm;
}

// Helper class which handles Model Logging Quality logic and uploads the
// logs to the Server.
class ModelQualityLogsUploader {
 public:
  using LoggingData =
      optimization_guide::proto::PasswordChangeSubmissionLoggingData;
  using QualityStatus = optimization_guide::proto::
      PasswordChangeQuality_StepQuality_SubmissionStatus;
  using FlowStep = optimization_guide::proto::PasswordChangeRequest::FlowStep;

  ModelQualityLogsUploader(content::WebContents* web_contents,
                           const GURL& change_password_url);
  ~ModelQualityLogsUploader();
  ModelQualityLogsUploader(const ModelQualityLogsUploader&) = delete;
  ModelQualityLogsUploader& operator=(const ModelQualityLogsUploader&) = delete;

  // As we only want to record one log per flow, this is to be called just
  // once. It will merge the 3 LogAiDataRequest and upload a single
  // log entry to the model quality logging service.
  void UploadFinalLog();

  // Sets quality data for Step=LOGGED_IN_CHECK.
  void SetLoggedInCheckQuality(int retry_count,
                               std::unique_ptr<LoggingData> logging_data);

  // Sets quality data for Step=OPEN_FORM_STEP.
  void SetOpenFormQuality(
      const std::optional<optimization_guide::proto::PasswordChangeResponse>&
          response,
      std::unique_ptr<LoggingData> logging_data);

  // Sets quality data for Step=SUBMIT_FORM_STEP.
  void SetSubmitFormQuality(
      const std::optional<optimization_guide::proto::PasswordChangeResponse>&
          response,
      std::unique_ptr<LoggingData> logging_data);

  // Sets quality data for Step=VERIFY_SUBMISSION_STEP.
  void SetVerifySubmissionQuality(
      const std::optional<optimization_guide::proto::PasswordChangeResponse>&
          response,
      std::unique_ptr<LoggingData> logging_data);

  // To be called if no form is seen after actuating on
  // Step=OPEN_FORM_STEP.
  void FormNotDetectedAfterOpening();

  // To be called if there is an expected failure
  // in Step=OPEN_FORM_STEP (e.g. Page Content is unavailable).
  void SetOpenFormUnexpectedFailure();

  // To be called if the flow is interrupted (e.g., if the tab or dialog are
  // closed).
  void SetFlowInterrupted(FlowStep step, QualityStatus quality_status);

  // Marks a flow step as skipped, indicating no
  // model call was made for this step.
  void MarkStepSkipped(FlowStep step);

  // To be called if element to click was not found
  // in Step=OPEN_FORM_STEP.
  void RecordButtonClickFailure(FlowStep step,
                                actor::mojom::ActionResultCode failure);

  // Called when the leak check is shown to the user. Sets information about the
  // password form which triggered the leak check.
  void SetLoginPasswordFormInfo(
      const password_manager::PasswordForm& password_form);

  // Called when APC flow discovers the change password form. Logs password form
  // information, e. g. form signature, fields & buttons texts.
  void SetChangePasswordFormData(
      const password_manager::PasswordForm& password_form);

  void SetStepDuration(FlowStep step, base::TimeDelta duration);

  // Records the outcome of the first login attempt
  // using a previously saved APC-password and immediately
  // uploads it to the server.
  static void RecordLoginAttemptQuality(
      optimization_guide::ModelQualityLogsUploaderService* mqls_service,
      const GURL& page_url,
      password_manager::LogInWithChangedPasswordOutcome login_outcome);

#if defined(UNIT_TEST)
  // Used for testing only.
  const optimization_guide::proto::LogAiDataRequest& GetFinalLog() const {
    return final_log_data_;
  }
#endif

 private:
  const base::Time flow_start_time_;
  optimization_guide::proto::LogAiDataRequest final_log_data_;
  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<ModelQualityLogsUploader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_MODEL_QUALITY_LOGS_UPLOADER_H_
