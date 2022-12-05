// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"

#include <winerror.h>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
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
  std::string token_base64;
  base::Base64Encode(kFakeDMToken, &token_base64);
  std::string nonce_base64;
  base::Base64Encode(kFakeDMToken, &token_base64);
  base::Base64Encode(kNonce, &nonce_base64);
  EXPECT_EQ(token_base64, args[0]);
  EXPECT_EQ(kFakeDmServerUrl, args[1]);
  EXPECT_EQ(nonce_base64, args[2]);
}

}  // namespace

class WinKeyRotationCommandTest : public testing::Test {
 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WinKeyRotationCommandTest, RotateSuccess) {
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};
  bool was_called = false;
  KeyRotationCommand::Status status = KeyRotationCommand::Status::TIMED_OUT;

  WinKeyRotationCommand command(base::BindRepeating(
      [](const wchar_t* command, const std::vector<std::string>& args,
         DWORD* return_code) {
        CheckCommandArgs(args);
        *return_code = installer::ROTATE_DTKEY_SUCCESS;
        return S_OK;
      }));

  command.Trigger(params, base::BindLambdaForTesting(
                              [&was_called,
                               &status](KeyRotationCommand::Status arg_status) {
                                was_called = true;
                                status = arg_status;
                              }));

  RunUntilIdle();

  ASSERT_TRUE(was_called);
  ASSERT_EQ(KeyRotationCommand::Status::SUCCEEDED, status);
}

TEST_F(WinKeyRotationCommandTest, RotateFailure) {
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};
  bool was_called = false;
  KeyRotationCommand::Status status = KeyRotationCommand::Status::TIMED_OUT;

  WinKeyRotationCommand command(base::BindRepeating(
      [](const wchar_t* command, const std::vector<std::string>& args,
         DWORD* return_code) {
        CheckCommandArgs(args);
        *return_code = installer::ROTATE_DTKEY_FAILED;
        return S_OK;
      }));

  command.Trigger(params, base::BindLambdaForTesting(
                              [&was_called,
                               &status](KeyRotationCommand::Status arg_status) {
                                was_called = true;
                                status = arg_status;
                              }));

  RunUntilIdle();

  ASSERT_TRUE(was_called);
  ASSERT_EQ(KeyRotationCommand::Status::FAILED, status);
}

TEST_F(WinKeyRotationCommandTest, RotateTimeout) {
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};
  bool was_called = false;
  KeyRotationCommand::Status status = KeyRotationCommand::Status::FAILED;

  WinKeyRotationCommand command(base::BindRepeating(
      [](const wchar_t* command, const std::vector<std::string>& args,
         DWORD* return_code) {
        CheckCommandArgs(args);
        // Not setting return_code.
        return E_ABORT;
      }));

  command.Trigger(params, base::BindLambdaForTesting(
                              [&was_called,
                               &status](KeyRotationCommand::Status arg_status) {
                                was_called = true;
                                status = arg_status;
                              }));

  RunUntilIdle();

  ASSERT_TRUE(was_called);
  ASSERT_EQ(KeyRotationCommand::Status::TIMED_OUT, status);
}

TEST_F(WinKeyRotationCommandTest, GoogleUpdateIssue) {
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};
  bool was_called = false;
  KeyRotationCommand::Status status = KeyRotationCommand::Status::SUCCEEDED;

  WinKeyRotationCommand command(base::BindRepeating(
      [](const wchar_t* command, const std::vector<std::string>& args,
         DWORD* return_code) {
        CheckCommandArgs(args);
        // Not setting return_code.
        return WinKeyRotationCommand::GOOPDATE_E_APP_USING_EXTERNAL_UPDATER;
      }));

  command.enable_waiting_for_testing(false);
  command.Trigger(params, base::BindLambdaForTesting(
                              [&was_called,
                               &status](KeyRotationCommand::Status arg_status) {
                                was_called = true;
                                status = arg_status;
                              }));

  RunUntilIdle();

  ASSERT_TRUE(was_called);
  ASSERT_EQ(KeyRotationCommand::Status::FAILED, status);
}

TEST_F(WinKeyRotationCommandTest, GeneralFailure) {
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};
  bool was_called = false;
  KeyRotationCommand::Status status = KeyRotationCommand::Status::SUCCEEDED;

  WinKeyRotationCommand command(base::BindRepeating(
      [](const wchar_t* command, const std::vector<std::string>& args,
         DWORD* return_code) {
        CheckCommandArgs(args);
        // Not setting return_code.
        return E_FAIL;
      }));

  command.enable_waiting_for_testing(false);
  command.Trigger(params, base::BindLambdaForTesting(
                              [&was_called,
                               &status](KeyRotationCommand::Status arg_status) {
                                was_called = true;
                                status = arg_status;
                              }));

  RunUntilIdle();

  ASSERT_TRUE(was_called);
  ASSERT_EQ(KeyRotationCommand::Status::FAILED, status);
}

}  // namespace enterprise_connectors
