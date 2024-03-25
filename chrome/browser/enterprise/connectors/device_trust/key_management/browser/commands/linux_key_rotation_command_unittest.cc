// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/linux_key_rotation_command.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;

namespace enterprise_connectors {

namespace {

constexpr char kExitCodeHistogram[] =
    "Enterprise.DeviceTrust.KeyRotationCommand.ExitCode";

constexpr char kNonce[] = "nonce";

constexpr char kFakeDMToken[] = "fake-browser-dm-token";

constexpr char kFakeDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

static constexpr const char* kSwitches[] = {
    switches::kRotateDTKey, switches::kDmServerUrl, switches::kPipeName,
    switches::kNonce, mojo::PlatformChannel::kHandleSwitch};

base::FilePath GetBinaryFilePath() {
  base::FilePath exe_path;
  if (base::PathService::Get(base::DIR_EXE, &exe_path)) {
    return exe_path.Append(constants::kBinaryFileName);
  }
  return exe_path;
}

}  // namespace

class LinuxKeyRotationCommandTest : public testing::Test {
 protected:
  LinuxKeyRotationCommandTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        scoped_path_override_(base::DIR_EXE),
        rotation_command_(
            LinuxKeyRotationCommand(mock_launch_callback_.Get(),
                                    test_shared_loader_factory_)) {}

  static base::CommandLine GetMojoCommandLine(base::CommandLine command_line) {
    auto test_command_line = base::GetMultiProcessTestChildBaseCommandLine();
    test_command_line.CopySwitchesFrom(command_line, kSwitches);
    return test_command_line;
  }

  static base::Process LaunchTestProcess(std::string process_name,
                                         base::CommandLine command_line,
                                         base::LaunchOptions options) {
    if (process_name == "InvalidProcess")
      return base::Process();

    return base::SpawnMultiProcessTestChild(
        process_name,
        process_name == "MojoInvitation"
            ? GetMojoCommandLine(command_line)
            : base::GetMultiProcessTestChildBaseCommandLine(),
        options);
  }

  void StartTestRotation(
      std::string process_name,
      enterprise_connectors::KeyRotationCommand::Status expected_status) {
    KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl,
                                         kNonce};
    CreateManagementServiceBinary();

    EXPECT_CALL(mock_launch_callback_, Run(_, _))
        .WillOnce([&process_name](const base::CommandLine& command_line,
                                  const base::LaunchOptions& options) {
          EXPECT_TRUE(options.allow_new_privs);
          return LaunchTestProcess(process_name, command_line, options);
        });

    base::test::TestFuture<KeyRotationCommand::Status> future_status;
    rotation_command_.Trigger(params, future_status.GetCallback());
    EXPECT_EQ(future_status.Get(), expected_status);
  }

  void CreateManagementServiceBinary() {
    ASSERT_TRUE(
        base::WriteFile(GetBinaryFilePath(), std::string_view("test_content")));
  }

  void ExpectCommandErrorHistogram(KeyRotationCommandError error) {
    static constexpr char kErrorHistogram[] =
        "Enterprise.DeviceTrust.KeyRotationCommand.Error";
    histogram_tester_.ExpectUniqueSample(kErrorHistogram, error, 1);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::ScopedPathOverride scoped_path_override_;
  base::MockCallback<LinuxKeyRotationCommand::LaunchCallback>
      mock_launch_callback_;

  LinuxKeyRotationCommand rotation_command_;
};

// Tests for the key mojo invitation where the chrome management service
// process successfully accepted the mojo invitation and connected to the
// url_loader_factory.
MULTIPROCESS_TEST_MAIN(MojoInvitation) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Validate url_loader_factory.
  auto command_line = *base::CommandLine::ForCurrentProcess();
  auto channel_endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
  auto incoming_invitation =
      mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
  auto pipe = incoming_invitation.ExtractMessagePipe(
      command_line.GetSwitchValueNative(switches::kPipeName));
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>(std::move(pipe),
                                                            0));
  if (!url_loader_factory.is_connected())
    return 5;
  if (!url_loader_factory.is_bound())
    return 6;

  // Validate command line arguments.
  std::string token_base64 = base::Base64Encode(kFakeDMToken);
  std::string nonce_base64 = base::Base64Encode(kNonce);

  EXPECT_EQ(token_base64,
            command_line.GetSwitchValueNative(switches::kRotateDTKey));
  EXPECT_EQ(kFakeDmServerUrl,
            command_line.GetSwitchValueNative(switches::kDmServerUrl));
  EXPECT_EQ(nonce_base64, command_line.GetSwitchValueNative(switches::kNonce));

  return testing::Test::HasFailure();
}

TEST_F(LinuxKeyRotationCommandTest, MojoAcceptInvitation) {
  StartTestRotation("MojoInvitation", KeyRotationCommand::Status::SUCCEEDED);
  histogram_tester_.ExpectUniqueSample(kExitCodeHistogram, Status::kSuccess, 1);
}

// Tests for a key rotation when the chrome management service succeeded.
MULTIPROCESS_TEST_MAIN(Success) {
  return Status::kSuccess;
}

TEST_F(LinuxKeyRotationCommandTest, RotateSuccess) {
  StartTestRotation("Success", KeyRotationCommand::Status::SUCCEEDED);
  histogram_tester_.ExpectUniqueSample(kExitCodeHistogram, Status::kSuccess, 1);
}

// Tests for a key rotation failure when the chrome management service failed.
MULTIPROCESS_TEST_MAIN(Failure) {
  return Status::kFailure;
}

TEST_F(LinuxKeyRotationCommandTest, RotateFailure) {
  StartTestRotation("Failure", KeyRotationCommand::Status::FAILED);
  histogram_tester_.ExpectUniqueSample(kExitCodeHistogram, Status::kFailure, 1);
}

// Tests for a key rotation failure when the chrome management service failed
// with an unknown error.
MULTIPROCESS_TEST_MAIN(UnknownFailure) {
  return Status::kUnknownFailure;
}

TEST_F(LinuxKeyRotationCommandTest, RotateFailure_UnknownError) {
  StartTestRotation("UnknownFailure", KeyRotationCommand::Status::FAILED);
  histogram_tester_.ExpectUniqueSample(kExitCodeHistogram,
                                       Status::kUnknownFailure, 1);
}

// Tests for a key rotation failure when an invalid process was launched.
TEST_F(LinuxKeyRotationCommandTest, RotateFailureInvalidProcess) {
  StartTestRotation("InvalidProcess", KeyRotationCommand::Status::FAILED);
  histogram_tester_.ExpectTotalCount(kExitCodeHistogram, 0);
}

// Tests that the correct histogram is populated when the LogExitCode method
// receives a negative exit code.
TEST_F(LinuxKeyRotationCommandTest, NegativeExitCode) {
  LogKeyRotationExitCode(-1);
  histogram_tester_.ExpectUniqueSample(kExitCodeHistogram, -1, 1);
}

// Tests that the command will fail with the expected message when the
// management service binary is not found.
TEST_F(LinuxKeyRotationCommandTest, MissingServiceBinary) {
  KeyRotationCommand::Params params = {kFakeDMToken, kFakeDmServerUrl, kNonce};
  base::test::TestFuture<KeyRotationCommand::Status> future_status;
  rotation_command_.Trigger(params, future_status.GetCallback());
  EXPECT_EQ(future_status.Get(),
            KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION);
  ExpectCommandErrorHistogram(
      KeyRotationCommandError::kMissingManagementService);
}

}  // namespace enterprise_connectors
