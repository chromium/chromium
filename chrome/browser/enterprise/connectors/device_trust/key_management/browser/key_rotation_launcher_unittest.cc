// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mock_key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

constexpr char kNonce[] = "nonce";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeClientId[] = "fake-client-id";
constexpr char kExpectedDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

}  // namespace

class KeyRotationLauncherTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_command =
        std::make_unique<testing::StrictMock<test::MockKeyRotationCommand>>();
    mock_command_ = mock_command.get();
    scoped_command_factory_.SetMock(std::move(mock_command));

    launcher_ = KeyRotationLauncher::Create(&fake_dm_token_storage_,
                                            &fake_device_management_service_,
                                            test_shared_loader_factory_);
  }

  void SetDMToken() {
    // Set valid values.
    fake_dm_token_storage_.SetDMToken(kFakeDMToken);
    fake_dm_token_storage_.SetClientId(kFakeClientId);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<testing::StrictMock<test::MockKeyRotationCommand>, DanglingUntriaged>
      mock_command_;
  ScopedKeyRotationCommandFactory scoped_command_factory_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_{
      &job_creation_handler_};
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::unique_ptr<KeyRotationLauncher> launcher_;
};

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation) {
  SetDMToken();

  std::optional<KeyRotationCommand::Params> params;
  EXPECT_CALL(*mock_command_, Trigger(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&params](const KeyRotationCommand::Params given_params,
                    KeyRotationCommand::Callback callback) {
            params = given_params;
            std::move(callback).Run(KeyRotationCommand::Status::SUCCEEDED);
          }));

  launcher_->LaunchKeyRotation(kNonce, base::DoNothing());

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kNonce, params->nonce);
  EXPECT_EQ(kFakeDMToken, params->dm_token);
  EXPECT_EQ(kExpectedDmServerUrl, params->dm_server_url);
}

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation_InvalidDMTokenStorage) {
  auto launcher = KeyRotationLauncher::Create(
      nullptr, &fake_device_management_service_, test_shared_loader_factory_);

  base::test::TestFuture<KeyRotationCommand::Status> future;
  launcher->LaunchKeyRotation(kNonce, future.GetCallback());

  EXPECT_EQ(future.Get(),
            KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN_STORAGE);
}

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation_InvalidDMToken) {
  // Set the DM token to an invalid value (i.e. empty string).
  fake_dm_token_storage_.SetDMToken("");

  base::test::TestFuture<KeyRotationCommand::Status> future;
  launcher_->LaunchKeyRotation(kNonce, future.GetCallback());

  EXPECT_EQ(future.Get(), KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN);
}

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation_InvalidManagementService) {
  SetDMToken();

  auto launcher = KeyRotationLauncher::Create(&fake_dm_token_storage_, nullptr,
                                              test_shared_loader_factory_);

  base::test::TestFuture<KeyRotationCommand::Status> future;
  launcher->LaunchKeyRotation(kNonce, future.GetCallback());

  EXPECT_EQ(future.Get(),
            KeyRotationCommand::Status::FAILED_INVALID_MANAGEMENT_SERVICE);
}

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation_InvalidCommand) {
  SetDMToken();
  scoped_command_factory_.ReturnInvalidCommand();

  base::test::TestFuture<KeyRotationCommand::Status> future;
  launcher_->LaunchKeyRotation(kNonce, future.GetCallback());

  EXPECT_EQ(future.Get(), KeyRotationCommand::Status::FAILED_INVALID_COMMAND);
}

}  // namespace enterprise_connectors
