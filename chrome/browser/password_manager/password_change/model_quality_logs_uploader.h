// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_MODEL_QUALITY_LOGS_UPLOADER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_MODEL_QUALITY_LOGS_UPLOADER_H_

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

class Profile;

// Helper class which handles Model Logging Quality logic and uploads the
// logs to the Server.
class ModelQualityLogsUploader {
 public:
  explicit ModelQualityLogsUploader(Profile* profile);
  ~ModelQualityLogsUploader();
  ModelQualityLogsUploader(const ModelQualityLogsUploader&) = delete;
  ModelQualityLogsUploader& operator=(const ModelQualityLogsUploader&) = delete;
  // As we only want to record one log per flow, this is to be called just
  // once. It will merge all existing LogAiDataRequest and upload a single
  // log entry to the model quality logging service.
  void UploadFinalLog();
  // Merges the logging data with the response given by
  // optimization service call.
  void MergeData(
      const optimization_guide::proto::PasswordChangeResponse& response,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);
#if defined(UNIT_TEST)
  // Used for testing only.
  const std::vector<optimization_guide::proto::LogAiDataRequest>&
  GetLogEntryRequestsForTesting() const {
    return log_entries_requests_;
  }
#endif

 private:
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> CreateNewLogEntry();
  void AddFinalModelStatusLog(
      optimization_guide::proto::FinalModelStatus final_model_status,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  // Holds all feature's logging data request to at the end of the
  // flow merge them in a single log entry.
  std::vector<optimization_guide::proto::LogAiDataRequest>
      log_entries_requests_;
  const raw_ptr<Profile> profile_;
  base::WeakPtrFactory<ModelQualityLogsUploader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_MODEL_QUALITY_LOGS_UPLOADER_H_
