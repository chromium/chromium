// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/code_sign_clone_manager.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/main_function_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kMainExecutable[] = "Contents/MacOS/TestApp";

const std::vector<base::FilePath> test_files({
    base::FilePath(kMainExecutable),
    base::FilePath("Contents/Info.plist"),
    base::FilePath("Contents/Frameworks/TestApp.framework/Versions/1.1.1.1/"
                   "TestApp"),
    base::FilePath("Contents/Frameworks/TestApp.framework/Versions/Current"),
});

base::FilePath TestAppPath() {
  return base::apple::NSStringToFilePath(NSTemporaryDirectory())
      .Append("TestApp.app");
}

base::FilePath CreateTestApp() {
  base::FilePath test_app = TestAppPath();
  EXPECT_TRUE(base::CreateDirectory(test_app));

  for (base::FilePath file : test_files) {
    EXPECT_TRUE(base::CreateDirectory(test_app.Append(file.DirName())));
    EXPECT_TRUE(base::WriteFile(test_app.Append(file), "test"));
  }

  return test_app;
}

}  // namespace

namespace code_sign_clone_manager {

TEST(CodeSignCloneManagerTest, Clone) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      code_sign_clone_manager::kMacAppCodeSignClone);

  // Create the test app,
  base::FilePath test_app = CreateTestApp();

  // Prevent `CodeSignCloneManager` from running the cleanup helper process.
  // Under test `CHILD_PROCESS_EXE` resolves to the test binary which does not
  // handle the cleanup switches.
  base::ScopedPathOverride scoped_path_override(content::CHILD_PROCESS_EXE);

  base::RunLoop run_loop;
  base::FilePath tmp_app_path;
  CodeSignCloneManager clone_manager(
      test_app, base::FilePath("TestApp"),
      base::BindOnce(
          [](base::OnceClosure quit_closure, base::FilePath* out_app_path,
             base::FilePath in_app_path) {
            *out_app_path = in_app_path;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &tmp_app_path));
  run_loop.Run();

  ASSERT_NE(tmp_app_path, base::FilePath());

  // Make sure the tmp app path has the expected files.
  for (base::FilePath file : test_files) {
    EXPECT_TRUE(base::PathExists(tmp_app_path.Append(file)));
  }

  // Make sure the main executable is a hard link.
  struct stat src_stat;
  EXPECT_EQ(stat(test_app.Append(kMainExecutable).value().c_str(), &src_stat),
            0);
  struct stat tmp_stat;
  EXPECT_EQ(
      stat(tmp_app_path.Append(kMainExecutable).value().c_str(), &tmp_stat), 0);
  EXPECT_EQ(src_stat.st_dev, tmp_stat.st_dev);
  EXPECT_EQ(src_stat.st_ino, tmp_stat.st_ino);
  EXPECT_GT(src_stat.st_nlink, 1);

  EXPECT_TRUE(clone_manager.get_needs_cleanup_for_testing());

  // The clone is typically cleaned up by a helper child process when
  // `clone_manager` goes out of scope. In the test environment that does not
  // happen so manually clean up.
  base::DeletePathRecursively(tmp_app_path.DirName());
}

TEST(CodeSignCloneManagerTest, InvalidDirhelperPath) {
  // Remove the @available check once `MAC_OS_X_VERSION_MIN_REQUIRED` is macOS
  // 11+.
  if (@available(macOS 11.0, *)) {
    base::test::TaskEnvironment task_environment;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        code_sign_clone_manager::kMacAppCodeSignClone);

    base::FilePath test_app = CreateTestApp();
    base::ScopedPathOverride scoped_path_override(content::CHILD_PROCESS_EXE);

    // Set an unsupported `_dirhelper` path.
    CodeSignCloneManager::SetDirhelperPathForTesting(base::FilePath("/tmp"));

    base::RunLoop run_loop;
    base::FilePath tmp_app_path;
    CodeSignCloneManager clone_manager(
        test_app, base::FilePath("TestApp"),
        base::BindOnce(
            [](base::OnceClosure quit_closure, base::FilePath* out_app_path,
               base::FilePath in_app_path) {
              *out_app_path = in_app_path;
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), &tmp_app_path));
    run_loop.Run();

    // Ensure there was a failure.
    EXPECT_TRUE(tmp_app_path.empty());
    EXPECT_FALSE(clone_manager.get_needs_cleanup_for_testing());

    CodeSignCloneManager::ClearDirhelperPathForTesting();
  }
}

