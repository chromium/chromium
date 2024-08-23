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
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/version.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/main_function_params.h"
#include "content/public/test/browser_task_environment.h"
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
  content::BrowserTaskEnvironment task_environment;
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
  content::BrowserTaskEnvironment task_environment;
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

TEST(CodeSignCloneManagerTest, FailedHardLink) {
  content::BrowserTaskEnvironment task_environment;
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

TEST(CodeSignCloneManagerTest, IsFileOpenMoreThanOnceSameProcess) {
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());

  // Open the file. We are assuming this is the first time the file has been
  // opened, by any process on the host. This is a reasonable assumption given
  // the temp file is relatively out of the way. However, it is not guaranteed.
  // Another process could open the temp file before the call to
  // `IsFileOpenMoreThanOnce`, throwing off the results. If this test is flaky,
  // we should find another approach.
  base::ScopedFD fd_first(
      HANDLE_EINTR(open(temp_file.path().value().c_str(), O_RDONLY)));
  ASSERT_NE(fd_first, -1) << strerror(errno);
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd_first.get()),
            internal::FileOpenMoreThanOnce::kNo);

  // Open it again.
  base::ScopedFD fd_second(
      HANDLE_EINTR(open(temp_file.path().value().c_str(), O_RDONLY)));
  ASSERT_NE(fd_second, -1) << strerror(errno);
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd_first.get()),
            internal::FileOpenMoreThanOnce::kYes);
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd_second.get()),
            internal::FileOpenMoreThanOnce::kYes);

  // Close one of the file descriptors. Ensure `IsFileOpenMoreThanOnce` reflects
  // the change.
  fd_first.reset();
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd_second.get()),
            internal::FileOpenMoreThanOnce::kNo);

  // With one file descriptor still open, check for exclusivity using the path.
  // In this case, the file will be opened by `IsFileOpenMoreThanOnce`, `kYes`
  // should be returned.
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(temp_file.path()),
            internal::FileOpenMoreThanOnce::kYes);

  // Close the last file descriptor, checking by path should now return `kNo`.
  fd_second.reset();
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(temp_file.path()),
            internal::FileOpenMoreThanOnce::kNo);
}

TEST(CodeSignCloneManagerTest, IsFileOpenMoreThanOnceSeparateProcess) {
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());

  // The same comment about this test potentially being flaky also applies here.
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(temp_file.path()),
            internal::FileOpenMoreThanOnce::kNo);

  // Open the temp file. The temp file will be opened again by the child
  // process.
  base::ScopedFD fd(
      HANDLE_EINTR(open(temp_file.path().value().c_str(), O_RDONLY)));
  ASSERT_NE(fd, -1) << strerror(errno);

  // Ensure the file is still only open once. Again, the same comment about this
  // test potentially being flaky also applies here.
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd.get()),
            internal::FileOpenMoreThanOnce::kNo);

  {
    // Use TEE(1) to open `temp_file` a second time, as well as synchronizing
    // when `IsFileOpenMoreThanOnce` is safe to be called. The test will write a
    // sentinel byte to `tee`'s stdin and wait for the byte to arrive on `tee`'s
    // stdout'. This indicates that `tee` is up and running and has opened the
    // `temp_file`. The sentinel byte will also be written to `temp_file`, but
    // that is immaterial. It is simply a side effect of using `tee` as the
    // "opener" of `temp_file`.

    // `TeeChildProcess` is a small, special purpose class. On construction, a
    // `tee` child process is started. A sentinel byte is written to
    // `stdin_writer`, then read back from `stdout_reader`. On destruction,
    // `stdin_writer` is closed, causing `tee` to exit.
    class TeeChildProcess {
     public:
      TeeChildProcess(const base::FilePath& temp_file_path) {
        Init(temp_file_path);
        if (testing::Test::HasFatalFailure()) {
          return;
        }
        RoundTripSentinelByte();
      }
      TeeChildProcess(const TeeChildProcess&) = delete;
      TeeChildProcess& operator=(const TeeChildProcess&) = delete;

      ~TeeChildProcess() {
        if (!process_.IsValid()) {
          return;
        }
        WaitForExit();
      }

     private:
      void Init(const base::FilePath& temp_file_path) {
        int tee_input_pipe[2];
        ASSERT_EQ(pipe(tee_input_pipe), 0) << strerror(errno);
        base::ScopedFD stdin_reader(tee_input_pipe[0]);
        stdin_writer_.reset(tee_input_pipe[1]);

        int tee_output_pipe[2];
        ASSERT_EQ(pipe(tee_output_pipe), 0) << strerror(errno);
        stdout_reader_.reset(tee_output_pipe[0]);
        base::ScopedFD stdout_writer(tee_output_pipe[1]);

        base::LaunchOptions options;
        options.fds_to_remap.emplace_back(stdin_reader.get(), STDIN_FILENO);
        options.fds_to_remap.emplace_back(stdout_writer.get(), STDOUT_FILENO);
        process_ = base::LaunchProcess(
            {
                "/usr/bin/tee",
                temp_file_path.value().c_str(),
            },
            options);
        ASSERT_TRUE(process_.IsValid());
      }

      void RoundTripSentinelByte() {
        // Write the sentinel byte.
        char buff = '\0';
        ASSERT_EQ(HANDLE_EINTR(write(stdin_writer_.get(), &buff, 1)), 1);

        // Wait for `tee` to respond back with the byte.
        buff = 'A';
        ASSERT_EQ(HANDLE_EINTR(read(stdout_reader_.get(), &buff, 1)), 1);
        EXPECT_EQ(buff, '\0');
      }

      void WaitForExit() {
        stdin_writer_.reset();
        int status;
        ASSERT_TRUE(process_.WaitForExit(&status));
        EXPECT_EQ(status, 0);
      }

      base::Process process_;
      base::ScopedFD stdout_reader_;
      base::ScopedFD stdin_writer_;
    };

    TeeChildProcess tee_child_process_scoper(temp_file.path());
    ASSERT_NO_FATAL_FAILURE();

    // The temp file should now be open by both this test and `tee`.
    EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd.get()),
              internal::FileOpenMoreThanOnce::kYes);

    // `tee` will exit when this scope ends.
  }

  // The temp file should now only be open by this test. Yet again, the same
  // comment about this test potentially being flaky also applies here.
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(fd.get()),
            internal::FileOpenMoreThanOnce::kNo);
}

