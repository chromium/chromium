// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/chrome_management_service.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/metrics_utils.h"
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

constexpr char kChromeManagementServiceStatusHistogramName[] =
    "Enterprise.DeviceTrust.ManagementService.Error";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeDmServerUrl[] = "www.fake_url.com";
constexpr char kFakeNonce[] = "fake-nonce";
constexpr char kStringPipeName[] = "0";
constexpr uint64_t kUintPipeName = 0;

}  // namespace

class ChromeManagementServiceTest : public testing::Test {
 public:
  static int TestRunProcess(const std::string& process_name,
                            const bool& permissions_callback_result,
                            const int& rotation_callback_result) {
    base::test::SingleThreadTaskEnvironment single_threaded_task_environment;
    base::MockCallback<ChromeManagementService::PermissionsCallback>
        mock_permissions_callback;
    base::MockCallback<ChromeManagementService::RotationCallback>
        mock_rotation_callback;

    auto* command_line = base::CommandLine::ForCurrentProcess();

    if (process_name != "CommandFailure") {
      EXPECT_CALL(mock_permissions_callback, Run())
          .WillOnce([&permissions_callback_result, &process_name]() {
            if (process_name == "PermissionsFailure") {
              // Since this is mocked, a failure metric needs to be recorded
              // here.
              enterprise_connectors::RecordError(
                  ManagementServiceError::kBinaryMissingManagementGroupID);
            }
            return permissions_callback_result;
          });
    }

    if (rotation_callback_result == kSuccess ||
        rotation_callback_result == kFailure) {
      EXPECT_CALL(mock_rotation_callback, Run(_))
          .WillOnce([&rotation_callback_result]() {
            return rotation_callback_result;
          });
    } else {
      EXPECT_CALL(mock_rotation_callback, Run(_)).Times(0);
    }

    ChromeManagementService chrome_management_service = ChromeManagementService(
        mock_permissions_callback.Get(), mock_rotation_callback.Get());

    return chrome_management_service.Run(command_line, kUintPipeName);
  }

 protected:
  base::CommandLine GetTestCommandLine() {
    auto test_command_line = base::GetMultiProcessTestChildBaseCommandLine();
    test_command_line.AppendSwitchASCII(switches::kRotateDTKey, kFakeDMToken);
    test_command_line.AppendSwitchASCII(switches::kPipeName, kStringPipeName);
    test_command_line.AppendSwitchASCII(switches::kDmServerUrl,
                                        kFakeDmServerUrl);
    test_command_line.AppendSwitchASCII(switches::kNonce, kFakeNonce);
    return test_command_line;
  }

  base::Process LaunchProcessAndSendInvitation(
      const std::string& process_name,
      base::CommandLine test_command_line) {
    mojo::PlatformChannel channel;
    mojo::OutgoingInvitation invitation;
    mojo::ScopedMessagePipeHandle pipe =
        invitation.AttachMessagePipe(kUintPipeName);
    auto pending_receiver =
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>(
            std::move(pipe));
    test_url_loader_factory_.Clone(std::move(pending_receiver));

    base::LaunchOptions options;
    if (process_name != "MojoFailure") {
      channel.PrepareToPassRemoteEndpoint(&options, &test_command_line);
    }
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
  base::HistogramTester histogram_tester;
  int res =
      ChromeManagementServiceTest::TestRunProcess("Successful", true, kSuccess);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
  return res;
}

TEST_F(ChromeManagementServiceTest, Success_Nonce) {
  auto child_process =
      LaunchProcessAndSendInvitation("Successful", GetTestCommandLine());

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

TEST_F(ChromeManagementServiceTest, Success_NoNonce) {
  auto test_command_line = base::GetMultiProcessTestChildBaseCommandLine();
  test_command_line.AppendSwitchASCII(switches::kRotateDTKey, kFakeDMToken);
  test_command_line.AppendSwitchASCII(switches::kPipeName, kStringPipeName);
  test_command_line.AppendSwitchASCII(switches::kDmServerUrl, kFakeDmServerUrl);
  auto child_process =
      LaunchProcessAndSendInvitation("Successful", test_command_line);

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

// Tests when the chrome management service failed due to a missing
// rotate dtkey switch.
MULTIPROCESS_TEST_MAIN(CommandFailure) {
  base::HistogramTester histogram_tester;

  int res =
      ChromeManagementServiceTest::TestRunProcess("CommandFailure", false, 2);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kCommandMissingRotateDTKey, 1);

  return res;
}

TEST_F(ChromeManagementServiceTest, Failure_IncorrectCommand) {
  auto test_command_line = base::GetMultiProcessTestChildBaseCommandLine();
  test_command_line.AppendSwitchASCII(switches::kPipeName, kStringPipeName);
  test_command_line.AppendSwitchASCII(switches::kDmServerUrl, kFakeDmServerUrl);
  test_command_line.AppendSwitchASCII(switches::kNonce, kFakeNonce);
  auto child_process =
      LaunchProcessAndSendInvitation("CommandFailure", test_command_line);

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to issues with accepting
// the mojo invitation and connecting to the browser process.
MULTIPROCESS_TEST_MAIN(MojoFailure) {
  base::HistogramTester histogram_tester;
  int res = ChromeManagementServiceTest::TestRunProcess("MojoFailure", true, 2);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kInvalidUrlLoaderFactory, 1);

  return res;
}

TEST_F(ChromeManagementServiceTest, Mojo_Failure) {
  base::HistogramTester histogram_tester;

  auto child_process =
      LaunchProcessAndSendInvitation("MojoFailure", GetTestCommandLine());

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to incorrect
// process permissions.
MULTIPROCESS_TEST_MAIN(PermissionsFailure) {
  base::HistogramTester histogram_tester;
  int res = ChromeManagementServiceTest::TestRunProcess("PermissionsFailure",
                                                        false, 2);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kBinaryMissingManagementGroupID, 1);
  return res;
}

TEST_F(ChromeManagementServiceTest, Failure_IncorrectPermissions) {
  auto child_process = LaunchProcessAndSendInvitation("PermissionsFailure",
                                                      GetTestCommandLine());

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to a failed
// key rotation.
MULTIPROCESS_TEST_MAIN(RotateDTKeyFailure) {
  base::HistogramTester histogram_tester;
  int res = ChromeManagementServiceTest::TestRunProcess("RotateDTKeyFailure",
                                                        true, kFailure);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
  return res;
}

TEST_F(ChromeManagementServiceTest, Failure_RotateDTKeyFailure) {
  auto child_process = LaunchProcessAndSendInvitation("RotateDTKeyFailure",
                                                      GetTestCommandLine());

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

}  // namespace enterprise_connectors
