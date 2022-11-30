// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/ml_agent.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace ml {
namespace {

// Arbitrary inactivity score for the fake ml service connection to return, and
// its quantization via sigmoid transform:
constexpr double kTestInactivityScore = -3.7;
constexpr int kQuantizedTestInactivityScore = 2;

// Quantization of k20190521ModelDefaultDimThreshold (-0.5), the builtin
// threshold for SmartDimModelV3, via sigmoid.
// It's higher than kTestInactivityScore , which implies a no dim decision.
constexpr int kQuantizedBuiltinThreshold = 37;

// Arbitrary dim thresholds lower than kTestInactivityScore and its quantization
// via sigmoid transform, implying a yes dim decisions.
constexpr double kLowDimThreshold = -10.0;
constexpr int kQuantizedLowDimThreshold = 0;

// Test data lies in src/chromeos/test/data/smart_dim.
base::FilePath GetTestDataPath(const std::string& file_name) {
  base::FilePath path;
  CHECK(chromeos::test_utils::GetTestDataPath("smart_dim", file_name, &path));
  return path;
}

void LoadDownloadableSmartDimComponent(const double& threshold) {
  const char json_string_template[] =
      "{"
      "\"input_nodes\": [3],"
      "\"output_nodes\": [5],"
      "\"threshold\": %f,"
      "\"expected_feature_size\": 343,"
      "\"metrics_model_name\": \"SmartDimModel\""
      "}";
  const std::string json_string =
      base::StringPrintf(json_string_template, threshold);

  const std::string model_string = "This is a model string";

  std::string pb_string;
  const base::FilePath pb_path =
      GetTestDataPath("20181115_example_preprocessor_config.pb");
  CHECK(base::ReadFileToString(pb_path, &pb_string));

  SmartDimMlAgent::GetInstance()->OnComponentReady(
      std::make_tuple(json_string, pb_string, model_string));
}

UserActivityEvent::Features DefaultFeatures() {
  UserActivityEvent::Features features;
  // Bucketize to 95.
  features.set_battery_percent(96.0);
  features.set_device_management(UserActivityEvent::Features::UNMANAGED);
  features.set_device_mode(UserActivityEvent::Features::CLAMSHELL);
  features.set_device_type(UserActivityEvent::Features::CHROMEBOOK);
  // Bucketize to 200.
  features.set_key_events_in_last_hour(290);
  features.set_last_activity_day(UserActivityEvent::Features::THU);
  // Bucketize to 7.
  features.set_last_activity_time_sec(25920);
  // Bucketize to 7.
  features.set_last_user_activity_time_sec(25920);
  // Bucketize to 2000.
  features.set_mouse_events_in_last_hour(2600);
  features.set_on_battery(false);
  features.set_previous_negative_actions_count(3);
  features.set_previous_positive_actions_count(0);
  features.set_recent_time_active_sec(190);
  features.set_video_playing_time_sec(0);
  features.set_on_to_dim_sec(30);
  features.set_dim_to_screen_off_sec(10);
  features.set_time_since_last_key_sec(30);
  features.set_time_since_last_mouse_sec(688);
  // Bucketize to 900.
  features.set_time_since_video_ended_sec(1100);
  features.set_has_form_entry(false);
  features.set_source_id(123);  // not used.
  features.set_engagement_score(40);
  features.set_tab_domain("//mail.google.com");
  return features;
}

// Checks that |prediction| contains the specified expected decision threshold,
// score, and response. Sets |callback_done| to true so that this can be used to
// check RequestDimDecision runs its callback.
void CheckResult(bool* callback_done,
                 const int expected_threshold,
                 const int expected_score,
                 UserActivityEvent::ModelPrediction::Response expected_response,
                 UserActivityEvent::ModelPrediction prediction) {
  EXPECT_EQ(expected_response, prediction.response());
  EXPECT_EQ(expected_threshold, prediction.decision_threshold());
  EXPECT_EQ(expected_score, prediction.inactivity_score());

  *callback_done = true;
}

}  // namespace

class SmartDimMlAgentTest : public testing::Test {
 public:
  SmartDimMlAgentTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::IO,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  SmartDimMlAgentTest(const SmartDimMlAgentTest&) = delete;
  SmartDimMlAgentTest& operator=(const SmartDimMlAgentTest&) = delete;

  void SetUp() override {
    chromeos::MachineLearningClient::InitializeFake();
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
    fake_service_connection_.SetOutputValue(
        std::vector<int64_t>{1L}, std::vector<double>{kTestInactivityScore});
  }

  void TearDown() override { chromeos::MachineLearningClient::Shutdown(); }

