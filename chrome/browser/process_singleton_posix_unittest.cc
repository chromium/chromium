// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProcessSingletonPosixTest : public testing::Test {
 public:
  // A ProcessSingleton exposing some protected methods for testing.
  class TestableProcessSingleton : public ProcessSingleton {
   public:
    explicit TestableProcessSingleton(const base::FilePath& user_data_dir)
        : ProcessSingleton(user_data_dir,
                           base::BindRepeating(
                               &TestableProcessSingleton::NotificationCallback,
                               base::Unretained(this))) {}

    std::vector<base::CommandLine> callback_command_lines_;

    using ProcessSingleton::NotifyOtherProcessWithTimeout;
    using ProcessSingleton::NotifyOtherProcessWithTimeoutOrCreate;
    using ProcessSingleton::OverrideCurrentPidForTesting;
    using ProcessSingleton::OverrideKillCallbackForTesting;
    using ProcessSingleton::StartWatching;

   private:
    bool NotificationCallback(base::CommandLine command_line,
                              const base::FilePath& current_directory) {
      callback_command_lines_.push_back(std::move(command_line));
      return true;
    }
  };

  ProcessSingletonPosixTest()
      : kill_callbacks_(0),
        task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        wait_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
        signal_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        process_singleton_on_thread_(nullptr) {}

  void SetUp() override {
    testing::Test::SetUp();

    ProcessSingleton::DisablePromptForTesting();
    ProcessSingleton::SkipIsChromeProcessCheckForTesting(false);
    ProcessSingleton::SetUserOptedUnlockInUseProfileForTesting(false);
    // Put the lock in a temporary directory.  Doesn't need to be a
    // full profile to test this code.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Use a long directory name to ensure that the socket isn't opened through
    // the symlink.
    user_data_path_ = temp_dir_.GetPath().Append(
        std::string(sizeof(sockaddr_un::sun_path), 'a'));
    ASSERT_TRUE(CreateDirectory(user_data_path_));

    lock_path_ = user_data_path_.Append(chrome::kSingletonLockFilename);
    socket_path_ = user_data_path_.Append(chrome::kSingletonSocketFilename);
    cookie_path_ = user_data_path_.Append(chrome::kSingletonCookieFilename);
  }

  void TearDown() override {
    scoped_refptr<base::ThreadTestHelper> io_helper(
        new base::ThreadTestHelper(content::GetIOThreadTaskRunner({}).get()));
    ASSERT_TRUE(io_helper->Run());

    // Destruct the ProcessSingleton object before the IO thread so that its
    // internals are destructed properly.
    if (process_singleton_on_thread_) {
      worker_thread_->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ProcessSingletonPosixTest::DestructProcessSingleton,
                         base::Unretained(this)));

      scoped_refptr<base::ThreadTestHelper> helper(
          new base::ThreadTestHelper(worker_thread_->task_runner().get()));
      ASSERT_TRUE(helper->Run());
    }

    testing::Test::TearDown();
  }

  void CreateProcessSingletonOnThread() {
    ASSERT_FALSE(worker_thread_.get());
    worker_thread_ = std::make_unique<base::Thread>("BlockingThread");
    worker_thread_->Start();

    worker_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ProcessSingletonPosixTest::CreateProcessSingletonInternal,
            base::Unretained(this)));

    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(worker_thread_->task_runner().get()));
    ASSERT_TRUE(helper->Run());
  }

  TestableProcessSingleton* CreateProcessSingleton() {
    return new TestableProcessSingleton(user_data_path_);
  }

  void VerifyFiles() {
    struct stat statbuf;
    ASSERT_EQ(0, lstat(lock_path_.value().c_str(), &statbuf));
    ASSERT_TRUE(S_ISLNK(statbuf.st_mode));
    char buf[PATH_MAX];
    ssize_t len = readlink(lock_path_.value().c_str(), buf, PATH_MAX);
    ASSERT_GT(len, 0);

    ASSERT_EQ(0, lstat(socket_path_.value().c_str(), &statbuf));
    ASSERT_TRUE(S_ISLNK(statbuf.st_mode));

    len = readlink(socket_path_.value().c_str(), buf, PATH_MAX);
    ASSERT_GT(len, 0);
    base::FilePath socket_target_path = base::FilePath(std::string(buf, len));

    ASSERT_EQ(0, lstat(socket_target_path.value().c_str(), &statbuf));
    ASSERT_TRUE(S_ISSOCK(statbuf.st_mode));

    len = readlink(cookie_path_.value().c_str(), buf, PATH_MAX);
    ASSERT_GT(len, 0);
    std::string cookie(buf, len);

    base::FilePath remote_cookie_path = socket_target_path.DirName().
        Append(chrome::kSingletonCookieFilename);
    len = readlink(remote_cookie_path.value().c_str(), buf, PATH_MAX);
    ASSERT_GT(len, 0);
    EXPECT_EQ(cookie, std::string(buf, len));
  }

  ProcessSingleton::NotifyResult NotifyOtherProcess(bool override_kill) {
    std::unique_ptr<TestableProcessSingleton> process_singleton(
        CreateProcessSingleton());
    base::CommandLine command_line(
        base::CommandLine::ForCurrentProcess()->GetProgram());
    command_line.AppendArg("about:blank");
    if (override_kill) {
      process_singleton->OverrideCurrentPidForTesting(
          base::GetCurrentProcId() + 1);
      process_singleton->OverrideKillCallbackForTesting(base::BindRepeating(
          &ProcessSingletonPosixTest::KillCallback, base::Unretained(this)));
    }

    return process_singleton->NotifyOtherProcessWithTimeout(
        command_line, kRetryAttempts, timeout(), true);
  }

  // A helper method to call ProcessSingleton::NotifyOtherProcessOrCreate().
  ProcessSingleton::NotifyResult NotifyOtherProcessOrCreate(
      const std::string& url) {
    std::unique_ptr<TestableProcessSingleton> process_singleton(
        CreateProcessSingleton());
    base::CommandLine command_line(
        base::CommandLine::ForCurrentProcess()->GetProgram());
    command_line.AppendArg(url);
    return process_singleton->NotifyOtherProcessWithTimeoutOrCreate(
        command_line, kRetryAttempts, timeout());
  }

  void CheckNotified() {
    ASSERT_TRUE(process_singleton_on_thread_);
    ASSERT_EQ(1u, process_singleton_on_thread_->callback_command_lines_.size());
    ASSERT_TRUE(base::Contains(
        process_singleton_on_thread_->callback_command_lines_[0].argv(),
        "about:blank"));
    ASSERT_EQ(0, kill_callbacks_);
  }

  void BlockWorkerThread() {
    worker_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ProcessSingletonPosixTest::BlockThread,
                                  base::Unretained(this)));
  }

  void UnblockWorkerThread() {
    wait_event_.Signal();  // Unblock the worker thread for shutdown.
    signal_event_.Wait();  // Ensure thread unblocks before continuing.
  }

  void BlockThread() {
    wait_event_.Wait();
    signal_event_.Signal();
  }

  base::FilePath user_data_path_;
  base::FilePath lock_path_;
  base::FilePath socket_path_;
  base::FilePath cookie_path_;
  int kill_callbacks_;

 private:
  static const int kRetryAttempts = 2;

  base::TimeDelta timeout() const {
    return TestTimeouts::tiny_timeout() * kRetryAttempts;
  }

  void CreateProcessSingletonInternal() {
    ASSERT_TRUE(!process_singleton_on_thread_);
    process_singleton_on_thread_ = CreateProcessSingleton();
    ASSERT_EQ(ProcessSingleton::PROCESS_NONE,
              process_singleton_on_thread_->NotifyOtherProcessOrCreate());
    process_singleton_on_thread_->StartWatching();
  }

  void DestructProcessSingleton() {
    ASSERT_TRUE(process_singleton_on_thread_);
    delete process_singleton_on_thread_;
  }

  void KillCallback(int pid) {
    kill_callbacks_++;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::WaitableEvent wait_event_;
  base::WaitableEvent signal_event_;

  std::unique_ptr<base::Thread> worker_thread_;
  raw_ptr<TestableProcessSingleton, DanglingUntriaged>
      process_singleton_on_thread_;
};

}  // namespace

