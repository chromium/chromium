// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#include "components/crx_file/crx_verifier.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

namespace {

// The public key hash for the test CRX.
constexpr uint8_t kKeyHashTest[] = {
    0x69, 0xfc, 0x41, 0xf6, 0x17, 0x20, 0xc6, 0x36, 0x92, 0xcd, 0x95,
    0x76, 0x69, 0xf6, 0x28, 0xcc, 0xbe, 0x98, 0x4b, 0x93, 0x17, 0xd6,
    0x9c, 0xb3, 0x64, 0x0c, 0x0d, 0x25, 0x61, 0xc5, 0x80, 0x1d};

class RecoveryImprovedActionHandlerTest : public PlatformTest {
 public:
  RecoveryImprovedActionHandlerTest() = default;

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  base::test::TaskEnvironment task_environment_;
};

class TestActionHandler : public RecoveryComponentActionHandler {
 public:
  // Accepts a test CRX without a production publisher proof.
  TestActionHandler()
      : RecoveryComponentActionHandler(
            {std::begin(kKeyHashTest), std::end(kKeyHashTest)},
            crx_file::VerifierFormat::CRX3) {}
  TestActionHandler(const TestActionHandler&) = delete;
  TestActionHandler& operator=(const TestActionHandler&) = delete;

 protected:
  ~TestActionHandler() override = default;

 private:
  // Overrides for RecoveryComponentActionHandler.
  base::CommandLine MakeCommandLine(
      const base::FilePath& unpack_path) const override;
  void PrepareFiles(const base::FilePath& unpack_path) const override;
  void Elevate(Callback callback) override;
};

base::CommandLine TestActionHandler::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  base::CommandLine command_line(
      unpack_path.Append(FILE_PATH_LITERAL("ChromeRecovery.exe")));
  return command_line;
}

void TestActionHandler::PrepareFiles(const base::FilePath& unpack_path) const {}

// This test fixture only tests the per-user execution flow.
void TestActionHandler::Elevate(Callback callback) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
TEST_F(RecoveryImprovedActionHandlerTest, HandleError) {
  unzip::SetUnzipperLaunchOverrideForTesting(
      base::BindRepeating(&unzip::LaunchInProcessUnzipper));

  // Tests the error is propagated through the callback in the error case.
  base::RunLoop runloop;
  base::MakeRefCounted<TestActionHandler>()->Handle(
      base::FilePath{FILE_PATH_LITERAL("not-found")}, "some-session-id",
      base::BindOnce(
          [](base::OnceClosure quit_closure, bool succeeded, int error_code,
             int extra_code1) {
            EXPECT_FALSE(succeeded);
            EXPECT_EQ(update_client::UnpackerError::kInvalidFile,
                      static_cast<update_client::UnpackerError>(error_code));
            EXPECT_EQ(2, extra_code1);
            std::move(quit_closure).Run();
          },
          runloop.QuitClosure()));
  runloop.Run();
}
#endif  //  BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
TEST_F(RecoveryImprovedActionHandlerTest, HandleSuccess) {
  unzip::SetUnzipperLaunchOverrideForTesting(
      base::BindRepeating(&unzip::LaunchInProcessUnzipper));

  // Tests that the recovery program runs and it returns an expected value.
  constexpr char kActionRunFileName[] = "ChromeRecovery.crx3";
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath from_path =
      update_client::GetTestFilePath(kActionRunFileName);
  const base::FilePath to_path =
      temp_dir_.GetPath().AppendASCII(kActionRunFileName);
  ASSERT_TRUE(base::CopyFile(from_path, to_path));

  base::RunLoop runloop;
  base::MakeRefCounted<TestActionHandler>()->Handle(
      to_path, "some-session-id",
      base::BindOnce(
          [](base::OnceClosure quit_closure, bool succeeded, int error_code,
             int extra_code1) {
            EXPECT_TRUE(succeeded);
            EXPECT_EQ(1877345072, error_code);
            EXPECT_EQ(0, extra_code1);
            std::move(quit_closure).Run();
          },
          runloop.QuitClosure()));
  {
    // For some reason, the task which runs the wait for the recovery EXE
    // execution is handled with some delay. This causes the run loop to
    // fail with a timeout.
    const base::test::ScopedRunLoopTimeout specific_timeout(FROM_HERE,
                                                            base::Seconds(60));
    runloop.Run();
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace component_updater
