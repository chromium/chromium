// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/logging.h"

#include <sstream>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <ucontext.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <excpt.h>
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include <lib/zx/event.h>
#include <lib/zx/exception.h>
#include <lib/zx/thread.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <zircon/types.h>
#endif  // BUILDFLAG(IS_FUCHSIA)

#include <optional>

namespace logging {

namespace {

using ::testing::Return;
using ::testing::_;

class LoggingTest : public testing::Test {
 protected:
  const ScopedLoggingSettings& scoped_logging_settings() {
    return scoped_logging_settings_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  ScopedLoggingSettings scoped_logging_settings_;
};

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

class MockLogAssertHandler {
 public:
  MOCK_METHOD4(
      HandleLogAssert,
      void(const char*, int, const std::string_view, const std::string_view));
};

TEST_F(LoggingTest, BasicLogging) {
  MockLogSource mock_log_source;

  // 4 base logs: LOG, LOG_IF, PLOG, and PLOG_IF
  int expected_logs = 4;

  // 4 verbose logs: VLOG, VLOG_IF, VPLOG, VPLOG_IF.
  if (VLOG_IS_ON(0))
    expected_logs += 4;

  // 4 debug logs: DLOG, DLOG_IF, DPLOG, DPLOG_IF.
  if (DCHECK_IS_ON())
    expected_logs += 4;

  // 4 verbose debug logs: DVLOG, DVLOG_IF, DVPLOG, DVPLOG_IF
  if (VLOG_IS_ON(0) && DCHECK_IS_ON())
    expected_logs += 4;

  EXPECT_CALL(mock_log_source, Log())
      .Times(expected_logs)
      .WillRepeatedly(Return("log message"));

  SetMinLogLevel(LOGGING_INFO);

  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_EQ(DCHECK_IS_ON(), DLOG_IS_ON(INFO));

  EXPECT_TRUE(VLOG_IS_ON(0));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, true) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, true) << mock_log_source.Log();
  VLOG(0) << mock_log_source.Log();
  VLOG_IF(0, true) << mock_log_source.Log();
  VPLOG(0) << mock_log_source.Log();
  VPLOG_IF(0, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(0) << mock_log_source.Log();
  DVLOG_IF(0, true) << mock_log_source.Log();
  DVPLOG(0) << mock_log_source.Log();
  DVPLOG_IF(0, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, LogIsOn) {
  SetMinLogLevel(LOGGING_INFO);
  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_TRUE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOGGING_WARNING);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_TRUE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOGGING_ERROR);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOGGING_FATAL + 1);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_FALSE(LOG_IS_ON(ERROR));
  // LOG_IS_ON(FATAL) should always be true.
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  // If DCHECK_IS_ON() then DFATAL is FATAL.
  EXPECT_EQ(DCHECK_IS_ON(), LOG_IS_ON(DFATAL));
}

TEST_F(LoggingTest, LoggingIsLazyBySeverity) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  SetMinLogLevel(LOGGING_WARNING);

  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(DLOG_IS_ON(INFO));
  EXPECT_FALSE(VLOG_IS_ON(1));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, false) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, false) << mock_log_source.Log();
  VLOG(1) << mock_log_source.Log();
  VLOG_IF(1, true) << mock_log_source.Log();
  VPLOG(1) << mock_log_source.Log();
  VPLOG_IF(1, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(1) << mock_log_source.Log();
  DVLOG_IF(1, true) << mock_log_source.Log();
  DVPLOG(1) << mock_log_source.Log();
  DVPLOG_IF(1, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, LoggingIsLazyByDestination) {
  MockLogSource mock_log_source;
  MockLogSource mock_log_source_error;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  // Severity >= ERROR is always printed to stderr.
  EXPECT_CALL(mock_log_source_error, Log()).Times(1).
      WillRepeatedly(Return("log message"));

  LoggingSettings settings;
  settings.logging_dest = LOG_NONE;
  InitLogging(settings);

  LOG(INFO) << mock_log_source.Log();
  LOG(WARNING) << mock_log_source.Log();
  LOG(ERROR) << mock_log_source_error.Log();
}

// Check that logging to stderr is gated on LOG_TO_STDERR.
TEST_F(LoggingTest, LogToStdErrFlag) {
  LoggingSettings settings;
  settings.logging_dest = LOG_NONE;
  InitLogging(settings);
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);
  LOG(INFO) << mock_log_source.Log();

  settings.logging_dest = LOG_TO_STDERR;
  MockLogSource mock_log_source_stderr;
  InitLogging(settings);
  EXPECT_CALL(mock_log_source_stderr, Log()).Times(1).WillOnce(Return("foo"));
  LOG(INFO) << mock_log_source_stderr.Log();
}

