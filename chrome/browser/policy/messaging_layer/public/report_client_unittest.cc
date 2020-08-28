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

// #if defined(OS_CHROMEOS)
// #include "chrome/browser/chromeos/settings/device_settings_service.h"
// #include "components/policy/proto/chrome_device_policy.pb.h"
// #else
// #include "chrome/browser/policy/chrome_browser_policy_connector.h"
// #endif

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
    auto client = std::make_unique<policy::MockCloudPolicyClient>();
    client->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
    ReportingClient::Setup_test(std::move(client));
  }

  void TearDown() override { ReportingClient::Reset_test(); }

 protected:
  content::BrowserTaskEnvironment task_envrionment_;
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
