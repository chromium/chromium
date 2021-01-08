// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
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

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // OS_CHROMEOS

namespace reporting {
namespace {

using policy::DMToken;
using reporting::Destination;

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType* res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

class ReportClientTest : public testing::Test {
 public:
  void SetUp() override {
#if defined(OS_CHROMEOS)
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
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
    test_reporting_ =
        std::make_unique<ReportingClient::TestEnvironment>(client_.get());

    scoped_feature_list_.InitAndEnableFeature(
        ReportingClient::kEncryptedReportingPipeline);
  }

  void TearDown() override {
#if defined(OS_CHROMEOS)
    user_manager_.reset();
    profile_.reset();
#endif  // OS_CHROMEOS
  }

 protected:
  content::BrowserTaskEnvironment task_envrionment_;
  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // OS_CHROMEOS
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  const DMToken dm_token_ = DMToken::CreateValidTokenForTesting("TOKEN");
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a ReportQueue can be created using the ReportingClient.
TEST_F(ReportClientTest, CreatesReportQueue) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);

  TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> a;
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     a.cb());
  ASSERT_OK(a.result());
}

// Ensures that created ReportQueues are actually different.
TEST_F(ReportClientTest, CreatesTwoDifferentReportQueues) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> a1;
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     a1.cb());
  auto result = a1.result();
  ASSERT_OK(result);
  auto report_queue_1 = std::move(result.ValueOrDie());

  TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> a2;
  config_result = ReportQueueConfiguration::Create(dm_token_, destination_,
                                                   policy_checker_callback_);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     a2.cb());
  result = a2.result();
  ASSERT_OK(result);
  auto report_queue_2 = std::move(result.ValueOrDie());

  EXPECT_NE(report_queue_1.get(), report_queue_2.get());
}

}  // namespace
}  // namespace reporting
