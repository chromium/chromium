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
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/mojo_helper/mock_mojo_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/mojo_helper/mojo_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;
using testing::Test;

namespace enterprise_connectors {

namespace {

constexpr char kChromeManagementServiceStatusHistogramName[] =
    "Enterprise.DeviceTrust.ManagementService.Error";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeDmServerUrl[] = "www.fake_url.com";
constexpr char kFakeNonce[] = "fake-nonce";
constexpr char kStringPipeName[] = "0";
constexpr uint64_t kUintPipeName = 0;
constexpr uint16_t kCustomErrorCode = 199;

}  // namespace

using test::MockMojoHelper;

class ChromeManagementServiceTest : public testing::Test {
 public:
  static int TestRunProcess(const std::string& process_name,
                            const bool& permissions_callback_result,
                            const int& rotation_callback_result,
                            std::unique_ptr<MojoHelper> mojo_helper,
                            std::optional<ManagementServiceError> error) {
    base::HistogramTester histogram_tester;
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

    ChromeManagementService chrome_management_service(
        mock_permissions_callback.Get(), mock_rotation_callback.Get(),
        std::move(mojo_helper));

    int res = chrome_management_service.Run(command_line, kUintPipeName);

    if (error) {
      histogram_tester.ExpectUniqueSample(
          kChromeManagementServiceStatusHistogramName, error.value(), 1);
    } else {
      histogram_tester.ExpectTotalCount(
          kChromeManagementServiceStatusHistogramName, 0);
    }

    return Test::HasFailure() ? kCustomErrorCode : res;
  }

