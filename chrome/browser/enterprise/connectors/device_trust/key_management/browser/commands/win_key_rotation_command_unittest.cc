// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"

#include <winerror.h>

#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/metrics_utils.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

const char kNonce[] = "nonce";
const char kFakeDMToken[] = "fake-browser-dm-token";
const char kFakeDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

void CheckCommandArgs(const std::vector<std::string>& args) {
  std::string token_base64 = base::Base64Encode(kFakeDMToken);
  std::string nonce_base64 = base::Base64Encode(kNonce);
  EXPECT_EQ(token_base64, args[0]);
  EXPECT_EQ(kFakeDmServerUrl, args[1]);
  EXPECT_EQ(nonce_base64, args[2]);
}

}  // namespace

class WinKeyRotationCommandTest : public testing::Test {
 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void RunTest(
      HRESULT command_hresult,
      std::optional<int> exit_code,
      KeyRotationCommand::Status expected_status,
      std::optional<KeyRotationCommandError> logged_error = std::nullopt,
      bool skip_wait = false) {
    install_static::ScopedInstallDetails install_details(true);

    KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl,
                                         kNonce};

    WinKeyRotationCommand command(base::BindLambdaForTesting(
        [&command_hresult, &exit_code](const wchar_t* command,
                                       const std::vector<std::string>& args,
                                       std::optional<DWORD>* return_code) {
          CheckCommandArgs(args);
          if (exit_code) {
            *return_code = exit_code.value();
          }
          return command_hresult;
        }));

    if (skip_wait) {
      command.enable_waiting_for_testing(false);
    }

    base::test::TestFuture<KeyRotationCommand::Status> future_status;
    command.Trigger(params, future_status.GetCallback());

    EXPECT_EQ(future_status.Get(), expected_status);

    VerifyHistograms(logged_error, exit_code);
  }

  void VerifyHistograms(std::optional<KeyRotationCommandError> error,
                        std::optional<int> exit_code) {
    if (error) {
      histogram_tester_.ExpectUniqueSample(
          "Enterprise.DeviceTrust.KeyRotationCommand.Error", error.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "Enterprise.DeviceTrust.KeyRotationCommand.Error", 0);
    }

    if (exit_code) {
      histogram_tester_.ExpectUniqueSample(
          "Enterprise.DeviceTrust.KeyRotationCommand.ExitCode",
          exit_code.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "Enterprise.DeviceTrust.KeyRotationCommand.ExitCode", 0);
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WinKeyRotationCommandTest, RotateSuccess) {
  RunTest(S_OK, installer::ROTATE_DTKEY_SUCCESS,
          KeyRotationCommand::Status::SUCCEEDED);
}

TEST_F(WinKeyRotationCommandTest, RotateFailure) {
  RunTest(S_OK, installer::ROTATE_DTKEY_FAILED,
          KeyRotationCommand::Status::FAILED);
}

TEST_F(WinKeyRotationCommandTest, RotateTimeout) {
  RunTest(E_ABORT, std::nullopt, KeyRotationCommand::Status::TIMED_OUT,
          KeyRotationCommandError::kTimeout);
}

TEST_F(WinKeyRotationCommandTest, GoogleUpdateConcurrencyIssue) {
  RunTest(WinKeyRotationCommand::GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
          std::nullopt, KeyRotationCommand::Status::FAILED,
          KeyRotationCommandError::kUpdaterConcurrency, /*skip_wait=*/true);
}

TEST_F(WinKeyRotationCommandTest, GeneralFailure) {
  RunTest(S_OK, std::nullopt, KeyRotationCommand::Status::FAILED,
          KeyRotationCommandError::kUnknown);
}

TEST_F(WinKeyRotationCommandTest, COMClassNotRegistered) {
  RunTest(REGDB_E_CLASSNOTREG, std::nullopt,
          KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION,
          KeyRotationCommandError::kClassNotRegistered);
}

TEST_F(WinKeyRotationCommandTest, COMClassNoInterface) {
  RunTest(E_NOINTERFACE, std::nullopt,
          KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION,
          KeyRotationCommandError::kNoInterface);
}

TEST_F(WinKeyRotationCommandTest, UnknownHresult) {
  RunTest(CLASS_E_NOAGGREGATION, std::nullopt,
          KeyRotationCommand::Status::FAILED,
          KeyRotationCommandError::kUnknown);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceTrust.KeyRotationCommand.Error.Hresult",
      static_cast<int>(CLASS_E_NOAGGREGATION), 1);
}

TEST_F(WinKeyRotationCommandTest, UserLevelInstall) {
  install_static::ScopedInstallDetails install_details(false);
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};

  WinKeyRotationCommand command(base::BindLambdaForTesting(
      [](const wchar_t* command, const std::vector<std::string>& args,
         std::optional<DWORD>* return_code) {
        NOTREACHED_IN_MIGRATION() << "Should not get to launching the command.";
        return S_OK;
      }));

  base::test::TestFuture<KeyRotationCommand::Status> future_status;
  command.Trigger(params, future_status.GetCallback());

  EXPECT_EQ(future_status.Get(),
            KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION);
  VerifyHistograms(KeyRotationCommandError::kUserInstallation, std::nullopt);
}

}  // namespace enterprise_connectors