TEST(CodeSignCloneManagerTest, FailedHardLink) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      code_sign_clone_manager::kMacAppCodeSignClone);

  base::FilePath test_app = CreateTestApp();
  base::ScopedPathOverride scoped_path_override(content::CHILD_PROCESS_EXE);

  base::RunLoop run_loop;
  base::FilePath tmp_app_path;
  CodeSignCloneManager clone_manager(
      test_app, base::FilePath("DoesNotExist"),
      base::BindOnce(
          [](base::OnceClosure quit_closure, base::FilePath* out_app_path,
             base::FilePath in_app_path) {
            *out_app_path = in_app_path;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &tmp_app_path));
  run_loop.Run();

  // Ensure there was a failure.
  EXPECT_TRUE(tmp_app_path.empty());
  EXPECT_FALSE(clone_manager.get_needs_cleanup_for_testing());
}

TEST(CodeSignCloneManagerTest, FailedClone) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      code_sign_clone_manager::kMacAppCodeSignClone);

  // Starting in macOS 10.15 the system volume is a separate read-only volume.
  // Cloning files from the system volume to a non-system volume will fail with
  // EXDEV (Cross-device link). Perfect for this test.
  // https://support.apple.com/en-us/101400
  base::FilePath test_app("/System/Applications/Calculator.app");
  base::ScopedPathOverride scoped_path_override(content::CHILD_PROCESS_EXE);
  base::RunLoop run_loop;
  base::FilePath tmp_app_path;
  CodeSignCloneManager clone_manager(
      test_app, base::FilePath("Calculator"),
      base::BindOnce(
          [](base::OnceClosure quit_closure, base::FilePath* out_app_path,
             base::FilePath in_app_path) {
            *out_app_path = in_app_path;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &tmp_app_path));
  run_loop.Run();

  // Ensure there was a failure.
  EXPECT_TRUE(tmp_app_path.empty());
  EXPECT_FALSE(clone_manager.get_needs_cleanup_for_testing());
}

TEST(CodeSignCloneManagerTest, ChromeCodeSignCloneCleanupMain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      code_sign_clone_manager::kMacAppCodeSignClone);

  struct TestCase {
    std::string name;
    bool wants_delete;
  };
  std::vector<TestCase> test_cases{
      {std::string("abcdef"), true},  {std::string("ABCDEF"), true},
      {std::string("012345"), true},  {std::string("tKdILk"), true},
      {std::string("-KdILk"), true},  {std::string("../tKk"), false},
      {std::string("tKd../"), false}, {std::string("tKdP.."), true},
      {std::string("."), false},      {std::string(".."), false},
      {std::string("../"), false},    {std::string("./"), false},
      {std::string("../../"), false}, {std::string("AAAAAAAAAA"), false},
  };

  // Create a unique per test invocation temp dir to avoid collisions when
  // this test is run multiple times in parallel.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  CodeSignCloneManager::SetTemporaryDirectoryPathForTesting(
      scoped_temp_dir.GetPath());

  base::FilePath temp_dir =
      CodeSignCloneManager::GetCloneTemporaryDirectoryForTesting();

  for (TestCase& test : test_cases) {
    base::FilePath test_dir = temp_dir.Append("code_sign_clone." + test.name);
    EXPECT_TRUE(base::CreateDirectory(test_dir));

    // Test that ChromeCodeSignCloneCleanupMain only deletes valid paths.
    base::CommandLine cli(
        {"/does/not/exist",
         base::StringPrintf("--%s=%s", switches::kUniqueTempDirSuffix,
                            test.name.c_str()),
         "--no-wait-for-parent-exit-for-testing"});
    internal::ChromeCodeSignCloneCleanupMain(content::MainFunctionParams(&cli));
    EXPECT_EQ(base::PathExists(test_dir), !test.wants_delete) << test.name;
  }

  CodeSignCloneManager::ClearTemporaryDirectoryPathForTesting();
}

}  // namespace code_sign_clone_manager