// Test if the socket file and symbol link created by ProcessSingletonPosix
// are valid.
// If this test flakes, use http://crbug.com/74554.
TEST_F(ProcessSingletonPosixTest, CheckSocketFile) {
  CreateProcessSingletonOnThread();
  VerifyFiles();
}

// TODO(james.su@gmail.com): port following tests to Windows.
// Test success case of NotifyOtherProcess().
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessSuccess) {
  CreateProcessSingletonOnThread();
  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED, NotifyOtherProcess(true));
  CheckNotified();
}

// Test failure case of NotifyOtherProcess().
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessFailure) {
  base::HistogramTester histogram_tester;
  CreateProcessSingletonOnThread();

  BlockWorkerThread();
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcess(true));
  ASSERT_EQ(1, kill_callbacks_);
  UnblockWorkerThread();
  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteHungProcessTerminateReason",
      ProcessSingleton::SOCKET_READ_FAILED, 1u);
}

// Test that we don't kill ourselves by accident if a lockfile with the same pid
// happens to exist.
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessNoSuicide) {
  base::HistogramTester histogram_tester;
  CreateProcessSingletonOnThread();
  // Replace lockfile with one containing our own pid.
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  std::string symlink_content = base::StringPrintf(
      "%s%c%u",
      net::GetHostName().c_str(),
      '-',
      base::GetCurrentProcId());
  EXPECT_EQ(0, symlink(symlink_content.c_str(), lock_path_.value().c_str()));

  // Remove socket so that we will not be able to notify the existing browser.
  EXPECT_EQ(0, unlink(socket_path_.value().c_str()));

  // Pretend we are browser process.
  ProcessSingleton::SkipIsChromeProcessCheckForTesting(true);

  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcess(false));
  // If we've gotten to this point without killing ourself, the test succeeded.
  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteProcessInteractionResult",
      ProcessSingleton::SAME_BROWSER_INSTANCE, 1u);
}

