// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for DeviceTrustSignalReporter.

#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_signal_reporter.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace enterprise_connectors {

class DeviceTrustSignalReporterForTest
    : public DeviceTrustSignalReporterForTestBase {
 public:
  using DeviceTrustSignalReporterForTestBase::
      DeviceTrustSignalReporterForTestBase;
  reporting::MockReportQueue* GetReportQueue() { return mock_queue_; }

  // Mocking this method because 1) it replies on CloudPolicyClient in
  // production, and 2) so that unit tests can mock queue creation success and
  // failure.
  MOCK_METHOD(void,
              PostCreateReportQueueTask,
              (reporting::ReportQueueProvider::CreateReportQueueCallback,
               std::unique_ptr<reporting::ReportQueueConfiguration>));

  // Invoke this method upon calling PostCreateReportQueueTask to mock queue
  // creation success.
  using DeviceTrustSignalReporterForTestBase::CreateMockReportQueueAndCallback;
  // Invoke this method upon calling PostCreateReportQueueTask to mock queue
  // creation failure.
  using DeviceTrustSignalReporterForTestBase::FailCreateReportQueueAndCallback;
};

class DeviceTrustSignalReporterTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        reporting::ReportQueueProvider::kEncryptedReportingPipeline);
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
      // Mock AddRecord() to store message received so that its content can be
      // verified.
      EXPECT_CALL(*(reporter_.GetReportQueue()), AddRecord(_, _, _))
          .WillRepeatedly(
              Invoke([this](base::StringPiece val, reporting::Priority priority,
                            reporting::MockReportQueue::EnqueueCallback cb) {
                base::Optional<base::Value> msg_result =
                    base::JSONReader::Read(val);
                ASSERT_TRUE(msg_result.has_value());
                msg_received_ = std::move(msg_result.value());
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
