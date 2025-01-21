// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_start_crd_session_job.h"
#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"
#include "chrome/browser/ash/policy/remote_commands/crd/start_crd_session_job_delegate.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
using CallbackWithResult =
    base::OnceCallback<void(policy::ResultType result,
                            std::optional<std::string> payload)>;
constexpr char kSpotlightConnectionCode[] = "123";
constexpr char kUserEmail[] = "cat@gmail.com";

class MockDeviceCommandStartCrdSessionJob
    : public policy::DeviceCommandStartCrdSessionJob {
 public:
  explicit MockDeviceCommandStartCrdSessionJob(
      policy::StartCrdSessionJobDelegate& delegate)
      : policy::DeviceCommandStartCrdSessionJob(delegate, "") {}
  MOCK_METHOD(bool,
              ParseCommandPayload,
              (const std::string& command_payload),
              (override));
  MOCK_METHOD(void, TerminateImpl, (), (override));
  MOCK_METHOD(void, RunImpl, (CallbackWithResult callback), (override));
};

class SpotlightCrdManagerImplTest : public testing::Test {
 public:
  SpotlightCrdManagerImplTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({ash::features::kBocaSpotlight},
                                          /*disabled_features=*/{});
    auto crd_job =
        std::make_unique<NiceMock<MockDeviceCommandStartCrdSessionJob>>(
            delegate_);
    crd_job_ = crd_job.get();
    manager_ = std::make_unique<SpotlightCrdManagerImpl>(std::move(crd_job));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::FakeStartCrdSessionJobDelegate delegate_;
  std::unique_ptr<SpotlightCrdManagerImpl> manager_;
  raw_ptr<NiceMock<MockDeviceCommandStartCrdSessionJob>> crd_job_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SpotlightCrdManagerImplTest, OnSessionStarted) {
  base::Value::Dict payload;
  payload.Set("idlenessCutoffSec", 0);
  payload.Set("terminateUponInput", false);
  payload.Set("ackedUserPresence", true);
  payload.Set("crdSessionType", policy::CrdSessionType::REMOTE_SUPPORT_SESSION);
  payload.Set("showConfirmationDialog", false);
  payload.Set("adminEmail", kUserEmail);

  std::optional<std::string> json_payload = base::WriteJson(payload);
  EXPECT_TRUE(json_payload.has_value());
  EXPECT_CALL(*crd_job_, ParseCommandPayload(json_payload.value())).Times(1);

  manager_->OnSessionStarted(kUserEmail);
}

TEST_F(SpotlightCrdManagerImplTest, OnSessionEnded) {
  EXPECT_CALL(*crd_job_, TerminateImpl).Times(1);

  manager_->OnSessionEnded();
}

TEST_F(SpotlightCrdManagerImplTest, InitiateSpotlightSession) {
  EXPECT_CALL(*crd_job_, RunImpl(_))
      .WillOnce(WithArg<0>(Invoke([&](auto callback) {
        std::move(callback).Run(policy::ResultType::kSuccess,
                                "{\"accessCode\":\"123\"}");
      })));
  base::test::TestFuture<std::optional<std::string>> future;

  manager_->OnSessionStarted(kUserEmail);
  manager_->InitiateSpotlightSession(future.GetCallback());

  auto result = future.Get();
  EXPECT_EQ(kSpotlightConnectionCode, result.value());
}

TEST_F(SpotlightCrdManagerImplTest, InitiateSpotlightSessionWithCrdFailure) {
  EXPECT_CALL(*crd_job_, RunImpl(_))
      .WillOnce(WithArg<0>(Invoke([&](auto callback) {
        std::move(callback).Run(policy::ResultType::kFailure, std::nullopt);
      })));
  base::test::TestFuture<std::optional<std::string>> future;

  manager_->OnSessionStarted(kUserEmail);
  manager_->InitiateSpotlightSession(future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace ash::boca
