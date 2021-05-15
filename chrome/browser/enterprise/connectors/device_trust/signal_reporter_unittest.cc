// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for DeviceTrustSignalReporter.

#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_signal_reporter.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::Invoke;

namespace enterprise_connectors {

class DeviceTrustSignalReporterForTest
    : public DeviceTrustSignalReporterForTestBase {
 public:
  using DeviceTrustSignalReporterForTestBase::
      DeviceTrustSignalReporterForTestBase;
  reporting::MockReportQueue* GetReportQueue() { return mock_queue_; }

  MOCK_METHOD(policy::DMToken, GetDmToken, (), (const override));
  // Invoke these to mock GetDmToken success/failure.
  policy::DMToken GetDmTokenFailure() const {
    // Original class' GetDmToken() would fail because BrowserDMTokenStorage
    // depends on policy in production. But this helps us test that code branch.
    return DeviceTrustSignalReporter::GetDmToken();
  }
  policy::DMToken GetDmTokenSuccess() const {
    // The ReporterForTestBase class returns a dummy token.
    return DeviceTrustSignalReporterForTestBase::GetDmToken();
  }

  MOCK_METHOD(QueueConfigStatusOr,
              CreateQueueConfiguration,
              (const std::string&, base::RepeatingCallback<bool()>),
              (const override));
  // Invoke these to mock CreateQueueConfiguration success/failure.
  QueueConfigStatusOr CreateQueueConfigurationSuccess(
      const std::string& dm_token,
      base::RepeatingCallback<bool()> policy_check) const {
    return DeviceTrustSignalReporter::CreateQueueConfiguration(dm_token,
                                                               policy_check);
  }
  QueueConfigStatusOr CreateQueueConfigurationFailure(
      const std::string& dm_token,
      base::RepeatingCallback<bool()> policy_check) const {
    return reporting::ReportQueueConfiguration::Create(
        dm_token, reporting::Destination::UNDEFINED_DESTINATION, {});
  }

  // Need to mock reporting::ReportQueueProvider::CreateQueue because 1) it
  // replies on CloudPolicyClient in production, and 2) so that unit tests can
  // mock queue creation success and failure.
  using DeviceTrustSignalReporter::QueueCreation;
  using DeviceTrustSignalReporter::SetQueueCreationForTesting;
  // Invoke these to mock queue creation success/failure.
  using DeviceTrustSignalReporterForTestBase::CreateMockReportQueueAndCallback;
  using DeviceTrustSignalReporterForTestBase::FailCreateReportQueueAndCallback;
};

class DeviceTrustSignalReporterTest : public testing::Test {
 public:
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

 protected:
  using Reporter = DeviceTrustSignalReporterForTest;
  void ExpectQueueCreationSuccess(bool success) {
    Reporter::QueueCreation f =
        base::BindOnce(success ? &Reporter::CreateMockReportQueueAndCallback
                               : &Reporter::FailCreateReportQueueAndCallback,
                       base::Unretained(&reporter_));
    reporter_.SetQueueCreationForTesting(std::move(f));
  }

  bool PolicyCheck() {
    ++policy_checked_;
    return true;
  }

  void OnCreateQueueResult(bool success) {
    ++create_queue_callbacked_;
    create_queue_success_ += success;
    std::move(quit_closure_).Run();
  }

