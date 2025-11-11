// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ActorLoginQualityLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    logs_uploader_ = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        &pref_service_);
    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        pref_service_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        pref_service_.registry());
  }

  void TearDown() override { logs_uploader_ = nullptr; }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_uploader_;
};

TEST_F(ActorLoginQualityLoggerTest, UploadFinalLogNoMetadata) {
  ActorLoginQualityLogger logger;
  logger.UploadFinalLog(logs_uploader_.get());
  // Since the log has no metadata, no log is uploaded.
  EXPECT_EQ(0u, logs_uploader_->uploaded_logs().size());
}

TEST_F(ActorLoginQualityLoggerTest, UploadFinalLogHandlesNull) {
  ActorLoginQualityLogger logger;
  logger.UploadFinalLog(nullptr);
  // No log is available because the service is null.
  EXPECT_TRUE(logs_uploader_->uploaded_logs().empty());
}