TEST(CodeSignCloneManagerTest, IsFileOpenMoreThanOnceInvalidPath) {
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(base::FilePath()),
            internal::FileOpenMoreThanOnce::kError);
}

TEST(CodeSignCloneManagerTest, IsFileOpenMoreThanOnceBadFileDescriptor) {
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(-1),
            internal::FileOpenMoreThanOnce::kError);
}

TEST(CodeSignCloneManagerTest, IsFileOpenMoreThanOnceHardLink) {
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());

  // A small scoper class that creates a hard link of the provided `path`. The
  // created hard link will reside in the same directory as `path`, and will
  // have the file extension `.link`. For example:
  //
  // ScopedHardLink scoped_hard_link(base::FilePath("/tmp/foo"));
  // scoped_hard_link.get(); // returns "/tmp/foo.link"
  class ScopedHardLink {
   public:
    explicit ScopedHardLink(const base::FilePath& path) { Init(path); }
    ScopedHardLink(const ScopedHardLink&) = delete;
    ScopedHardLink& operator=(const ScopedHardLink&) = delete;
    ~ScopedHardLink() { base::DeleteFile(path_); }
    base::FilePath get() { return path_; }

   private:
    void Init(const base::FilePath& path) {
      path_ = path.AddExtension("link");
      ASSERT_NE(link(path.value().c_str(), path_.value().c_str()), -1)
          << strerror(errno);
    }
    base::FilePath path_;
  };

  ScopedHardLink scoped_hard_link(temp_file.path());
  ASSERT_NO_FATAL_FAILURE();

  // Both `temp_file` and `scoped_link` point to the same inode, expect
  // `IsFileOpenMoreThanOnce` behavior to be the same for both.
  // See the excellent write up by mark@chromium.org on the topic:
  // https://chromium-review.googlesource.com/c/chromium/src/+/5793760/comment/9df14e62_3a24ccc0
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(temp_file.path()),
            internal::FileOpenMoreThanOnce::kNo);
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(scoped_hard_link.get()),
            internal::FileOpenMoreThanOnce::kNo);

  // Open one of the paths.
  base::ScopedFD temp_file_open_fd(
      HANDLE_EINTR(open(temp_file.path().value().c_str(), O_RDONLY)));
  ASSERT_NE(temp_file_open_fd, -1) << strerror(errno);

  // Again, the `IsFileOpenMoreThanOnce` behavior should be the same for both.
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(temp_file.path()),
            internal::FileOpenMoreThanOnce::kYes);
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(scoped_hard_link.get()),
            internal::FileOpenMoreThanOnce::kYes);

  // After closing, the behavior should also be the same.
  temp_file_open_fd.reset();
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(temp_file.path()),
            internal::FileOpenMoreThanOnce::kNo);
  EXPECT_EQ(internal::IsFileOpenMoreThanOnce(scoped_hard_link.get()),
            internal::FileOpenMoreThanOnce::kNo);
}

}  // namespace code_sign_clone_manager
