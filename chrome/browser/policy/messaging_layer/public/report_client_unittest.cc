// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/record_constants.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef OS_CHROMEOS
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // OS_CHROMEOS

namespace reporting {
namespace {

using policy::DMToken;
using reporting::Destination;
using reporting::Priority;

class TestCallbackWaiter {
 public:
  TestCallbackWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {}

  virtual void Signal() { run_loop_->Quit(); }

  void Wait() { run_loop_->Run(); }
  void Reset() {
    run_loop_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
};

class ReportingClientTest : public testing::Test {
 public:
  void SetUp() override {
#ifdef OS_CHROMEOS
    // Set up fake primary profile.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<chromeos::FakeChromeUserManager>>();
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
#endif  // OS_CHROMEOS

    // Provide a mock cloud policy client.
    auto client = std::make_unique<policy::MockCloudPolicyClient>();
    client->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
    ReportingClient::Setup_test(std::move(client));
  }

  void TearDown() override {
    ReportingClient::Reset_test();
#ifdef OS_CHROMEOS
    user_manager_.reset();
    profile_.reset();
#endif  // OS_CHROMEOS
  }

 protected:
  content::BrowserTaskEnvironment task_envrionment_;
#ifdef OS_CHROMEOS
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // OS_CHROMEOS
  const DMToken dm_token_ = DMToken::CreateValidTokenForTesting("TOKEN");
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  const Priority priority_ = Priority::IMMEDIATE;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

// Tests that a ReportQueue can be created using the ReportingClient.
TEST_F(ReportingClientTest, CreatesReportQueue) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  ASSERT_OK(config_result);

  TestCallbackWaiter waiter;
  StatusOr<std::unique_ptr<ReportQueue>> result;
  auto create_report_queue_cb = base::BindOnce(
      [](TestCallbackWaiter* waiter,
         StatusOr<std::unique_ptr<ReportQueue>>* result,
         StatusOr<std::unique_ptr<ReportQueue>> create_result) {
        *result = std::move(create_result);
        waiter->Signal();
      },
      &waiter, &result);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     std::move(create_report_queue_cb));

  waiter.Wait();
  waiter.Reset();
  ASSERT_OK(result);
}

// Ensures that created ReportQueues are actually different.
TEST_F(ReportingClientTest, CreatesTwoDifferentReportQueues) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  TestCallbackWaiter waiter;
  StatusOr<std::unique_ptr<ReportQueue>> result;
  auto create_report_queue_cb = base::BindOnce(
      [](TestCallbackWaiter* waiter,
         StatusOr<std::unique_ptr<ReportQueue>>* result,
         StatusOr<std::unique_ptr<ReportQueue>> create_result) {
        *result = std::move(create_result);
        waiter->Signal();
      },
      &waiter, &result);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     std::move(create_report_queue_cb));
  waiter.Wait();
  waiter.Reset();
  ASSERT_OK(result);
  auto report_queue_1 = std::move(result.ValueOrDie());

  config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  create_report_queue_cb = base::BindOnce(
      [](TestCallbackWaiter* waiter,
         StatusOr<std::unique_ptr<ReportQueue>>* result,
         StatusOr<std::unique_ptr<ReportQueue>> create_result) {
        *result = std::move(create_result);
        waiter->Signal();
      },
      &waiter, &result);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     std::move(create_report_queue_cb));
  waiter.Wait();
  ASSERT_OK(result);

  auto report_queue_2 = std::move(result.ValueOrDie());

  EXPECT_NE(report_queue_1.get(), report_queue_2.get());
}

}  // namespace
}  // namespace reporting
