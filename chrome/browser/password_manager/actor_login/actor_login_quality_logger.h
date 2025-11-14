// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace optimization_guide {
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

// Manages Model Logging Quality and uploads logs to the server.
// Each log corresponds to a single filling, which means there would be
// at most one GetCredentials request, and at most two AttemptLogin requests.
// The second AttemptLogin request can happen if the first one failed with
// kErrorDeviceReauthRequired.
class ActorLoginQualityLogger {
 public:
  ActorLoginQualityLogger();
  ~ActorLoginQualityLogger();
  ActorLoginQualityLogger(const ActorLoginQualityLogger&) = delete;
  ActorLoginQualityLogger& operator=(const ActorLoginQualityLogger&) = delete;

  // To be called when the trajectory is finished and the final log should
  // be uploaded to the server.
  void UploadFinalLog(
      optimization_guide::ModelQualityLogsUploaderService* mqls_uploader);

 private:
  optimization_guide::proto::LogAiDataRequest log_data_;
  base::WeakPtrFactory<ActorLoginQualityLogger> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_