// Test that we can still notify a process on the same host even after the
// hostname changed.
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessHostChanged) {
  CreateProcessSingletonOnThread();
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED, NotifyOtherProcess(false));
  CheckNotified();
}

// Test that we kill hung browser when lock says process is on another host and
// we can't notify it over the socket.
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessDifferingHost) {
  base::HistogramTester histogram_tester;
  CreateProcessSingletonOnThread();

  BlockWorkerThread();

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcess(true));
  ASSERT_EQ(1, kill_callbacks_);

  // lock_path_ should be unlinked in NotifyOtherProcess().
  base::FilePath target_path;
  EXPECT_FALSE(base::ReadSymbolicLink(lock_path_, &target_path));

  UnblockWorkerThread();

  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteHungProcessTerminateReason",
      ProcessSingleton::SOCKET_READ_FAILED, 1u);
}

// Test that we'll start creating ProcessSingleton when we have old lock file
// that says process is on another host and there is browser with the same pid
// but with another user data dir. Also suppose that user opted to unlock
// profile.
TEST_F(ProcessSingletonPosixTest,
       NotifyOtherProcessDifferingHost_UnlockedProfileBeforeKill) {
  base::HistogramTester histogram_tester;
  CreateProcessSingletonOnThread();

  BlockWorkerThread();

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  // Remove socket so that we will not be able to notify the existing browser.
  EXPECT_EQ(0, unlink(socket_path_.value().c_str()));

  // Unlock profile that was locked by process on another host.
  ProcessSingleton::SetUserOptedUnlockInUseProfileForTesting(true);
  // Treat process with pid 1234 as browser with different user data dir.
  ProcessSingleton::SkipIsChromeProcessCheckForTesting(true);

  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcess(false));

  // lock_path_ should be unlinked in NotifyOtherProcess().
  base::FilePath target_path;
  EXPECT_FALSE(base::ReadSymbolicLink(lock_path_, &target_path));

  UnblockWorkerThread();

  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteHungProcessTerminateReason",
      ProcessSingleton::NOTIFY_ATTEMPTS_EXCEEDED, 1u);
  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteProcessInteractionResult",
      ProcessSingleton::PROFILE_UNLOCKED_BEFORE_KILL, 1u);
}

// Test that we unlock profile when lock says process is on another host and we
// can't notify it over the socket.
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessOrCreate_DifferingHost) {
  base::HistogramTester histogram_tester;
  CreateProcessSingletonOnThread();

  BlockWorkerThread();

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  // Remove socket so that we will not be able to notify the existing browser.
  EXPECT_EQ(0, unlink(socket_path_.value().c_str()));
  // Unlock profile that was locked by process on another host.
  ProcessSingleton::SetUserOptedUnlockInUseProfileForTesting(true);

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcessOrCreate(url));

  ASSERT_EQ(0, unlink(lock_path_.value().c_str()));

  UnblockWorkerThread();

  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteProcessInteractionResult",
      ProcessSingleton::PROFILE_UNLOCKED, 1u);
}