 protected:
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
  // DownloadWorker::InitializeFromComponent posts task to BrowserThread::UI,
  // while content::BrowserTaskEnvironment provides BrowserThread support in
  // unittest.
  content::BrowserTaskEnvironment task_environment_;

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

// This test covers two things:
// 1. ml_agent can swap between download worker and builtin worker as per
// IsDownloadWorkerReady.
// 2. ml_agent can combine results from worker with threshold to get right
// DIM/NO_DIM decisions.
TEST_F(SmartDimMlAgentTest, SwitchBetweenWorkers) {
  auto* agent = SmartDimMlAgent::GetInstance();
  agent->ResetForTesting();

  // Without LoadDownloadableSmartDimComponent, download_worker_ is not ready.
  EXPECT_FALSE(agent->IsDownloadWorkerReady());

  bool callback_done = false;
  // By checking prediction.decision_threshold == kQuantizedBuiltinThreshold we
  // know that builtin worker is at work. This threshold is high, so the
  // decision is NO_DIM.
  agent->RequestDimDecision(
      DefaultFeatures(),
      base::BindOnce(&CheckResult, &callback_done, kQuantizedBuiltinThreshold,
                     kQuantizedTestInactivityScore,
                     UserActivityEvent::ModelPrediction::NO_DIM));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_done);

  // After load from download components, it should use download worker.
  LoadDownloadableSmartDimComponent(kLowDimThreshold);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(agent->IsDownloadWorkerReady());

  callback_done = false;
  // By checking prediction.decision_threshold == kQuantizedLowDimThreshold we
  // know that download worker is at work. This threshold is low, so the
  // decision is DIM.
  agent->RequestDimDecision(
      DefaultFeatures(),
      base::BindOnce(&CheckResult, &callback_done, kQuantizedLowDimThreshold,
                     kQuantizedTestInactivityScore,
                     UserActivityEvent::ModelPrediction::DIM));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

// Check that CancelableOnceCallback ensures a callback doesn't execute twice,
// in case two RequestDimDecision() calls were made before any callback ran.
TEST_F(SmartDimMlAgentTest, CheckCancelableCallback) {
  SmartDimMlAgent::GetInstance()->ResetForTesting();

  bool callback_done = false;
  int num_callbacks_run = 0;
  for (int i = 0; i < 2; i++) {
    SmartDimMlAgent::GetInstance()->RequestDimDecision(
        DefaultFeatures(),
        base::BindOnce(
            [](bool* callback_done, int* num_callbacks_run,
               UserActivityEvent::ModelPrediction prediction) {
              *callback_done = true;
              (*num_callbacks_run)++;
            },
            &callback_done, &num_callbacks_run));
  }
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_done);
  EXPECT_EQ(1, num_callbacks_run);
}

// Check that CancelPreviousRequest() can successfully prevent a previous
// requested dim decision request from running.
TEST_F(SmartDimMlAgentTest, CheckCanceledRequest) {
  SmartDimMlAgent::GetInstance()->ResetForTesting();

  bool callback_done = false;
  SmartDimMlAgent::GetInstance()->RequestDimDecision(
      DefaultFeatures(), base::BindOnce(
                             [](bool* callback_done,
                                UserActivityEvent::ModelPrediction prediction) {
                               *callback_done = true;
                             },
                             &callback_done));
  SmartDimMlAgent::GetInstance()->CancelPreviousRequest();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_done);
}

// Check that when ML service fails to load model or create graph executor,
// download_worker is initially ready, then eventually marked not ready.
TEST_F(SmartDimMlAgentTest, LoadModelFailure) {
  SmartDimMlAgent::GetInstance()->ResetForTesting();

  // Make fake_service_connection_ fail to load models and turn it to async_mode
  // to fake the real ml-service loading a bad flatbuffer model.
  fake_service_connection_.SetLoadModelFailure();
  fake_service_connection_.SetAsyncMode(true);

  // Before ml-service responds loading failure, OnConnectionError isn't
  // invoked, download_worker_ is set to ready (fake-ready).
  LoadDownloadableSmartDimComponent(kLowDimThreshold);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(SmartDimMlAgent::GetInstance()->IsDownloadWorkerReady());

  // Requests during the fake-ready status doesn't crash.
  bool callback_done = false;
  SmartDimMlAgent::GetInstance()->RequestDimDecision(
      DefaultFeatures(), base::BindOnce(
                             [](bool* callback_done,
                                UserActivityEvent::ModelPrediction prediction) {
                               *callback_done = true;
                             },
                             &callback_done));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_done);

  // Ml-service responds loading failure, OnConnectionError is invoked,
  // download_worker_ is set to not ready.
  fake_service_connection_.RunPendingCalls();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(SmartDimMlAgent::GetInstance()->IsDownloadWorkerReady());

  // Reset fake_service_connection_ so that builtin_worker can process requests.
  fake_service_connection_.SetAsyncMode(false);
  fake_service_connection_.SetExecuteSuccess();
  // Requests after the fake-ready status can be processed successfully.
  SmartDimMlAgent::GetInstance()->RequestDimDecision(
      DefaultFeatures(), base::BindOnce(
                             [](bool* callback_done,
                                UserActivityEvent::ModelPrediction prediction) {
                               *callback_done = true;
                             },
                             &callback_done));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

}  // namespace ml
}  // namespace power
}  // namespace ash
