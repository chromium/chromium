// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"

namespace optimization_guide {
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

// Implementation of actor_login::ActorLoginQualityLoggerInterface
class ActorLoginQualityLogger
    : public actor_login::ActorLoginQualityLoggerInterface {
 public:
  ActorLoginQualityLogger();
  ~ActorLoginQualityLogger() override;
  ActorLoginQualityLogger(const ActorLoginQualityLogger&) = delete;
  ActorLoginQualityLogger& operator=(const ActorLoginQualityLogger&) = delete;

  // actor_login::ActorLoginQualityLoggerInterface:
  void UploadFinalLog(optimization_guide::ModelQualityLogsUploaderService*
                          mqls_uploader) const override;

  base::WeakPtr<ActorLoginQualityLogger> AsWeakPtr();

 private:
  optimization_guide::proto::LogAiDataRequest log_data_;
  base::WeakPtrFactory<ActorLoginQualityLogger> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_