// Check that messages with severity ERROR or higher are always logged to
// stderr if no log-destinations are set, other than LOG_TO_FILE.
// This test is currently only POSIX-compatible.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
namespace {
void TestForLogToStderr(int log_destinations,
                        bool* did_log_info,
                        bool* did_log_error) {
  const char kInfoLogMessage[] = "This is an INFO level message";
  const char kErrorLogMessage[] = "Here we have a message of level ERROR";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Set up logging.
  LoggingSettings settings;
  settings.logging_dest = log_destinations;
  base::FilePath file_logs_path;
  if (log_destinations & LOG_TO_FILE) {
    file_logs_path = temp_dir.GetPath().Append("file.log");
    settings.log_file_path = file_logs_path.value().c_str();
  }
  InitLogging(settings);

  // Create a file and change stderr to write to that file, to easily check
  // contents.
  base::FilePath stderr_logs_path = temp_dir.GetPath().Append("stderr.log");
  base::File stderr_logs = base::File(
      stderr_logs_path,
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  base::ScopedFD stderr_backup = base::ScopedFD(dup(STDERR_FILENO));
  int dup_result = dup2(stderr_logs.GetPlatformFile(), STDERR_FILENO);
  ASSERT_EQ(dup_result, STDERR_FILENO);

  LOG(INFO) << kInfoLogMessage;
  LOG(ERROR) << kErrorLogMessage;

  // Restore the original stderr logging destination.
  dup_result = dup2(stderr_backup.get(), STDERR_FILENO);
  ASSERT_EQ(dup_result, STDERR_FILENO);

  // Check which of the messages were written to stderr.
  std::string written_logs;
  ASSERT_TRUE(base::ReadFileToString(stderr_logs_path, &written_logs));
  *did_log_info = written_logs.find(kInfoLogMessage) != std::string::npos;
  *did_log_error = written_logs.find(kErrorLogMessage) != std::string::npos;
}
}  // namespace

TEST_F(LoggingTest, AlwaysLogErrorsToStderr) {
  bool did_log_info = false;
  bool did_log_error = false;

  // Fuchsia only logs to stderr when explicitly specified.
#if !BUILDFLAG(IS_FUCHSIA)
  // When no destinations are specified, ERRORs should still log to stderr.
  TestForLogToStderr(LOG_NONE, &did_log_info, &did_log_error);
  EXPECT_FALSE(did_log_info);
  EXPECT_TRUE(did_log_error);

  // Logging only to a file should also log ERRORs to stderr as well.
  TestForLogToStderr(LOG_TO_FILE, &did_log_info, &did_log_error);
  EXPECT_FALSE(did_log_info);
  EXPECT_TRUE(did_log_error);
#endif

  // ERRORs should not be logged to stderr if any destination besides FILE is
  // set.
  TestForLogToStderr(LOG_TO_SYSTEM_DEBUG_LOG, &did_log_info, &did_log_error);
  EXPECT_FALSE(did_log_info);
  EXPECT_FALSE(did_log_error);

  // Both ERRORs and INFO should be logged if LOG_TO_STDERR is set.
  TestForLogToStderr(LOG_TO_STDERR, &did_log_info, &did_log_error);
  EXPECT_TRUE(did_log_info);
  EXPECT_TRUE(did_log_error);
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(LoggingTest, InitWithFileDescriptor) {
  const char kErrorLogMessage[] = "something bad happened";

  // Open a file to pass to the InitLogging.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_log_path = temp_dir.GetPath().Append("file.log");
  FILE* log_file = fopen(file_log_path.value().c_str(), "w");
  CHECK(log_file);

  // Set up logging.
  LoggingSettings settings;
  settings.logging_dest = LOG_TO_FILE;
  settings.log_file = log_file;
  InitLogging(settings);

  LOG(ERROR) << kErrorLogMessage;

  // Check the message was written to the log file.
  std::string written_logs;
  ASSERT_TRUE(base::ReadFileToString(file_log_path, &written_logs));
  ASSERT_NE(written_logs.find(kErrorLogMessage), std::string::npos);
}

TEST_F(LoggingTest, DuplicateLogFile) {
  const char kErrorLogMessage1[] = "something really bad happened";
  const char kErrorLogMessage2[] = "some other bad thing happened";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_log_path = temp_dir.GetPath().Append("file.log");

  // Set up logging.
  LoggingSettings settings;
  settings.logging_dest = LOG_TO_FILE;
  settings.log_file_path = file_log_path.value().c_str();
  InitLogging(settings);

  LOG(ERROR) << kErrorLogMessage1;

  // Duplicate the log FILE, close the original (to make sure we actually
  // duplicated it), and write to the duplicate.
  FILE* log_file_dup = DuplicateLogFILE();
  CHECK(log_file_dup);
  CloseLogFile();
  fprintf(log_file_dup, "%s\n", kErrorLogMessage2);
  fflush(log_file_dup);

  // Check the messages were written to the log file.
  std::string written_logs;
  ASSERT_TRUE(base::ReadFileToString(file_log_path, &written_logs));
  ASSERT_NE(written_logs.find(kErrorLogMessage1), std::string::npos);
  ASSERT_NE(written_logs.find(kErrorLogMessage2), std::string::npos);
  fclose(log_file_dup);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !CHECK_WILL_STREAM() && BUILDFLAG(IS_WIN)
// Tell clang to not optimize this function or else it will remove the CHECKs.
[[clang::optnone]] NOINLINE void CheckContainingFunc(int death_location) {
  CHECK(death_location != 1);
  CHECK(death_location != 2);
  CHECK(death_location != 3);
}

int GetCheckExceptionData(EXCEPTION_POINTERS* p, DWORD* code, void** addr) {
  *code = p->ExceptionRecord->ExceptionCode;
  *addr = p->ExceptionRecord->ExceptionAddress;
  return EXCEPTION_EXECUTE_HANDLER;
}

TEST_F(LoggingTest, CheckCausesDistinctBreakpoints) {
  DWORD code1 = 0;
  DWORD code2 = 0;
  DWORD code3 = 0;
  void* addr1 = nullptr;
  void* addr2 = nullptr;
  void* addr3 = nullptr;

  // Record the exception code and addresses.
  __try {
    CheckContainingFunc(1);
  } __except (
      GetCheckExceptionData(GetExceptionInformation(), &code1, &addr1)) {
  }

  __try {
    CheckContainingFunc(2);
  } __except (
      GetCheckExceptionData(GetExceptionInformation(), &code2, &addr2)) {
  }

  __try {
    CheckContainingFunc(3);
  } __except (
      GetCheckExceptionData(GetExceptionInformation(), &code3, &addr3)) {
  }

  // Ensure that the exception codes are correct (in particular, breakpoints,
  // not access violations).
  EXPECT_EQ(STATUS_BREAKPOINT, code1);
  EXPECT_EQ(STATUS_BREAKPOINT, code2);
  EXPECT_EQ(STATUS_BREAKPOINT, code3);

  // Ensure that none of the CHECKs are colocated.
  EXPECT_NE(addr1, addr2);
  EXPECT_NE(addr1, addr3);
  EXPECT_NE(addr2, addr3);
}
#elif BUILDFLAG(IS_FUCHSIA)

// CHECK causes a direct crash (without jumping to another function) only in
// official builds. Unfortunately, continuous test coverage on official builds
// is lower. Furthermore, since the Fuchsia implementation uses threads, it is
// not possible to rely on an implementation of CHECK that calls abort(), which
// takes down the whole process, preventing the thread exception handler from
// handling the exception. DO_CHECK here falls back on base::ImmediateCrash() in
// non-official builds, to catch regressions earlier in the CQ.
#if !CHECK_WILL_STREAM()
#define DO_CHECK CHECK
#else
#define DO_CHECK(cond)      \
  if (!(cond)) {            \
    base::ImmediateCrash(); \
  }
#endif

struct thread_data_t {
  // For signaling the thread ended properly.
  zx::event event;
  // For catching thread exceptions. Created by the crashing thread.
  zx::channel channel;
  // Location where the thread is expected to crash.
  int death_location;
};

// Indicates the exception channel has been created successfully.
constexpr zx_signals_t kChannelReadySignal = ZX_USER_SIGNAL_0;

// Indicates an error setting up the crash thread.
constexpr zx_signals_t kCrashThreadErrorSignal = ZX_USER_SIGNAL_1;

void* CrashThread(void* arg) {
  thread_data_t* data = (thread_data_t*)arg;
  int death_location = data->death_location;

  // Register the exception handler.
  zx_status_t status =
      zx::thread::self()->create_exception_channel(0, &data->channel);
  if (status != ZX_OK) {
    data->event.signal(0, kCrashThreadErrorSignal);
    return nullptr;
  }
  data->event.signal(0, kChannelReadySignal);

  DO_CHECK(death_location != 1);
  DO_CHECK(death_location != 2);
  DO_CHECK(death_location != 3);

  // We should never reach this point, signal the thread incorrectly ended
  // properly.
  data->event.signal(0, kCrashThreadErrorSignal);
  return nullptr;
}

// Helper function to call pthread_exit(nullptr).
_Noreturn __NO_SAFESTACK void exception_pthread_exit() {
  pthread_exit(nullptr);
}

// Runs the CrashThread function in a separate thread.
void SpawnCrashThread(int death_location, uintptr_t* child_crash_addr) {
  zx::event event;
  zx_status_t status = zx::event::create(0, &event);
  ASSERT_EQ(status, ZX_OK);

  // Run the thread.
  thread_data_t thread_data = {std::move(event), zx::channel(), death_location};
  pthread_t thread;
  int ret = pthread_create(&thread, nullptr, CrashThread, &thread_data);
  ASSERT_EQ(ret, 0);

  // Wait for the thread to set up its exception channel.
  zx_signals_t signals = 0;
  status =
      thread_data.event.wait_one(kChannelReadySignal | kCrashThreadErrorSignal,
                                 zx::time::infinite(), &signals);
  ASSERT_EQ(status, ZX_OK);
  ASSERT_EQ(signals, kChannelReadySignal);

  // Wait for the exception and read it out of the channel.
  status =
      thread_data.channel.wait_one(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED,
                                   zx::time::infinite(), &signals);
  ASSERT_EQ(status, ZX_OK);
  // Check the thread did crash and not terminate.
  ASSERT_FALSE(signals & ZX_CHANNEL_PEER_CLOSED);

  zx_exception_info_t exception_info;
  zx::exception exception;
  status = thread_data.channel.read(
      0, &exception_info, exception.reset_and_get_address(),
      sizeof(exception_info), 1, nullptr, nullptr);
  ASSERT_EQ(status, ZX_OK);

  // Get the crash address and point the thread towards exiting.
  zx::thread zircon_thread;
  status = exception.get_thread(&zircon_thread);
  ASSERT_EQ(status, ZX_OK);
  zx_thread_state_general_regs_t buffer;
  status = zircon_thread.read_state(ZX_THREAD_STATE_GENERAL_REGS, &buffer,
                                    sizeof(buffer));
  ASSERT_EQ(status, ZX_OK);
#if defined(ARCH_CPU_X86_64)
  *child_crash_addr = static_cast<uintptr_t>(buffer.rip);
  buffer.rip = reinterpret_cast<uintptr_t>(exception_pthread_exit);
#elif defined(ARCH_CPU_ARM64)
  *child_crash_addr = static_cast<uintptr_t>(buffer.pc);
  buffer.pc = reinterpret_cast<uintptr_t>(exception_pthread_exit);
#else
#error Unsupported architecture
#endif
  ASSERT_EQ(zircon_thread.write_state(ZX_THREAD_STATE_GENERAL_REGS, &buffer,
                                      sizeof(buffer)),
            ZX_OK);

  // Clear the exception so the thread continues.
  uint32_t state = ZX_EXCEPTION_STATE_HANDLED;
  ASSERT_EQ(
      exception.set_property(ZX_PROP_EXCEPTION_STATE, &state, sizeof(state)),
      ZX_OK);
  exception.reset();

  // Join the exiting pthread.
  ASSERT_EQ(pthread_join(thread, nullptr), 0);
}

TEST_F(LoggingTest, CheckCausesDistinctBreakpoints) {
  uintptr_t child_crash_addr_1 = 0;
  uintptr_t child_crash_addr_2 = 0;
  uintptr_t child_crash_addr_3 = 0;

  SpawnCrashThread(1, &child_crash_addr_1);
  SpawnCrashThread(2, &child_crash_addr_2);
  SpawnCrashThread(3, &child_crash_addr_3);

  ASSERT_NE(0u, child_crash_addr_1);
  ASSERT_NE(0u, child_crash_addr_2);
  ASSERT_NE(0u, child_crash_addr_3);
#if defined(OFFICIAL_BUILD)
  // In unofficial builds, we'll end up in std::abort
  // for each crash. In official builds, we should get a different
  // crash address for each location.
  ASSERT_NE(child_crash_addr_1, child_crash_addr_2);
  ASSERT_NE(child_crash_addr_1, child_crash_addr_3);
  ASSERT_NE(child_crash_addr_2, child_crash_addr_3);
#endif  // defined(OFFICIAL_BUILD)
}
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_IOS) && \
    (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY))

int g_child_crash_pipe;

void CheckCrashTestSighandler(int, siginfo_t* info, void* context_ptr) {
  // Conversely to what clearly stated in "man 2 sigaction", some Linux kernels
  // do NOT populate the |info->si_addr| in the case of a SIGTRAP. Hence we
  // need the arch-specific boilerplate below, which is inspired by breakpad.
  // At the same time, on OSX, ucontext.h is deprecated but si_addr works fine.
  uintptr_t crash_addr = 0;
#if BUILDFLAG(IS_MAC)
  crash_addr = reinterpret_cast<uintptr_t>(info->si_addr);
#else  // OS_*
  ucontext_t* context = reinterpret_cast<ucontext_t*>(context_ptr);
#if defined(ARCH_CPU_X86)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_EIP]);
#elif defined(ARCH_CPU_X86_64)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RIP]);
#elif defined(ARCH_CPU_ARMEL)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.arm_pc);
#elif defined(ARCH_CPU_ARM64)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.pc);
#endif  // ARCH_*
#endif  // OS_*
  HANDLE_EINTR(write(g_child_crash_pipe, &crash_addr, sizeof(uintptr_t)));
  _exit(0);
}

// CHECK causes a direct crash (without jumping to another function) only in
// official builds. Unfortunately, continuous test coverage on official builds
// is lower. DO_CHECK here falls back on a home-brewed implementation in
// non-official builds, to catch regressions earlier in the CQ.
#if !CHECK_WILL_STREAM()
#define DO_CHECK CHECK
#else
#define DO_CHECK(cond)      \
  if (!(cond)) {            \
    base::ImmediateCrash(); \
  }
#endif

void CrashChildMain(int death_location) {
  struct sigaction act = {};
  act.sa_sigaction = CheckCrashTestSighandler;
  act.sa_flags = SA_SIGINFO;
  ASSERT_EQ(0, sigaction(SIGTRAP, &act, nullptr));
  ASSERT_EQ(0, sigaction(SIGBUS, &act, nullptr));
  ASSERT_EQ(0, sigaction(SIGILL, &act, nullptr));
  ASSERT_EQ(0, sigaction(SIGABRT, &act, nullptr));
  DO_CHECK(death_location != 1);
  DO_CHECK(death_location != 2);
  printf("\n");
  DO_CHECK(death_location != 3);

  // Should never reach this point.
  const uintptr_t failed = 0;
  HANDLE_EINTR(write(g_child_crash_pipe, &failed, sizeof(uintptr_t)));
}

void SpawnChildAndCrash(int death_location, uintptr_t* child_crash_addr) {
  int pipefd[2];
  ASSERT_EQ(0, pipe(pipefd));

  int pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {      // child process.
    close(pipefd[0]);  // Close reader (parent) end.
    g_child_crash_pipe = pipefd[1];
    CrashChildMain(death_location);
    FAIL() << "The child process was supposed to crash. It didn't.";
  }

  close(pipefd[1]);  // Close writer (child) end.
  DCHECK(child_crash_addr);
  int res = HANDLE_EINTR(read(pipefd[0], child_crash_addr, sizeof(uintptr_t)));
  ASSERT_EQ(static_cast<int>(sizeof(uintptr_t)), res);
}

TEST_F(LoggingTest, CheckCausesDistinctBreakpoints) {
  uintptr_t child_crash_addr_1 = 0;
  uintptr_t child_crash_addr_2 = 0;
  uintptr_t child_crash_addr_3 = 0;

  SpawnChildAndCrash(1, &child_crash_addr_1);
  SpawnChildAndCrash(2, &child_crash_addr_2);
  SpawnChildAndCrash(3, &child_crash_addr_3);

  ASSERT_NE(0u, child_crash_addr_1);
  ASSERT_NE(0u, child_crash_addr_2);
  ASSERT_NE(0u, child_crash_addr_3);

#if defined(OFFICIAL_BUILD)
  // In unofficial builds, we'll end up in std::abort
  // for each crash. In official builds, we should get a different
  // crash address for each location.
  ASSERT_NE(child_crash_addr_1, child_crash_addr_2);
  ASSERT_NE(child_crash_addr_1, child_crash_addr_3);
  ASSERT_NE(child_crash_addr_2, child_crash_addr_3);
#endif
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(LoggingTest, DebugLoggingReleaseBehavior) {
#if DCHECK_IS_ON()
  int debug_only_variable = 1;
#endif
  // These should avoid emitting references to |debug_only_variable|
  // in release mode.
  DLOG_IF(INFO, debug_only_variable) << "test";
  DLOG_ASSERT(debug_only_variable) << "test";
  DPLOG_IF(INFO, debug_only_variable) << "test";
  DVLOG_IF(1, debug_only_variable) << "test";
}

TEST_F(LoggingTest, NestedLogAssertHandlers) {
  if (LOGGING_DFATAL != LOGGING_FATAL) {
    GTEST_SKIP() << "Test relies on DFATAL being FATAL for "
                    "NestedLogAssertHandlers to fire.";
  }

  ::testing::InSequence dummy;
  ::testing::StrictMock<MockLogAssertHandler> handler_a, handler_b;

  EXPECT_CALL(
      handler_a,
      HandleLogAssert(
          _, _, std::string_view("First assert must be caught by handler_a"),
          _));
  EXPECT_CALL(
      handler_b,
      HandleLogAssert(
          _, _, std::string_view("Second assert must be caught by handler_b"),
          _));
  EXPECT_CALL(
      handler_a,
      HandleLogAssert(
          _, _,
          std::string_view("Last assert must be caught by handler_a again"),
          _));

  logging::ScopedLogAssertHandler scoped_handler_a(base::BindRepeating(
      &MockLogAssertHandler::HandleLogAssert, base::Unretained(&handler_a)));

  // Using LOG(DFATAL) rather than LOG(FATAL) as the latter is not cancellable.
  LOG(DFATAL) << "First assert must be caught by handler_a";

  {
    logging::ScopedLogAssertHandler scoped_handler_b(base::BindRepeating(
        &MockLogAssertHandler::HandleLogAssert, base::Unretained(&handler_b)));
    LOG(DFATAL) << "Second assert must be caught by handler_b";
  }

  LOG(DFATAL) << "Last assert must be caught by handler_a again";
}

// Test that defining an operator<< for a type in a namespace doesn't prevent
// other code in that namespace from calling the operator<<(ostream, wstring)
// defined by logging.h. This can fail if operator<<(ostream, wstring) can't be
// found by ADL, since defining another operator<< prevents name lookup from
// looking in the global namespace.
namespace nested_test {
  class Streamable {};
  [[maybe_unused]] std::ostream& operator<<(std::ostream& out,
                                            const Streamable&) {
    return out << "Streamable";
  }
  TEST_F(LoggingTest, StreamingWstringFindsCorrectOperator) {
    std::wstring wstr = L"Hello World";
    std::ostringstream ostr;
    ostr << wstr;
    EXPECT_EQ("Hello World", ostr.str());
  }
}  // namespace nested_test

TEST_F(LoggingTest, LogPrefix) {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static base::NoDestructor<std::string> log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string = str;
    return true;
  });

