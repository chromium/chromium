// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using GetCredentialsDetails =
    optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails;

// Expect two protos to be equal if they are serialized into the same strings.
MATCHER_P(ProtoEquals, expected_message, "") {
  std::string expected_serialized, actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

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

TEST_F(ActorLoginQualityLoggerTest, SetsGetCredentialsDetails) {
  ActorLoginQualityLogger logger;

  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_SIGN_IN_FORM);
  expected_details.set_permission_details(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_HAS_PERMANENT_PERMISSION);
  expected_details.set_getting_credentials_time_ms(5);
  logger.SetGetCredentialsDetails(expected_details);

  GetCredentialsDetails get_credentials_details =
      logger.get_log_data().get_credentials_details();
  EXPECT_THAT(get_credentials_details, ProtoEquals(expected_details));
}
