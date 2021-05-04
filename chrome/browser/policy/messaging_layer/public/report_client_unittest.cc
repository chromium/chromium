// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::SizeIs;
using ::testing::WithArgs;

namespace reporting {
namespace {

class ReportClientTest : public testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Set up fake primary profile.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<ash::FakeChromeUserManager>>();
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath(FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef")));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "12345"));
    const user_manager::User* user =
        mock_user_manager->AddPublicAccountUser(account_id);
    mock_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // Provide a mock cloud policy client.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("FAKE_DM_TOKEN");
    test_reporting_ =
        std::make_unique<ReportingClient::TestEnvironment>(client_.get());

    scoped_feature_list_.InitAndEnableFeature(
        ReportQueueProvider::kEncryptedReportingPipeline);
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.reset();
    profile_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  // BrowserTaskEnvironment must be instantiated before other classes that posts
  // tasks.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  const std::string dm_token_ = "TOKEN";
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

// Tests that a ReportQueue can be created using the ReportingClient.
TEST_F(ReportClientTest, CreatesReportQueue) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);

  test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> a;
  ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                   a.cb());
  ASSERT_OK(a.result());
}

// Ensures that created ReportQueues are actually different.
TEST_F(ReportClientTest, CreatesTwoDifferentReportQueues) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> a1;
  ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                   a1.cb());
  auto result = a1.result();
  ASSERT_OK(result);
  auto report_queue_1 = std::move(result.ValueOrDie());

  test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> a2;
  config_result = ReportQueueConfiguration::Create(dm_token_, destination_,
                                                   policy_checker_callback_);
  ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                   a2.cb());
  result = a2.result();
  ASSERT_OK(result);
  auto report_queue_2 = std::move(result.ValueOrDie());

  EXPECT_NE(report_queue_1.get(), report_queue_2.get());
}

// Creates queue, enqueues messages and verifies they are uploaded.
TEST_F(ReportClientTest, EnqueueMessageAndUpload) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> create_queue_event;
  ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                   create_queue_event.cb());
  auto result = create_queue_event.result();
  ASSERT_OK(result);
  auto report_queue = std::move(result.ValueOrDie());

  test::TestEvent<Status> enqueue_record_event;
  report_queue->Enqueue("Record", FAST_BATCH, enqueue_record_event.cb());
  ASSERT_OK(enqueue_record_event.result());

  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([](base::Value payload,
                    policy::CloudPolicyClient::ResponseCallback done_cb) {
            base::Value* const records = payload.FindListKey("encryptedRecord");
            ASSERT_THAT(records, Ne(nullptr));
            base::Value::ListView records_list = records->GetList();
            ASSERT_THAT(records_list, SizeIs(1));
            base::Value* const seq_info =
                records_list[0].FindDictKey("sequenceInformation");
            ASSERT_THAT(seq_info, Ne(nullptr));
            base::Value response{base::Value::Type::DICTIONARY};
            response.SetPath("lastSucceedUploadedRecord", std::move(*seq_info));
            std::move(done_cb).Run(std::move(response));
          })));
  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}
}  // namespace
}  // namespace reporting