  // Logging with a prefix includes the prefix string.
  const char kPrefix[] = "prefix";
  SetLogPrefix(kPrefix);
  LOG(ERROR) << "test";  // Writes into |log_string|.
  EXPECT_NE(std::string::npos, log_string->find(kPrefix));
  // Logging without a prefix does not include the prefix string.
  SetLogPrefix(nullptr);
  LOG(ERROR) << "test";  // Writes into |log_string|.
  EXPECT_EQ(std::string::npos, log_string->find(kPrefix));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(LoggingTest, LogCrosSyslogFormat) {
  // Set log format to syslog format.
  scoped_logging_settings().SetLogFormat(LogFormat::LOG_FORMAT_SYSLOG);

  const char* kTimestampPattern = R"(\d\d\d\d\-\d\d\-\d\d)"             // date
                                  R"(T\d\d\:\d\d\:\d\d\.\d\d\d\d\d\d)"  // time
                                  R"(Z.+\n)";  // timezone

  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static base::NoDestructor<std::string> log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string = str;
    return true;
  });

  {
    // All flags are true.
    SetLogItems(true, true, true, true);
    const char* kExpected =
        R"(\S+ \d+ ERROR \S+\[\d+:\d+\]\: \[\S+\] message\n)";

    LOG(ERROR) << "message";

    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kTimestampPattern));
    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kExpected));
  }

  {
    // Timestamp is true.
    SetLogItems(false, false, true, false);
    const char* kExpected = R"(\S+ ERROR \S+\: \[\S+\] message\n)";

    LOG(ERROR) << "message";

    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kTimestampPattern));
    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kExpected));
  }

  {
    // PID and timestamp are true.
    SetLogItems(true, false, true, false);
    const char* kExpected = R"(\S+ ERROR \S+\[\d+\]: \[\S+\] message\n)";

    LOG(ERROR) << "message";

    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kTimestampPattern));
    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kExpected));
  }

  {
    // ThreadID and timestamp are true.
    SetLogItems(false, true, true, false);
    const char* kExpected = R"(\S+ ERROR \S+\[:\d+\]: \[\S+\] message\n)";

    LOG(ERROR) << "message";

    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kTimestampPattern));
    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kExpected));
  }

  {
    // All flags are false.
    SetLogItems(false, false, false, false);
    const char* kExpected = R"(ERROR \S+: \[\S+\] message\n)";

    LOG(ERROR) << "message";

    EXPECT_THAT(*log_string, ::testing::MatchesRegex(kExpected));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// We define a custom operator<< for std::u16string so we can use it with
// logging. This tests that conversion.
TEST_F(LoggingTest, String16) {
  // Basic stream test.
  {
    std::ostringstream stream;
    stream << "Empty '" << std::u16string() << "' standard '"
           << std::u16string(u"Hello, world") << "'";
    EXPECT_STREQ("Empty '' standard 'Hello, world'", stream.str().c_str());
  }

  // Interesting edge cases.
  {
    // These should each get converted to the invalid character: EF BF BD.
    std::u16string initial_surrogate;
    initial_surrogate.push_back(0xd800);
    std::u16string final_surrogate;
    final_surrogate.push_back(0xdc00);

    // Old italic A = U+10300, will get converted to: F0 90 8C 80 'z'.
    std::u16string surrogate_pair;
    surrogate_pair.push_back(0xd800);
    surrogate_pair.push_back(0xdf00);
    surrogate_pair.push_back('z');

    // Will get converted to the invalid char + 's': EF BF BD 's'.
    std::u16string unterminated_surrogate;
    unterminated_surrogate.push_back(0xd800);
    unterminated_surrogate.push_back('s');

    std::ostringstream stream;
    stream << initial_surrogate << "," << final_surrogate << ","
           << surrogate_pair << "," << unterminated_surrogate;

    EXPECT_STREQ("\xef\xbf\xbd,\xef\xbf\xbd,\xf0\x90\x8c\x80z,\xef\xbf\xbds",
                 stream.str().c_str());
  }
}

// Tests that we don't VLOG from logging_unittest except when in the scope
// of the ScopedVmoduleSwitches.
TEST_F(LoggingTest, ScopedVmoduleSwitches) {
  EXPECT_TRUE(VLOG_IS_ON(0));

  // To avoid unreachable-code warnings when VLOG is disabled at compile-time.
  int expected_logs = 0;
  if (VLOG_IS_ON(0))
    expected_logs += 1;

  SetMinLogLevel(LOGGING_FATAL);

  {
    MockLogSource mock_log_source;
    EXPECT_CALL(mock_log_source, Log()).Times(0);

    VLOG(1) << mock_log_source.Log();
  }

  {
    ScopedVmoduleSwitches scoped_vmodule_switches;
    scoped_vmodule_switches.InitWithSwitches(__FILE__ "=1");
    MockLogSource mock_log_source;
    EXPECT_CALL(mock_log_source, Log())
        .Times(expected_logs)
        .WillRepeatedly(Return("log message"));

    VLOG(1) << mock_log_source.Log();
  }

  {
    MockLogSource mock_log_source;
    EXPECT_CALL(mock_log_source, Log()).Times(0);

    VLOG(1) << mock_log_source.Log();
  }
}

TEST_F(LoggingTest, BuildCrashString) {
  EXPECT_EQ("file.cc:42: ",
            LogMessage("file.cc", 42, LOGGING_ERROR).BuildCrashString());

  // BuildCrashString() should strip path/to/file prefix.
  LogMessage msg(
#if BUILDFLAG(IS_WIN)
      "..\\foo\\bar\\file.cc",
#else
      "../foo/bar/file.cc",
#endif  // BUILDFLAG(IS_WIN)
      42, LOGGING_ERROR);
  msg.stream() << "Hello";
  EXPECT_EQ("file.cc:42: Hello", msg.BuildCrashString());
}

TEST_F(LoggingTest, SystemErrorNotChanged) {
  auto set_last_error = [](logging::SystemErrorCode error) {
#if BUILDFLAG(IS_WIN)
    ::SetLastError(error);
#else
    errno = error;
#endif
  };

  SystemErrorCode during_streaming = 0;
  SystemErrorCode set_during_streaming = 0;

  set_last_error(SystemErrorCode(123));
  LOG(WARNING) << (during_streaming = GetLastSystemErrorCode())
               << (set_last_error(SystemErrorCode(42)),
                   set_during_streaming = GetLastSystemErrorCode());

  // Initializing the LogMessage shouldn't change the observable error code.
  EXPECT_EQ(SystemErrorCode(123), during_streaming);
  // Verify that we can set and get the error code during streaming.
  EXPECT_EQ(SystemErrorCode(42), set_during_streaming);
  // Verify that the last set error code (during streaming) is preserved after
  // logging as well.
  EXPECT_EQ(SystemErrorCode(42), GetLastSystemErrorCode());

  // Repeat the test above but using PLOG.
  during_streaming = 0;
  set_during_streaming = 0;
  set_last_error(SystemErrorCode(123));
  PLOG(ERROR) << (during_streaming = GetLastSystemErrorCode())
              << (set_last_error(SystemErrorCode(42)),
                  set_during_streaming = GetLastSystemErrorCode());

  EXPECT_EQ(SystemErrorCode(123), during_streaming);
  EXPECT_EQ(SystemErrorCode(42), set_during_streaming);
  EXPECT_EQ(SystemErrorCode(42), GetLastSystemErrorCode());
}

TEST_F(LoggingTest, CorrectSystemErrorUsed) {
  auto set_last_error = [](logging::SystemErrorCode error) {
#if BUILDFLAG(IS_WIN)
    ::SetLastError(error);
#else
    errno = error;
#endif
  };

  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static base::NoDestructor<std::string> log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string = str;
    return true;
  });

  const SystemErrorCode kTestError = 28;
  const std::string kExpectedSystemErrorMsg =
      SystemErrorCodeToString(kTestError);

  set_last_error(kTestError);
  PLOG(ERROR);

  // Test that the last system error code got printed as expected.
  EXPECT_NE(std::string::npos, log_string->find(kExpectedSystemErrorMsg));

  if (DCHECK_IS_ON()) {
    *log_string = "";
    set_last_error(kTestError);
    DPLOG(ERROR);

    EXPECT_NE(std::string::npos, log_string->find(kExpectedSystemErrorMsg));
  }
}

TEST_F(LoggingTest, BuildTimeVLOG) {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static base::NoDestructor<std::string> log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string = str;
    return true;
  });

  // No VLOG by default.
  EXPECT_FALSE(VLOG_IS_ON(1));
  VLOG(1) << "Expect not logged";
  EXPECT_TRUE(log_string->empty());

  // Re-define ENABLED_VLOG_LEVEL to enable VLOG(1).
  // Note that ENABLED_VLOG_LEVEL has impact on all the code after it so please
  // keep this test case the last one in this file.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

  EXPECT_TRUE(VLOG_IS_ON(1));
  EXPECT_FALSE(VLOG_IS_ON(2));

  VLOG(1) << "Expect logged";
  EXPECT_THAT(*log_string, ::testing::MatchesRegex(".* Expect logged\n"));

  log_string->clear();
  VLOG(2) << "Expect not logged";
  EXPECT_TRUE(log_string->empty());
}

// NO NEW TESTS HERE
// The test above redefines ENABLED_VLOG_LEVEL, so new tests should be added
// before it.

}  // namespace

}  // namespace logging
