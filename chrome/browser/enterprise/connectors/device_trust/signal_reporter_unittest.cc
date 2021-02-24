// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for DeviceTrustSignalReporter.

#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/messaging_layer/public/mock_report_queue.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
namespace enterprise_connectors {

class DeviceTrustSignalReporterForTest : public DeviceTrustSignalReporter {
 public:
  using DeviceTrustSignalReporter::DeviceTrustSignalReporter;
  reporting::MockReportQueue* GetReportQueue() { return mock_queue_; }

  // Mocking this method because 1) it replies on CloudPolicyClient in
  // production, and 2) so that unit tests can mock queue creation success and
  // failure.
  MOCK_METHOD(void,
              PostCreateReportQueueTask,
              (reporting::ReportingClient::CreateReportQueueCallback,
               std::unique_ptr<reporting::ReportQueueConfiguration>));

  // Invoke this method upon calling PostCreateReportQueueTask to mock queue
  // creation success.
  void CreateMockReportQueueAndCallback(
      reporting::ReportingClient::CreateReportQueueCallback create_queue_cb,
      std::unique_ptr<reporting::ReportQueueConfiguration> config) {
    mock_queue_ = new testing::StrictMock<reporting::MockReportQueue>();
    std::move(create_queue_cb)
        .Run({std::unique_ptr<reporting::ReportQueue>(mock_queue_)});
  }

  // Invoke this method upon calling PostCreateReportQueueTask to mock queue
  // creation success.
  void FailCreateReportQueueAndCallback(
      reporting::ReportingClient::CreateReportQueueCallback create_queue_cb,
      std::unique_ptr<reporting::ReportQueueConfiguration> config) {
    std::move(create_queue_cb)
        .Run(
            reporting::Status(reporting::error::INTERNAL,
                              "Mocked ReportQueue creation failure for tests"));
  }

 protected:
  // Overriding this method as it relies on CloudPolicyClient in production.
  policy::DMToken GetDmToken() const override {
    return policy::DMToken::CreateValidTokenForTesting("dummy_token");
  }

 private:
  reporting::MockReportQueue* mock_queue_{nullptr};
  base::OnceCallback<void(void)> create_queue_cb_;
};

class DeviceTrustSignalReporterTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        reporting::ReportingClient::kEncryptedReportingPipeline);
  }

  void InitQueue() {
    run_loop_ = std::make_unique<base::RunLoop>();
    quit_closure_ = run_loop_->QuitClosure();
    reporter_.Init(
        base::BindRepeating(&DeviceTrustSignalReporterTest::PolicyCheck,
                            base::Unretained(this)),
        base::BindRepeating(&DeviceTrustSignalReporterTest::OnCreateQueueResult,
                            base::Unretained(this)));
    run_loop_->Run();
  }

  bool PolicyCheck() {
    ++policy_checked_;
    return true;
  }

  void OnCreateQueueResult(bool success) {
    ++create_queue_callbacked_;
    create_queue_success_ += success;

    if (success) {
      ASSERT_NE(reporter_.GetReportQueue(), nullptr);
      EXPECT_CALL(*(reporter_.GetReportQueue()), ValueEnqueue_(_, _, _))
          .WillRepeatedly(Invoke(
              [this](const base::Value& val, reporting::Priority priority,
                     reporting::MockReportQueue::EnqueueCallback cb) {
                msg_received_ = val.Clone();
                std::move(cb).Run(reporting::Status::StatusOK());
              }));
    }

    std::move(quit_closure_).Run();
  }

  void EnqueueCallback(bool success) {
    ++msg_callbacked_;
    msg_added_ += success;
    std::move(quit_closure_).Run();
  }

 protected:
  DeviceTrustSignalReporterForTest reporter_;
  int create_queue_callbacked_ = 0;
  int create_queue_success_ = 0;
  int policy_checked_ = 0;
  int msg_added_ = 0;
  int msg_callbacked_ = 0;
  base::Value msg_received_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_closure_;
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DeviceTrustSignalReporterTest, SendReportNormal) {
  // Setup to create a MockReportQueue in PostCreateReportQueueTask.
  EXPECT_CALL(reporter_, PostCreateReportQueueTask(_, _))
      .WillRepeatedly(Invoke(
          &reporter_,
          &DeviceTrustSignalReporterForTest::CreateMockReportQueueAndCallback));

  // Initialize the queue and the setup above should result in success.
  InitQueue();
  ASSERT_EQ(create_queue_callbacked_, 1);
  ASSERT_EQ(create_queue_success_, 1);

  // Try Init() again to make sure it goes into the switch case
  // create_queue_status_ == CreateQueueStatus::DONE.
  InitQueue();
  ASSERT_EQ(create_queue_callbacked_, 2);
  ASSERT_EQ(create_queue_success_, 2);

  base::Value val;
  for (int i = 1; i < 3; ++i) {
    val = base::Value(base::Value::Type::DICTIONARY);
    val.SetIntKey("test_field", i);

    run_loop_ = std::make_unique<base::RunLoop>();
    quit_closure_ = run_loop_->QuitClosure();
    reporter_.SendReport(
        std::move(val),
        base::BindOnce(&DeviceTrustSignalReporterTest::EnqueueCallback,
                       base::Unretained(this)));
    run_loop_->Run();

    ASSERT_EQ(msg_callbacked_, i);
    ASSERT_EQ(msg_added_, i);
    ASSERT_EQ(msg_received_.FindPath("test_field")->GetInt(), i);
  }
}

TEST_F(DeviceTrustSignalReporterTest, InitQueueFailure) {
  // Setup to fail in PostCreateReportQueueTask.
  EXPECT_CALL(reporter_, PostCreateReportQueueTask(_, _))
      .WillRepeatedly(Invoke(
          &reporter_,
          &DeviceTrustSignalReporterForTest::FailCreateReportQueueAndCallback));

  // Initialize the queue and the setup above should result in failure.
  InitQueue();
  ASSERT_EQ(create_queue_callbacked_, 1);
  ASSERT_EQ(create_queue_success_, 0);

  // Try Init() again to make sure it goes into the switch case
  // create_queue_status_ == CreateQueueStatus::DONE but after failure.
  InitQueue();
  ASSERT_EQ(create_queue_callbacked_, 2);
  ASSERT_EQ(create_queue_success_, 0);

  ASSERT_EQ(reporter_.GetReportQueue(), nullptr);
}

}  // namespace enterprise_connectors