// Test that Create fails when another browser is using the profile directory.
TEST_F(ProcessSingletonPosixTest, CreateFailsWithExistingBrowser) {
  CreateProcessSingletonOnThread();

  std::unique_ptr<TestableProcessSingleton> process_singleton(
      CreateProcessSingleton());
  process_singleton->OverrideCurrentPidForTesting(base::GetCurrentProcId() + 1);
  EXPECT_FALSE(process_singleton->Create());
}

// Test that Create fails when another browser is using the profile directory
// but with the old socket location.
TEST_F(ProcessSingletonPosixTest, CreateChecksCompatibilitySocket) {
  CreateProcessSingletonOnThread();
  std::unique_ptr<TestableProcessSingleton> process_singleton(
      CreateProcessSingleton());
  process_singleton->OverrideCurrentPidForTesting(base::GetCurrentProcId() + 1);

  // Do some surgery so as to look like the old configuration.
  char buf[PATH_MAX];
  ssize_t len = readlink(socket_path_.value().c_str(), buf, sizeof(buf));
  ASSERT_GT(len, 0);
  base::FilePath socket_target_path = base::FilePath(std::string(buf, len));
  ASSERT_EQ(0, unlink(socket_path_.value().c_str()));
  ASSERT_EQ(0, rename(socket_target_path.value().c_str(),
                      socket_path_.value().c_str()));
  ASSERT_EQ(0, unlink(cookie_path_.value().c_str()));

  EXPECT_FALSE(process_singleton->Create());
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket before of a bad cookie.
TEST_F(ProcessSingletonPosixTest, NotifyOtherProcessOrCreate_BadCookie) {
  CreateProcessSingletonOnThread();
  // Change the cookie.
  EXPECT_EQ(0, unlink(cookie_path_.value().c_str()));
  EXPECT_EQ(0, symlink("INCORRECTCOOKIE", cookie_path_.value().c_str()));

  // Also change the hostname, so the remote does not retry.
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE, NotifyOtherProcessOrCreate(url));
}

TEST_F(ProcessSingletonPosixTest, IgnoreSocketSymlinkWithTooLongTarget) {
  base::HistogramTester histogram_tester;
  CreateProcessSingletonOnThread();
  // Change the symlink to one with a too-long target.
  char buf[PATH_MAX];
  ssize_t len = readlink(socket_path_.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  base::FilePath socket_target_path = base::FilePath(std::string(buf, len));
  base::FilePath long_socket_target_path = socket_target_path.DirName().Append(
      std::string(sizeof(sockaddr_un::sun_path), 'b'));
  ASSERT_EQ(0, unlink(socket_path_.value().c_str()));
  ASSERT_EQ(0, symlink(long_socket_target_path.value().c_str(),
                       socket_path_.value().c_str()));

  // A new ProcessSingleton should ignore the invalid socket path target.
  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcessOrCreate(url));

  // Lock file contains PID of unit_tests process. It is non browser process so
  // we treat lock file as orphaned.
  histogram_tester.ExpectUniqueSample(
      "Chrome.ProcessSingleton.RemoteProcessInteractionResult",
      ProcessSingleton::ORPHANED_LOCK_FILE, 1u);
}

#if BUILDFLAG(IS_MAC)
// Test that if there is an existing lock file, and we could not flock()
// it, then exit.
TEST_F(ProcessSingletonPosixTest, CreateRespectsOldMacLock) {
  std::unique_ptr<TestableProcessSingleton> process_singleton(
      CreateProcessSingleton());
  base::ScopedFD lock_fd(HANDLE_EINTR(
      open(lock_path_.value().c_str(), O_RDWR | O_CREAT | O_EXLOCK, 0644)));
  ASSERT_TRUE(lock_fd.is_valid());
  EXPECT_FALSE(process_singleton->Create());
  base::File::Info info;
  EXPECT_TRUE(base::GetFileInfo(lock_path_, &info));
  EXPECT_FALSE(info.is_directory);
  EXPECT_FALSE(info.is_symbolic_link);
}

// Test that if there is an existing lock file, and it's not locked, we replace
// it.
TEST_F(ProcessSingletonPosixTest, CreateReplacesOldMacLock) {
  std::unique_ptr<TestableProcessSingleton> process_singleton(
      CreateProcessSingleton());
  EXPECT_TRUE(base::WriteFile(lock_path_, std::string_view()));
  EXPECT_TRUE(process_singleton->Create());
  VerifyFiles();
}
#endif  // BUILDFLAG(IS_MAC)
