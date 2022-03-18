// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/chrome_management_service.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;

namespace enterprise_connectors {

namespace {

constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kStringPipeName[] = "0";
constexpr uint64_t kUintPipeName = 0;

}  // namespace

class ChromeManagementServiceTest : public testing::Test {
 public:
  static int TestRunProcess(const bool& permissions_callback_result,
                            const int& rotation_callback_result) {
    base::MockCallback<ChromeManagementService::PermissionsCallback>
        mock_permissions_callback;
    base::MockCallback<ChromeManagementService::RotationCallback>
        mock_rotation_callback;

    auto* command_line = base::CommandLine::ForCurrentProcess();

    EXPECT_CALL(mock_permissions_callback, Run())
        .WillOnce([&permissions_callback_result]() {
          return permissions_callback_result;
        });

    if (rotation_callback_result == kSuccess ||
        rotation_callback_result == kFailure) {
      EXPECT_CALL(mock_rotation_callback, Run(_, _))
          .WillOnce([&rotation_callback_result]() {
            return rotation_callback_result;
          });
    } else {
      EXPECT_CALL(mock_rotation_callback, Run(_, _)).Times(0);
    }

    ChromeManagementService chrome_management_service = ChromeManagementService(
        mock_permissions_callback.Get(), mock_rotation_callback.Get());

    return chrome_management_service.Run(command_line, kUintPipeName);
  }

 protected:
  base::Process LaunchProcessAndSendInvitation(
      const std::string& process_name) {
    auto test_command_line = base::GetMultiProcessTestChildBaseCommandLine();
    test_command_line.AppendSwitchASCII(switches::kRotateDTKey, kFakeDMToken);
    if (process_name != "CommandFailure")
      test_command_line.AppendSwitchASCII(switches::kPipeName, kStringPipeName);

    mojo::PlatformChannel channel;
    mojo::OutgoingInvitation invitation;
    mojo::ScopedMessagePipeHandle pipe =
        invitation.AttachMessagePipe(kUintPipeName);
    auto pending_receiver =
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>(
            std::move(pipe));
    test_url_loader_factory_.Clone(std::move(pending_receiver));

    base::LaunchOptions options;
    channel.PrepareToPassRemoteEndpoint(&options, &test_command_line);
    auto process = base::SpawnMultiProcessTestChild(process_name,
                                                    test_command_line, options);
    mojo::OutgoingInvitation::Send(std::move(invitation), process.Handle(),
                                   channel.TakeLocalEndpoint());
    return process;
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Tests when the chrome management service successfully called to
// rotate the key.
MULTIPROCESS_TEST_MAIN(Successful) {
  return ChromeManagementServiceTest::TestRunProcess(true, kSuccess);
}

TEST_F(ChromeManagementServiceTest, Success) {
  auto child_process = LaunchProcessAndSendInvitation("Successful");

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

// Tests when the chrome management service failed due to a missing
// rotate dtkey switch.
MULTIPROCESS_TEST_MAIN(CommandFailure) {
  return ChromeManagementServiceTest::TestRunProcess(false, 2);
}

TEST_F(ChromeManagementServiceTest, Failure_IncorrectCommand) {
  auto child_process = LaunchProcessAndSendInvitation("CommandFailure");

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to incorrect
// process permissions.
MULTIPROCESS_TEST_MAIN(PermissionsFailure) {
  return ChromeManagementServiceTest::TestRunProcess(false, 2);
}

TEST_F(ChromeManagementServiceTest, Failure_IncorrectPermissions) {
  auto child_process = LaunchProcessAndSendInvitation("PermissionsFailure");

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to a failed
// key rotation.
MULTIPROCESS_TEST_MAIN(RotateDTKeyFailure) {
  return ChromeManagementServiceTest::TestRunProcess(true, kFailure);
}

TEST_F(ChromeManagementServiceTest, Failure_RotateDTKeyFailure) {
  auto child_process = LaunchProcessAndSendInvitation("RotateDTKeyFailure");

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

}  // namespace enterprise_connectors