  Reporter reporter_;
  int create_queue_callbacked_ = 0;
  int create_queue_success_ = 0;
  int policy_checked_ = 0;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_closure_;
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
  content::BrowserTaskEnvironment task_environment_;
};

class DeviceTrustSignalReporter_SendReportTest
    : public DeviceTrustSignalReporterTest {
 public:
  void SetUp() override {
    DeviceTrustSignalReporterTest::SetUp();

    // Setup to successfully initialize the report queue.
    EXPECT_CALL(reporter_, GetDmToken())
        .WillOnce(Invoke(&reporter_, &Reporter::GetDmTokenSuccess));
    EXPECT_CALL(reporter_, CreateQueueConfiguration(_, _))
        .WillOnce(
            Invoke(&reporter_, &Reporter::CreateQueueConfigurationSuccess));
    ExpectQueueCreationSuccess(true);

    // Initialize the queue and the setup above should result in success.
    InitQueue();
    ASSERT_EQ(create_queue_callbacked_, 1);
    ASSERT_EQ(create_queue_success_, 1);
    ASSERT_NE(reporter_.GetReportQueue(), nullptr);

    // Mock AddRecord() to store message received so that its content can be
    // verified.
    EXPECT_CALL(*(reporter_.GetReportQueue()), AddRecord(_, _, _))
        .WillRepeatedly(
            Invoke([this](base::StringPiece str, reporting::Priority priority,
                          reporting::MockReportQueue::EnqueueCallback cb) {
              msg_received_ = str.data();
              std::move(cb).Run(reporting::Status::StatusOK());
            }));
  }

 protected:
  DeviceTrustSignalReporter::Callback MakeEnqueueCallback() {
    return base::BindOnce(
        &DeviceTrustSignalReporter_SendReportTest::EnqueueCallback,
        base::Unretained(this));
  }

  int msg_added_ = 0;
  int msg_callbacked_ = 0;
  std::string msg_received_;

 private:
  void EnqueueCallback(bool success) {
    ++msg_callbacked_;
    msg_added_ += success;
    std::move(quit_closure_).Run();
  }
};

TEST_F(DeviceTrustSignalReporter_SendReportTest, base_Value) {
  std::string val_json;
  base::Value val;

  for (int i = 1; i < 3; ++i) {
    // Prepare the test message & its serialized version for comparison.
    val = base::Value(base::Value::Type::DICTIONARY);
    val.SetIntKey("test_field", i);
    ASSERT_TRUE(base::JSONWriter::Write(val, &val_json));

    // Send the test message.
    run_loop_ = std::make_unique<base::RunLoop>();
    quit_closure_ = run_loop_->QuitClosure();
    reporter_.SendReport(std::move(val), MakeEnqueueCallback());
    run_loop_->Run();

    // Check that the message was sent properly.
    ASSERT_EQ(msg_callbacked_, i);
    ASSERT_EQ(msg_added_, i);
    ASSERT_EQ(msg_received_, val_json);
  }
}

TEST_F(DeviceTrustSignalReporter_SendReportTest, proto) {
  std::string report_serialized;
  DeviceTrustReportEvent report;
  auto* credential = report.mutable_attestation_credential();
  credential->set_format(
      DeviceTrustReportEvent::Credential::EC_NID_X9_62_PRIME256V1_PUBLIC_DER);

  for (int i = 1; i < 3; ++i) {
    // Prepare the test message & its serialized version for comparison.
    credential->set_credential(base::StringPrintf("test credential %d", i));
    ASSERT_TRUE(report.SerializeToString(&report_serialized));

    // Send the test message.
    run_loop_ = std::make_unique<base::RunLoop>();
    quit_closure_ = run_loop_->QuitClosure();
    reporter_.SendReport(&report, MakeEnqueueCallback());
    run_loop_->Run();

    // Check that the message was sent properly.
    ASSERT_EQ(msg_callbacked_, i);
    ASSERT_EQ(msg_added_, i);
    ASSERT_EQ(msg_received_, report_serialized);
  }
}

class DeviceTrustSignalReporter_InitQueueTest
    : public DeviceTrustSignalReporterTest {
  void SetUp() override {}
};

TEST_F(DeviceTrustSignalReporter_InitQueueTest, Success) {
  EXPECT_CALL(reporter_, GetDmToken())
      .WillOnce(Invoke(&reporter_, &Reporter::GetDmTokenSuccess));
  EXPECT_CALL(reporter_, CreateQueueConfiguration(_, _))
      .WillOnce(Invoke(&reporter_, &Reporter::CreateQueueConfigurationSuccess));
  ExpectQueueCreationSuccess(true);

  // Initialize the queue and the setup above should result in success.
  InitQueue();
  ASSERT_EQ(create_queue_callbacked_, 1);
  ASSERT_EQ(create_queue_success_, 1);

  // Try Init() again to make sure it goes into the switch case
  // create_queue_status_ == CreateQueueStatus::DONE.
  InitQueue();
  ASSERT_EQ(create_queue_callbacked_, 2);
  ASSERT_EQ(create_queue_success_, 2);

  ASSERT_NE(reporter_.GetReportQueue(), nullptr);
}

class DeviceTrustSignalReporter_InitQueueFailureTest
    : public DeviceTrustSignalReporterTest {
 protected:
  void TestInitQueue() {
    // Initialize the queue and the setup above should result in failure.
    InitQueue();
    ASSERT_EQ(create_queue_callbacked_, 1);
    ASSERT_EQ(create_queue_success_, 0);
    ASSERT_EQ(reporter_.GetReportQueue(), nullptr);

    // Try Init() again to make sure it goes into the switch case
    // create_queue_status_ == CreateQueueStatus::DONE but after failure.
    InitQueue();
    ASSERT_EQ(create_queue_callbacked_, 2);
    ASSERT_EQ(create_queue_success_, 0);
    ASSERT_EQ(reporter_.GetReportQueue(), nullptr);
  }
};

TEST_F(DeviceTrustSignalReporter_InitQueueFailureTest, InGetDmToken) {
  EXPECT_CALL(reporter_, GetDmToken())
      .WillOnce(Invoke(&reporter_, &Reporter::GetDmTokenFailure));
  EXPECT_CALL(reporter_, CreateQueueConfiguration(_, _)).Times(0);

  TestInitQueue();
}

TEST_F(DeviceTrustSignalReporter_InitQueueFailureTest, InCreateQueueConfig) {
  EXPECT_CALL(reporter_, GetDmToken())
      .WillOnce(Invoke(&reporter_, &Reporter::GetDmTokenSuccess));
  // Setup to fail in CreateQueueConfiguration.
  EXPECT_CALL(reporter_, CreateQueueConfiguration(_, _))
      .WillOnce(Invoke(&reporter_, &Reporter::CreateQueueConfigurationFailure));

  TestInitQueue();
}

TEST_F(DeviceTrustSignalReporter_InitQueueFailureTest, InCreateReportQueue) {
  EXPECT_CALL(reporter_, GetDmToken())
      .WillOnce(Invoke(&reporter_, &Reporter::GetDmTokenSuccess));
  EXPECT_CALL(reporter_, CreateQueueConfiguration(_, _))
      .WillOnce(Invoke(&reporter_, &Reporter::CreateQueueConfigurationSuccess));
  ExpectQueueCreationSuccess(false);

  TestInitQueue();
}

}  // namespace enterprise_connectors