  static std::unique_ptr<MockMojoHelper> CreateTestMojoHelper() {
    return std::make_unique<MockMojoHelper>();
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
  return ChromeManagementServiceTest::TestRunProcess(
      "Successful", true, kSuccess, MojoHelper::Create(), std::nullopt);
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
  return ChromeManagementServiceTest::TestRunProcess(
      "CommandFailure", false, 1, MojoHelper::Create(),
      ManagementServiceError::kCommandMissingRotateDTKey);
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

// Tests when the chrome management service failed due to incorrect process
// permissions.
MULTIPROCESS_TEST_MAIN(PermissionsFailure) {
  return ChromeManagementServiceTest::TestRunProcess(
      "PermissionsFailure", false, 1, MojoHelper::Create(),
      ManagementServiceError::kBinaryMissingManagementGroupID);
}

TEST_F(ChromeManagementServiceTest, Failure_IncorrectPermissions) {
  auto child_process = LaunchProcessAndSendInvitation("PermissionsFailure",
                                                      GetTestCommandLine());

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(exit_code, kFailedInsufficientPermissions);
}

// Tests when the chrome management service failed due to an invalid platform
// channel endpoint.
MULTIPROCESS_TEST_MAIN(PlatformChannelEndpointFailure) {
  auto mojo_helper = ChromeManagementServiceTest::CreateTestMojoHelper();
  MockMojoHelper* mock_mojo_helper = mojo_helper.get();

  EXPECT_CALL(*mock_mojo_helper, GetEndpointFromCommandLine(_))
      .Times(1)
      .WillOnce([](const base::CommandLine& command_line) {
        return mojo::PlatformChannelEndpoint();
      });
  return ChromeManagementServiceTest::TestRunProcess(
      "PlatformChannelEndpointFailure", true, 1, std::move(mojo_helper),
      ManagementServiceError::kInvalidPlatformChannelEndpoint);
}

TEST_F(ChromeManagementServiceTest,
       MojoFailure_InvalidPlatformChannelEndpoint) {
  auto child_process = LaunchProcessAndSendInvitation(
      "PlatformChannelEndpointFailure", GetTestCommandLine());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to a failure accepting
// the mojo invitation.
MULTIPROCESS_TEST_MAIN(AcceptMojoInvitationFailure) {
  auto mojo_helper = ChromeManagementServiceTest::CreateTestMojoHelper();
  MockMojoHelper* mock_mojo_helper = mojo_helper.get();

  EXPECT_CALL(*mock_mojo_helper, GetEndpointFromCommandLine(_))
      .Times(1)
      .WillOnce([](const base::CommandLine& command_line) {
        return mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            command_line);
      });

  EXPECT_CALL(*mock_mojo_helper, AcceptMojoInvitation(_))
      .Times(1)
      .WillOnce([](mojo::PlatformChannelEndpoint channel_endpoint) {
        return mojo::IncomingInvitation();
      });

  return ChromeManagementServiceTest::TestRunProcess(
      "AcceptMojoInvitationFailure", true, 1, std::move(mojo_helper),
      ManagementServiceError::kInvalidMojoInvitation);
}

TEST_F(ChromeManagementServiceTest, MojoFailure_AcceptInvitationFailure) {
  auto child_process = LaunchProcessAndSendInvitation(
      "AcceptMojoInvitationFailure", GetTestCommandLine());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to an invalid scoped
// message pipe handle.
MULTIPROCESS_TEST_MAIN(InvalidPipeHandle) {
  auto mojo_helper = ChromeManagementServiceTest::CreateTestMojoHelper();
  MockMojoHelper* mock_mojo_helper = mojo_helper.get();

  EXPECT_CALL(*mock_mojo_helper, GetEndpointFromCommandLine(_))
      .Times(1)
      .WillOnce([](const base::CommandLine& command_line) {
        return mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            command_line);
      });

  EXPECT_CALL(*mock_mojo_helper, AcceptMojoInvitation(_))
      .Times(1)
      .WillOnce([](mojo::PlatformChannelEndpoint channel_endpoint) {
        return mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
      });

  EXPECT_CALL(*mock_mojo_helper, ExtractMojoMessage(_, _))
      .Times(1)
      .WillOnce([](mojo::IncomingInvitation invitation, uint64_t pipe_name) {
        auto pipe_handle = invitation.ExtractMessagePipe(pipe_name);
        pipe_handle.reset();
        return pipe_handle;
      });

  return ChromeManagementServiceTest::TestRunProcess(
      "InvalidPipeHandle", true, 1, std::move(mojo_helper),
      ManagementServiceError::kInvalidMessagePipeHandle);
}

TEST_F(ChromeManagementServiceTest, MojoFailure_InvalidPipeHandle) {
  auto child_process =
      LaunchProcessAndSendInvitation("InvalidPipeHandle", GetTestCommandLine());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to an invalid pending
// remote url loader factory.
MULTIPROCESS_TEST_MAIN(InvalidPendingRemoteUrlLoaderFactory) {
  auto mojo_helper = ChromeManagementServiceTest::CreateTestMojoHelper();
  MockMojoHelper* mock_mojo_helper = mojo_helper.get();

  EXPECT_CALL(*mock_mojo_helper, GetEndpointFromCommandLine(_))
      .Times(1)
      .WillOnce([](const base::CommandLine& command_line) {
        return mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            command_line);
      });

  EXPECT_CALL(*mock_mojo_helper, AcceptMojoInvitation(_))
      .Times(1)
      .WillOnce([](mojo::PlatformChannelEndpoint channel_endpoint) {
        return mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
      });

  EXPECT_CALL(*mock_mojo_helper, ExtractMojoMessage(_, _))
      .Times(1)
      .WillOnce([](mojo::IncomingInvitation invitation, uint64_t pipe_name) {
        return invitation.ExtractMessagePipe(pipe_name);
      });

  EXPECT_CALL(*mock_mojo_helper, CreatePendingRemote(_))
      .Times(1)
      .WillOnce([](mojo::ScopedMessagePipeHandle pipe_handle) {
        return mojo::PendingRemote<network::mojom::URLLoaderFactory>();
      });

  return ChromeManagementServiceTest::TestRunProcess(
      "InvalidPendingRemoteUrlLoaderFactory", true, 1, std::move(mojo_helper),
      ManagementServiceError::kInvalidPendingUrlLoaderFactory);
}

TEST_F(ChromeManagementServiceTest,
       MojoFailure_InvalidPendingRemoteLoaderFactory) {
  auto child_process = LaunchProcessAndSendInvitation(
      "InvalidPendingRemoteUrlLoaderFactory", GetTestCommandLine());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to an unbound remote url
// loader factory.
MULTIPROCESS_TEST_MAIN(UnboundRemoteUrlLoaderFactory) {
  auto mojo_helper = ChromeManagementServiceTest::CreateTestMojoHelper();
  MockMojoHelper* mock_mojo_helper = mojo_helper.get();

  EXPECT_CALL(*mock_mojo_helper, GetEndpointFromCommandLine(_))
      .Times(1)
      .WillOnce([](const base::CommandLine& command_line) {
        return mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            command_line);
      });

  EXPECT_CALL(*mock_mojo_helper, AcceptMojoInvitation(_))
      .Times(1)
      .WillOnce([](mojo::PlatformChannelEndpoint channel_endpoint) {
        return mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
      });

  EXPECT_CALL(*mock_mojo_helper, ExtractMojoMessage(_, _))
      .Times(1)
      .WillOnce([](mojo::IncomingInvitation invitation, uint64_t pipe_name) {
        return invitation.ExtractMessagePipe(pipe_name);
      });

  EXPECT_CALL(*mock_mojo_helper, CreatePendingRemote(_))
      .Times(1)
      .WillOnce([](mojo::ScopedMessagePipeHandle pipe_handle) {
        return mojo::PendingRemote<network::mojom::URLLoaderFactory>(
            std::move(pipe_handle), 0);
      });

  EXPECT_CALL(*mock_mojo_helper, BindRemote(_, _)).Times(1);

  return ChromeManagementServiceTest::TestRunProcess(
      "InvalidPendingRemoteUrlLoaderFactory", true, 1, std::move(mojo_helper),
      ManagementServiceError::kUnBoundUrlLoaderFactory);
}

TEST_F(ChromeManagementServiceTest, MojoFailure_UnboundRemoteLoaderFactory) {
  auto child_process = LaunchProcessAndSendInvitation(
      "UnboundRemoteUrlLoaderFactory", GetTestCommandLine());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to a disconnected remote
// url loader factory.
MULTIPROCESS_TEST_MAIN(DisconnectedRemoteUrlLoaderFactory) {
  auto mojo_helper = ChromeManagementServiceTest::CreateTestMojoHelper();
  MockMojoHelper* mock_mojo_helper = mojo_helper.get();

  EXPECT_CALL(*mock_mojo_helper, GetEndpointFromCommandLine(_))
      .Times(1)
      .WillOnce([](const base::CommandLine& command_line) {
        return mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            command_line);
      });

  EXPECT_CALL(*mock_mojo_helper, AcceptMojoInvitation(_))
      .Times(1)
      .WillOnce([](mojo::PlatformChannelEndpoint channel_endpoint) {
        return mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
      });

  EXPECT_CALL(*mock_mojo_helper, ExtractMojoMessage(_, _))
      .Times(1)
      .WillOnce([](mojo::IncomingInvitation invitation, uint64_t pipe_name) {
        return invitation.ExtractMessagePipe(pipe_name);
      });

  EXPECT_CALL(*mock_mojo_helper, CreatePendingRemote(_))
      .Times(1)
      .WillOnce([](mojo::ScopedMessagePipeHandle pipe_handle) {
        return mojo::PendingRemote<network::mojom::URLLoaderFactory>(
            std::move(pipe_handle), 0);
      });

  EXPECT_CALL(*mock_mojo_helper, BindRemote(_, _))
      .Times(1)
      .WillOnce([](mojo::Remote<network::mojom::URLLoaderFactory>&
                       remote_url_loader_factory,
                   mojo::PendingRemote<network::mojom::URLLoaderFactory>
                       pending_remote_url_loader_factory) {
        remote_url_loader_factory.Bind(
            std::move(pending_remote_url_loader_factory));
      });

  EXPECT_CALL(*mock_mojo_helper, CheckRemoteConnection(_))
      .Times(1)
      .WillOnce([](mojo::Remote<network::mojom::URLLoaderFactory>&
                       remote_url_loader_factory) { return false; });

  return ChromeManagementServiceTest::TestRunProcess(
      "DisconnectedRemoteUrlLoaderFactory", true, 1, std::move(mojo_helper),
      ManagementServiceError::kDisconnectedUrlLoaderFactory);
}

TEST_F(ChromeManagementServiceTest,
       MojoFailure_DisconnectedRemoteLoaderFactory) {
  auto child_process = LaunchProcessAndSendInvitation(
      "DisconnectedRemoteUrlLoaderFactory", GetTestCommandLine());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(kFailure, exit_code);
}

// Tests when the chrome management service failed due to a failed
// key rotation.
MULTIPROCESS_TEST_MAIN(RotateDTKeyFailure) {
  return ChromeManagementServiceTest::TestRunProcess(
      "RotateDTKeyFailure", true, kFailure, MojoHelper::Create(), std::nullopt);
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
