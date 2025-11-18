// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/language/core/browser/language_model.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using translate::testing::MockTranslateClient;
using translate::testing::MockTranslateDriver;
using translate::testing::MockTranslateRanker;

using ActorLoginQuality = optimization_guide::proto::ActorLoginQuality;
using GetCredentialsDetails =
    optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails;
using AttemptLoginDetails =
    optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails;
using FillingFormResult = optimization_guide::proto::
    ActorLoginQuality_AttemptLoginDetails_FillingFormResult;
using PermissionDetails =
    optimization_guide::proto::ActorLoginQuality_PermissionOption;

// Expect two protos to be equal if they are serialized into the same strings.
MATCHER_P(ProtoEquals, expected_message, "") {
  std::string expected_serialized, actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}
class FakeLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

void VerifyUniqueLogDetails(
    const std::vector<
        std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>& logs,
    const GetCredentialsDetails& expected_details) {
  ASSERT_EQ(logs.size(), 1u);

  const auto& log_entry = logs[0];
  ASSERT_TRUE(log_entry->has_actor_login());
  ASSERT_TRUE(log_entry->actor_login().has_quality());
  EXPECT_THAT(log_entry->actor_login().quality().get_credentials_details(),
              ProtoEquals(expected_details));
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
  EXPECT_EQ(0u, logs_uploader_->uploaded_logs().size());
}

TEST_F(ActorLoginQualityLoggerTest, UploadFinalLogHandlesNull) {
  ActorLoginQualityLogger logger;
  logger.UploadFinalLog(nullptr);
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

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader_->WaitForLogUpload(log_uploaded_signal.GetCallback());
  logger.UploadFinalLog(logs_uploader_.get());
  ASSERT_TRUE(log_uploaded_signal.Wait());

  VerifyUniqueLogDetails(logs_uploader_->uploaded_logs(), expected_details);
}

TEST_F(ActorLoginQualityLoggerTest, SetsDomainAndLanguage) {
  ActorLoginQualityLogger logger;

  translate::testing::MockTranslateDriver translate_driver;
  auto mock_translate_ranker =
      std::make_unique<translate::testing::MockTranslateRanker>();
  auto mock_translate_client =
      std::make_unique<MockTranslateClient>(&translate_driver, nullptr);
  auto language_model = std::make_unique<FakeLanguageModel>();
  auto translate_manager = std::make_unique<translate::TranslateManager>(
      mock_translate_client.get(), mock_translate_ranker.get(),
      language_model.get());
  translate_manager->GetLanguageState()->SetSourceLanguage("en-us");

  const GURL url1("https://subdomain.example.com/login");
  const GURL url2("https://someotherdomain.com");
  logger.SetDomainAndLanguage(translate_manager.get(), url1);
  logger.SetDomainAndLanguage(translate_manager.get(), url2);

  // Only the first domain should be recorded and only the eTLD+1.
  ActorLoginQuality expected_log;
  expected_log.set_domain("example.com");
  expected_log.set_language("en-us");

  EXPECT_THAT(logger.get_log_data(), ProtoEquals(expected_log));
}

TEST_F(ActorLoginQualityLoggerTest, LogsLocation) {
  // Required by the `VariationsService`.
  base::test::TaskEnvironment task_environment;
  // Set variation service.
  variations::TestVariationsService::RegisterPrefs(pref_service_.registry());
  auto enabled_state_provider =
      std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/true,
                                                          /*enabled=*/true);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          &pref_service_, enabled_state_provider.get(),
          /*backup_registry_key=*/std::wstring(),
          /*user_data_dir=*/base::FilePath(),
          metrics::StartupVisibility::kUnknown);
  auto variations_service = std::make_unique<variations::TestVariationsService>(
      &pref_service_, metrics_state_manager.get());
  TestingBrowserProcess::GetGlobal()->SetVariationsService(
      variations_service.get());

  // This pref directly overrides any country detection logic within the
  // variations service.
  pref_service_.SetString(variations::prefs::kVariationsCountry, "US");

  ActorLoginQuality expected_log;
  expected_log.set_location("US");

  ActorLoginQualityLogger logger;
  EXPECT_THAT(logger.get_log_data(), ProtoEquals(expected_log));

  TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
}

TEST_F(ActorLoginQualityLoggerTest, AddAttemptLoginDetails) {
  ActorLoginQualityLogger logger;

  optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
      expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  FillingFormResult* form_result = expected_details.add_filling_form_result();
  form_result->set_was_password_filled(true);
  logger.AddAttemptLoginDetails(expected_details);
  logger.AddAttemptLoginDetails(expected_details);

  ASSERT_EQ(logger.get_log_data().attempt_login_details().size(), 2);
  EXPECT_THAT(logger.get_log_data().attempt_login_details(0),
              ProtoEquals(expected_details));
}

TEST_F(ActorLoginQualityLoggerTest, SetsPermissionDetails) {
  ActorLoginQualityLogger logger;

  PermissionDetails expected_permission =
      optimization_guide::proto::ActorLoginQuality_PermissionOption_ALLOW_ONCE;
  logger.SetPermissionPicked(expected_permission);
  EXPECT_EQ(logger.get_log_data().permission_picked(), expected_permission);
}
