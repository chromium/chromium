// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#ifdef BASE_CHECK_H_
#error "logging.h should not include check.h"
#endif

#include <limits.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <tuple>
#include <vector>

#include "base/base_export.h"
#include "base/debug/crash_logging.h"
#include "base/immediate_crash.h"
#include "base/pending_task.h"
#include "base/strings/string_piece.h"
#include "base/task/common/task_annotator.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_NACL)
#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#endif  // !BUILDFLAG(IS_NACL)

#if defined(LEAK_SANITIZER) && !BUILDFLAG(IS_NACL)
#include "base/debug/leak_annotations.h"
#endif  // defined(LEAK_SANITIZER) && !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_WIN)
#include <io.h>
#include <windows.h>
typedef HANDLE FileHandle;
// Windows warns on using write().  It prefers _write().
#define write(fd, buf, count) _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2

#elif BUILDFLAG(IS_APPLE)
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <os/log.h>

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_NACL)
#include <sys/time.h>  // timespec doesn't seem to be in <time.h>
#endif
#include <time.h>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scoped_fx_logger.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include <android/log.h>
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "base/process/process_handle.h"
#define MAX_PATH PATH_MAX
typedef FILE* FileHandle;
#endif

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/stack.h"
#include "base/debug/activity_tracker.h"
#include "base/debug/alias.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_logging_settings.h"
#include "base/threading/platform_thread.h"
#include "base/vlog.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/posix/safe_strerror.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/scoped_file.h"
#endif

namespace logging {

namespace {

int g_min_log_level = 0;

#if BUILDFLAG(USE_RUNTIME_VLOG)
// NOTE: Once |g_vlog_info| has been initialized, it might be in use
// by another thread. Never delete the old VLogInfo, just create a second
// one and overwrite. We need to use leak-san annotations on this intentional
// leak.
//
// This can be read/written on multiple threads. In tests we don't see that
// causing a problem as updates tend to happen early. Atomic ensures there are
// no problems. To avoid some of the overhead of Atomic, we use
// |load(std::memory_order_acquire)| and |store(...,
// std::memory_order_release)| when reading or writing. This guarantees that the
// referenced object is available at the time the |g_vlog_info| is read and that
// |g_vlog_info| is updated atomically.
//
// Do not access this directly. You must use |GetVlogInfo|, |InitializeVlogInfo|
// and/or |ExchangeVlogInfo|.
std::atomic<VlogInfo*> g_vlog_info = nullptr;

VlogInfo* GetVlogInfo() {
  return g_vlog_info.load(std::memory_order_acquire);
}

// Sets g_vlog_info if it is not already set. Checking that it's not already set
// prevents logging initialization (which can come late in test setup) from
// overwriting values set via ScopedVmoduleSwitches.
bool InitializeVlogInfo(VlogInfo* vlog_info) {
  VlogInfo* previous_vlog_info = nullptr;
  return g_vlog_info.compare_exchange_strong(previous_vlog_info, vlog_info);
}

VlogInfo* ExchangeVlogInfo(VlogInfo* vlog_info) {
  return g_vlog_info.exchange(vlog_info);
}

// Creates a VlogInfo from the commandline if it has been initialized and if it
// contains relevant switches, otherwise this returns |nullptr|.
std::unique_ptr<VlogInfo> VlogInfoFromCommandLine() {
  if (!base::CommandLine::InitializedForCurrentProcess())
    return nullptr;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kV) &&
      !command_line->HasSwitch(switches::kVModule)) {
    return nullptr;
  }
#if defined(LEAK_SANITIZER) && !BUILDFLAG(IS_NACL)
  // See comments on |g_vlog_info|.
  ScopedLeakSanitizerDisabler lsan_disabler;
#endif  // defined(LEAK_SANITIZER)
  return std::make_unique<VlogInfo>(
      command_line->GetSwitchValueASCII(switches::kV),
      command_line->GetSwitchValueASCII(switches::kVModule), &g_min_log_level);
}

// If the commandline is initialized for the current process this will
// initialize g_vlog_info. If there are no VLOG switches, it will initialize it
// to |nullptr|.
void MaybeInitializeVlogInfo() {
  if (base::CommandLine::InitializedForCurrentProcess()) {
    std::unique_ptr<VlogInfo> vlog_info = VlogInfoFromCommandLine();
    if (vlog_info) {
      // VlogInfoFromCommandLine is annotated with ScopedLeakSanitizerDisabler
      // so it's allowed to leak. If the object was installed, we release it.
      if (InitializeVlogInfo(vlog_info.get())) {
        vlog_info.release();
      }
    }
  }
}
#endif  // BUILDFLAG(USE_RUNTIME_VLOG)

#if !BUILDFLAG(USE_RUNTIME_VLOG) && DCHECK_IS_ON()

// Warn developers that vlog command line settings are being ignored.
void MaybeWarnVmodule() {
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kV) ||
        command_line->HasSwitch(switches::kVModule)) {
      LOG(WARNING)
          << "--" << switches::kV << " and --" << switches::kVModule
          << " are currently ignored. See comments in base/logging.h on "
             "proper usage of USE_RUNTIME_VLOG.";
    }
  }
}

#endif  // !BUILDFLAG(USE_RUNTIME_VLOG) && DCHECK_IS_ON()

const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};
static_assert(LOGGING_NUM_SEVERITIES == std::size(log_severity_names),
              "Incorrect number of log_severity_names");

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOGGING_NUM_SEVERITIES)
    return log_severity_names[severity];
  return "UNKNOWN";
}

// Specifies the process' logging sink(s), represented as a combination of
// LoggingDestination values joined by bitwise OR.
uint32_t g_logging_destination = LOG_DEFAULT;

#if BUILDFLAG(IS_CHROMEOS)
// Specifies the format of log header for chrome os.
LogFormat g_log_format = LogFormat::LOG_FORMAT_SYSLOG;
#endif

#if BUILDFLAG(IS_FUCHSIA)
// Retains system logging structures.
base::ScopedFxLogger& GetScopedFxLogger() {
  static base::NoDestructor<base::ScopedFxLogger> logger;
  return *logger;
}
#endif

// For LOGGING_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LOGGING_ERROR;

// Which log file to use? This is initialized by InitLogging or
// will be lazily initialized to the default value when it is
// first needed.
using PathString = base::FilePath::StringType;
PathString* g_log_file_name = nullptr;

// This file is lazily opened and the handle may be nullptr
FileHandle g_log_file = nullptr;

// What should be prepended to each message?
bool g_log_process_id = false;
bool g_log_thread_id = false;
bool g_log_timestamp = true;
bool g_log_tickcount = false;
const char* g_log_prefix = nullptr;

// Should we pop up fatal debug messages in a dialog?
bool show_error_dialogs = false;

// An assert handler override specified by the client to be called instead of
// the debug message dialog and process termination. Assert handlers are stored
// in stack to allow overriding and restoring.
base::stack<LogAssertHandlerFunction>& GetLogAssertHandlerStack() {
  static base::NoDestructor<base::stack<LogAssertHandlerFunction>> instance;
  return *instance;
}

// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction g_log_message_handler = nullptr;

uint64_t TickCount() {
#if BUILDFLAG(IS_WIN)
  return GetTickCount();
#elif BUILDFLAG(IS_FUCHSIA)
  return static_cast<uint64_t>(
      zx_clock_get_monotonic() /
      static_cast<zx_time_t>(base::Time::kNanosecondsPerMicrosecond));
#elif BUILDFLAG(IS_APPLE)
  return mach_absolute_time();
#elif BUILDFLAG(IS_NACL)
  // NaCl sadly does not have _POSIX_TIMERS enabled in sys/features.h
  // So we have to use clock() for now.
  return clock();
#elif BUILDFLAG(IS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64_t absolute_micro = static_cast<uint64_t>(ts.tv_sec) * 1000000 +
                            static_cast<uint64_t>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
}

void DeleteFilePath(const PathString& log_name) {
#if BUILDFLAG(IS_WIN)
  DeleteFile(log_name.c_str());
#elif BUILDFLAG(IS_NACL)
  // Do nothing; unlink() isn't supported on NaCl.
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  unlink(log_name.c_str());
#else
#error Unsupported platform
#endif
}

PathString GetDefaultLogFile() {
#if BUILDFLAG(IS_WIN)
  // On Windows we use the same path as the exe.
  wchar_t module_name[MAX_PATH];
  GetModuleFileName(nullptr, module_name, MAX_PATH);

  PathString log_name = module_name;
  PathString::size_type last_backslash = log_name.rfind('\\', log_name.size());
  if (last_backslash != PathString::npos)
    log_name.erase(last_backslash + 1);
  log_name += FILE_PATH_LITERAL("debug.log");
  return log_name;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // On other platforms we just use the current directory.
  return PathString("debug.log");
#endif
}

// We don't need locks on Windows for atomically appending to files. The OS
// provides this functionality.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// Provides a lock to synchronize appending to the log file across
// threads. This can be required to support NFS file systems even on OSes that
// provide atomic append operations in most cases. It should be noted that this
// lock is not not shared across processes. When using NFS filesystems
// protection against clobbering between different processes will be best-effort
// and provided by the OS. See
// https://man7.org/linux/man-pages/man2/open.2.html.
//
// The lock also protects initializing and closing the log file which can
// happen concurrently with logging on some platforms like ChromeOS that need to
// redirect logging by calling BaseInitLoggingImpl() twice.
base::Lock& GetLoggingLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// Called by logging functions to ensure that |g_log_file| is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. |g_log_file| will be nullptr in this case.
bool InitializeLogFileHandle() {
  if (g_log_file)
    return true;

  if (!g_log_file_name) {
    // Nobody has called InitLogging to specify a debug log file, so here we
    // initialize the log file name to a default.
    g_log_file_name = new PathString(GetDefaultLogFile());
  }

  if ((g_logging_destination & LOG_TO_FILE) == 0)
    return true;

#if BUILDFLAG(IS_WIN)
  // The FILE_APPEND_DATA access mask ensures that the file is atomically
  // appended to across accesses from multiple threads.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364399(v=vs.85).aspx
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
  g_log_file = CreateFile(g_log_file_name->c_str(), FILE_APPEND_DATA,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
    // We are intentionally not using FilePath or FileUtil here to reduce the
    // dependencies of the logging implementation. For e.g. FilePath and
    // FileUtil depend on shell32 and user32.dll. This is not acceptable for
    // some consumers of base logging like chrome_elf, etc.
    // Please don't change the code below to use FilePath.
    // try the current directory
    wchar_t system_buffer[MAX_PATH];
    system_buffer[0] = 0;
    DWORD len = ::GetCurrentDirectory(std::size(system_buffer), system_buffer);
    if (len == 0 || len > std::size(system_buffer))
      return false;

    *g_log_file_name = system_buffer;
    // Append a trailing backslash if needed.
    if (g_log_file_name->back() != L'\\')
      *g_log_file_name += FILE_PATH_LITERAL("\\");
    *g_log_file_name += FILE_PATH_LITERAL("debug.log");

    g_log_file = CreateFile(g_log_file_name->c_str(), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
      g_log_file = nullptr;
      return false;
    }
  }
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  g_log_file = fopen(g_log_file_name->c_str(), "a");
  if (g_log_file == nullptr)
    return false;
#else
#error Unsupported platform
#endif

  return true;
}

void CloseFile(FileHandle log) {
#if BUILDFLAG(IS_WIN)
  CloseHandle(log);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  fclose(log);
#else
#error Unsupported platform
#endif
}

void CloseLogFileUnlocked() {
  if (!g_log_file)
    return;

  CloseFile(g_log_file);
  g_log_file = nullptr;

  // If we initialized logging via an externally-provided file descriptor, we
  // won't have a log path set and shouldn't try to reopen the log file.
  if (!g_log_file_name)
    g_logging_destination &= ~LOG_TO_FILE;
}

#if BUILDFLAG(IS_FUCHSIA)
inline FuchsiaLogSeverity LogSeverityToFuchsiaLogSeverity(
    LogSeverity severity) {
  switch (severity) {
    case LOGGING_INFO:
      return FUCHSIA_LOG_INFO;
    case LOGGING_WARNING:
      return FUCHSIA_LOG_WARNING;
    case LOGGING_ERROR:
      return FUCHSIA_LOG_ERROR;
    case LOGGING_FATAL:
      // Don't use FX_LOG_FATAL, otherwise fx_logger_log() will abort().
      return FUCHSIA_LOG_ERROR;
  }
  if (severity > -3) {
    // LOGGING_VERBOSE levels 1 and 2.
    return FUCHSIA_LOG_DEBUG;
  }
  // LOGGING_VERBOSE levels 3 and higher, or incorrect levels.
  return FUCHSIA_LOG_TRACE;
}
#endif  // BUILDFLAG(IS_FUCHSIA)

void WriteToFd(int fd, const char* data, size_t length) {
  size_t bytes_written = 0;
  long rv;
  while (bytes_written < length) {
    rv = HANDLE_EINTR(write(fd, data + bytes_written, length - bytes_written));
    if (rv < 0) {
      // Give up, nothing we can do now.
      break;
    }
    bytes_written += static_cast<size_t>(rv);
  }
}

void SetLogFatalCrashKey(LogMessage* log_message) {
#if !BUILDFLAG(IS_NACL)
  // In case of an out-of-memory condition, this code could be reentered when
  // constructing and storing the key. Using a static is not thread-safe, but if
  // multiple threads are in the process of a fatal crash at the same time, this
  // should work.
  static bool guarded = false;
  if (guarded)
    return;

  base::AutoReset<bool> guard(&guarded, true);

  static auto* const crash_key = base::debug::AllocateCrashKeyString(
      "LOG_FATAL", base::debug::CrashKeySize::Size1024);
  base::debug::SetCrashKeyString(crash_key, log_message->BuildCrashString());

#endif  // !BUILDFLAG(IS_NACL)
}

std::string BuildCrashString(const char* file,
                             int line,
                             const char* message_without_prefix) {
  // Only log last path component.
  if (file) {
    const char* slash = strrchr(file,
#if BUILDFLAG(IS_WIN)
                                '\\'
#else
                                '/'
#endif  // BUILDFLAG(IS_WIN)
    );
    if (slash) {
      file = slash + 1;
    }
  }

  return base::StringPrintf("%s:%d: %s", file, line, message_without_prefix);
}

}  // namespace

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
// In DCHECK-enabled Chrome builds, allow the meaning of LOGGING_DCHECK to be
// determined at run-time. We default it to INFO, to avoid it triggering
// crashes before the run-time has explicitly chosen the behaviour.
BASE_EXPORT logging::LogSeverity LOGGING_DCHECK = LOGGING_INFO;
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

bool BaseInitLoggingImpl(const LoggingSettings& settings) {
#if BUILDFLAG(IS_NACL)
  // Can log only to the system debug log and stderr.
  CHECK_EQ(settings.logging_dest & ~(LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR),
           0u);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  g_log_format = settings.log_format;
#endif

#if BUILDFLAG(USE_RUNTIME_VLOG)
  MaybeInitializeVlogInfo();
#endif  // BUILDFLAG(USE_RUNTIME_VLOG)

#if !BUILDFLAG(USE_RUNTIME_VLOG) && DCHECK_IS_ON()
  MaybeWarnVmodule();
#endif  // !BUILDFLAG(USE_RUNTIME_VLOG) && DCHECK_IS_ON()

  g_logging_destination = settings.logging_dest;

#if BUILDFLAG(IS_FUCHSIA)
  if (g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) {
    GetScopedFxLogger() = base::ScopedFxLogger::CreateForProcess();
  }
#endif

  // ignore file options unless logging to file is set.
  if ((g_logging_destination & LOG_TO_FILE) == 0)
    return true;

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::AutoLock guard(GetLoggingLock());
#endif

  // Calling InitLogging twice or after some log call has already opened the
  // default log file will re-initialize to the new options.
  CloseLogFileUnlocked();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (settings.log_file) {
    DCHECK(!settings.log_file_path);
    g_log_file = settings.log_file;
    return true;
  }
#endif

  DCHECK(settings.log_file_path) << "LOG_TO_FILE set but no log_file_path!";

  if (!g_log_file_name)
    g_log_file_name = new PathString();
  *g_log_file_name = settings.log_file_path;
  if (settings.delete_old == DELETE_OLD_LOG_FILE)
    DeleteFilePath(*g_log_file_name);

  return InitializeLogFileHandle();
}

void SetMinLogLevel(int level) {
  g_min_log_level = std::min(LOGGING_FATAL, level);
}

int GetMinLogLevel() {
  return g_min_log_level;
}

bool ShouldCreateLogMessage(int severity) {
  if (severity < g_min_log_level)
    return false;

  // Return true here unless we know ~LogMessage won't do anything.
  return g_logging_destination != LOG_NONE || g_log_message_handler ||
         severity >= kAlwaysPrintErrorLevel;
}

// Returns true when LOG_TO_STDERR flag is set, or |severity| is high.
// If |severity| is high then true will be returned when no log destinations are
// set, or only LOG_TO_FILE is set, since that is useful for local development
// and debugging.
bool ShouldLogToStderr(int severity) {
  if (g_logging_destination & LOG_TO_STDERR)
    return true;

#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia will persist data logged to stdio by a component, so do not emit
  // logs to stderr unless explicitly configured to do so.
  return false;
#else
  if (severity >= kAlwaysPrintErrorLevel)
    return (g_logging_destination & ~LOG_TO_FILE) == LOG_NONE;
  return false;
#endif
}

int GetVlogVerbosity() {
  return std::max(-1, LOG_INFO - GetMinLogLevel());
}

int GetVlogLevelHelper(const char* file, size_t N) {
  DCHECK_GT(N, 0U);

#if BUILDFLAG(USE_RUNTIME_VLOG)
  // Note: |g_vlog_info| may change on a different thread during startup
  // (but will always be valid or nullptr).
  VlogInfo* vlog_info = GetVlogInfo();
  return vlog_info ?
      vlog_info->GetVlogLevel(base::StringPiece(file, N - 1)) :
      GetVlogVerbosity();
#else
  return GetVlogVerbosity();
#endif  // BUILDFLAG(USE_RUNTIME_VLOG)
}

void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount) {
  g_log_process_id = enable_process_id;
  g_log_thread_id = enable_thread_id;
  g_log_timestamp = enable_timestamp;
  g_log_tickcount = enable_tickcount;
}

void SetLogPrefix(const char* prefix) {
  DCHECK(!prefix ||
         base::ContainsOnlyChars(prefix, "abcdefghijklmnopqrstuvwxyz"));
  g_log_prefix = prefix;
}

void SetShowErrorDialogs(bool enable_dialogs) {
  show_error_dialogs = enable_dialogs;
}

ScopedLogAssertHandler::ScopedLogAssertHandler(
    LogAssertHandlerFunction handler) {
  GetLogAssertHandlerStack().push(std::move(handler));
}

ScopedLogAssertHandler::~ScopedLogAssertHandler() {
  GetLogAssertHandlerStack().pop();
}

void SetLogMessageHandler(LogMessageHandlerFunction handler) {
  g_log_message_handler = handler;
}

LogMessageHandlerFunction GetLogMessageHandler() {
  return g_log_message_handler;
}

#if !defined(NDEBUG)
// Displays a message box to the user with the error message in it.
// Used for fatal messages, where we close the app simultaneously.
// This is for developers only; we don't use this in circumstances
// (like release builds) where users could see it, since users don't
// understand these messages anyway.
void DisplayDebugMessageInDialog(const std::string& str) {
  if (str.empty())
    return;

  if (!show_error_dialogs)
    return;

#if BUILDFLAG(IS_WIN)
  // We intentionally don't implement a dialog on other platforms.
  // You can just look at stderr.
  if (base::win::IsUser32AndGdi32Available()) {
    MessageBoxW(nullptr, base::as_wcstr(base::UTF8ToUTF16(str)), L"Fatal error",
                MB_OK | MB_ICONHAND | MB_TOPMOST);
  } else {
    OutputDebugStringW(base::as_wcstr(base::UTF8ToUTF16(str)));
  }
#endif  // BUILDFLAG(IS_WIN)
}
#endif  // !defined(NDEBUG)

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const char* condition)
    : severity_(LOGGING_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << condition << ". ";
}

LogMessage::~LogMessage() {
  size_t stack_start = stream_.str().length();
#if !defined(OFFICIAL_BUILD) && !BUILDFLAG(IS_NACL) && !defined(__UCLIBC__) && \
    !BUILDFLAG(IS_AIX)
  if (severity_ == LOGGING_FATAL && !base::debug::BeingDebugged()) {
    // Include a stack trace on a fatal, unless a debugger is attached.
    base::debug::StackTrace stack_trace;
    stream_ << std::endl;  // Newline to separate from log message.
    stack_trace.OutputToStream(&stream_);
    base::debug::TaskTrace task_trace;
    if (!task_trace.empty())
      task_trace.OutputToStream(&stream_);

    // Include the IPC context, if any.
    // TODO(chrisha): Integrate with symbolization once those tools exist!
    const auto* task = base::TaskAnnotator::CurrentTaskForThread();
    if (task && task->ipc_hash) {
      stream_ << "IPC message handler context: "
              << base::StringPrintf("0x%08X", task->ipc_hash) << std::endl;
    }

    // Include the crash keys, if any.
    base::debug::OutputCrashKeysToStream(stream_);
  }
#endif
  stream_ << std::endl;
  std::string str_newline(stream_.str());
  TRACE_LOG_MESSAGE(
      file_, base::StringPiece(str_newline).substr(message_start_), line_);

  if (severity_ == LOGGING_FATAL)
    SetLogFatalCrashKey(this);

  // Give any log message handler first dibs on the message.
  if (g_log_message_handler &&
      g_log_message_handler(severity_, file_, line_, message_start_,
                            str_newline)) {
    // The handler took care of it, no further processing.
    return;
  }

  if ((g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) != 0) {
#if BUILDFLAG(IS_WIN)
    OutputDebugStringA(str_newline.c_str());
#elif BUILDFLAG(IS_APPLE)
    // In LOG_TO_SYSTEM_DEBUG_LOG mode, log messages are always written to
    // stderr. If stderr is /dev/null, also log via os_log. If there's something
    // weird about stderr, assume that log messages are going nowhere and log
    // via os_log too. Messages logged via os_log show up in Console.app.
    //
    // Programs started by launchd, as UI applications normally are, have had
    // stderr connected to /dev/null since OS X 10.8. Prior to that, stderr was
    // a pipe to launchd, which logged what it received (see log_redirect_fd in
    // 10.7.5 launchd-392.39/launchd/src/launchd_core_logic.c).
    //
    // Another alternative would be to determine whether stderr is a pipe to
    // launchd and avoid logging via os_log only in that case. See 10.7.5
    // CF-635.21/CFUtilities.c also_do_stderr(). This would result in logging to
    // both stderr and os_log even in tests, where it's undesirable to log to
    // the system log at all.
    const bool log_to_system = []() {
      struct stat stderr_stat;
      if (fstat(fileno(stderr), &stderr_stat) == -1) {
        return true;
      }
      if (!S_ISCHR(stderr_stat.st_mode)) {
        return false;
      }

      struct stat dev_null_stat;
      if (stat(_PATH_DEVNULL, &dev_null_stat) == -1) {
        return true;
      }

      return !S_ISCHR(dev_null_stat.st_mode) ||
             stderr_stat.st_rdev == dev_null_stat.st_rdev;
    }();

    if (log_to_system) {
      // Log roughly the same way that CFLog() and NSLog() would. See 10.10.5
      // CF-1153.18/CFUtilities.c __CFLogCString().
      CFBundleRef main_bundle = CFBundleGetMainBundle();
      CFStringRef main_bundle_id_cf =
          main_bundle ? CFBundleGetIdentifier(main_bundle) : nullptr;
      std::string main_bundle_id =
          main_bundle_id_cf ? base::SysCFStringRefToUTF8(main_bundle_id_cf)
                            : std::string("");

      const class OSLog {
       public:
        explicit OSLog(const char* subsystem)
            : os_log_(subsystem ? os_log_create(subsystem, "chromium_logging")
                                : OS_LOG_DEFAULT) {}
        OSLog(const OSLog&) = delete;
        OSLog& operator=(const OSLog&) = delete;
        ~OSLog() {
          if (os_log_ != OS_LOG_DEFAULT) {
            os_release(os_log_);
          }
        }
        os_log_t get() const { return os_log_; }

       private:
        os_log_t os_log_;
      } log(main_bundle_id.empty() ? nullptr : main_bundle_id.c_str());
      const os_log_type_t os_log_type = [](LogSeverity severity) {
        switch (severity) {
          case LOGGING_INFO:
            return OS_LOG_TYPE_INFO;
          case LOGGING_WARNING:
            return OS_LOG_TYPE_DEFAULT;
          case LOGGING_ERROR:
            return OS_LOG_TYPE_ERROR;
          case LOGGING_FATAL:
            return OS_LOG_TYPE_FAULT;
          case LOGGING_VERBOSE:
            return OS_LOG_TYPE_DEBUG;
          default:
            return OS_LOG_TYPE_DEFAULT;
        }
      }(severity_);
      os_log_with_type(log.get(), os_log_type, "%{public}s",
                       str_newline.c_str());
    }
#elif BUILDFLAG(IS_ANDROID)
    android_LogPriority priority =
        (severity_ < 0) ? ANDROID_LOG_VERBOSE : ANDROID_LOG_UNKNOWN;
    switch (severity_) {
      case LOGGING_INFO:
        priority = ANDROID_LOG_INFO;
        break;
      case LOGGING_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
      case LOGGING_ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
      case LOGGING_FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
    }
    const char kAndroidLogTag[] = "chromium";
#if DCHECK_IS_ON()
    // Split the output by new lines to prevent the Android system from
    // truncating the log.
    std::vector<std::string> lines = base::SplitString(
        str_newline, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    // str_newline has an extra newline appended to it (at the top of this
    // function), so skip the last split element to avoid needlessly
    // logging an empty string.
    lines.pop_back();
    for (const auto& line : lines)
      __android_log_write(priority, kAndroidLogTag, line.c_str());
#else
    // The Android system may truncate the string if it's too long.
    __android_log_write(priority, kAndroidLogTag, str_newline.c_str());
#endif
#elif BUILDFLAG(IS_FUCHSIA)
    // LogMessage() will silently drop the message if the logger is not valid.
    // Skip the final character of |str_newline|, since LogMessage() will add
    // a newline.
    const auto message = base::StringPiece(str_newline).substr(message_start_);
    GetScopedFxLogger().LogMessage(file_, static_cast<uint32_t>(line_),
                                   message.substr(0, message.size() - 1),
                                   LogSeverityToFuchsiaLogSeverity(severity_));
#endif  // BUILDFLAG(IS_FUCHSIA)
  }

  if (ShouldLogToStderr(severity_)) {
    // Not using fwrite() here, as there are crashes on Windows when CRT calls
    // malloc() internally, triggering an OOM crash. This likely means that the
    // process is close to OOM, but at least get the proper error message out,
    // and give the caller a chance to free() up some resources. For instance if
    // the calling code is:
    //
    // allocate_something();
    // if (!TryToDoSomething()) {
    //   LOG(ERROR) << "Something went wrong";
    //   free_something();
    // }
    WriteToFd(STDERR_FILENO, str_newline.data(), str_newline.size());
  }

  if ((g_logging_destination & LOG_TO_FILE) != 0) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // If the client app did not call InitLogging() and the lock has not
    // been created it will be done now on calling GetLoggingLock(). We do this
    // on demand, but if two threads try to do this at the same time, there will
    // be a race condition to create the lock. This is why InitLogging should be
    // called from the main thread at the beginning of execution.
    base::AutoLock guard(GetLoggingLock());
#endif
    if (InitializeLogFileHandle()) {
#if BUILDFLAG(IS_WIN)
      DWORD num_written;
      WriteFile(g_log_file,
                static_cast<const void*>(str_newline.c_str()),
                static_cast<DWORD>(str_newline.length()),
                &num_written,
                nullptr);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
      std::ignore =
          fwrite(str_newline.data(), str_newline.size(), 1, g_log_file);
      fflush(g_log_file);
#else
#error Unsupported platform
#endif
    }
  }

  if (severity_ == LOGGING_FATAL) {
    // Write the log message to the global activity tracker, if running.
    base::debug::GlobalActivityTracker* tracker =
        base::debug::GlobalActivityTracker::Get();
    if (tracker)
      tracker->RecordLogMessage(str_newline);

    char str_stack[1024];
    base::strlcpy(str_stack, str_newline.data(), std::size(str_stack));
    base::debug::Alias(&str_stack);

    if (!GetLogAssertHandlerStack().empty()) {
      LogAssertHandlerFunction log_assert_handler =
          GetLogAssertHandlerStack().top();

      if (log_assert_handler) {
        log_assert_handler.Run(
            file_, line_,
            base::StringPiece(str_newline.c_str() + message_start_,
                              stack_start - message_start_),
            base::StringPiece(str_newline.c_str() + stack_start));
      }
    } else {
      // Don't use the string with the newline, get a fresh version to send to
      // the debug message process. We also don't display assertions to the
      // user in release mode. The enduser can't do anything with this
      // information, and displaying message boxes when the application is
      // hosed can cause additional problems.
#ifndef NDEBUG
      if (!base::debug::BeingDebugged()) {
        // Displaying a dialog is unnecessary when debugging and can complicate
        // debugging.
        DisplayDebugMessageInDialog(stream_.str());
      }
#endif

      // Crash the process to generate a dump.
      base::ImmediateCrash();
    }
  }
}

std::string LogMessage::BuildCrashString() const {
  return logging::BuildCrashString(file(), line(),
                                   str().c_str() + message_start_);
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  base::StringPiece filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos)
    filename.remove_prefix(last_slash_pos + 1);

#if BUILDFLAG(IS_CHROMEOS)
  if (g_log_format == LogFormat::LOG_FORMAT_SYSLOG) {
    InitWithSyslogPrefix(
        filename, line, TickCount(), log_severity_name(severity_), g_log_prefix,
        g_log_process_id, g_log_thread_id, g_log_timestamp, g_log_tickcount);
  } else
#endif  // BUILDFLAG(IS_CHROMEOS)
  {
    // TODO(darin): It might be nice if the columns were fixed width.
    stream_ << '[';
    if (g_log_prefix)
      stream_ << g_log_prefix << ':';
    if (g_log_process_id)
      stream_ << base::GetUniqueIdForProcess() << ':';
    if (g_log_thread_id)
      stream_ << base::PlatformThread::CurrentId() << ':';
    if (g_log_timestamp) {
#if BUILDFLAG(IS_WIN)
      SYSTEMTIME local_time;
      GetLocalTime(&local_time);
      stream_ << std::setfill('0')
              << std::setw(2) << local_time.wMonth
              << std::setw(2) << local_time.wDay
              << '/'
              << std::setw(2) << local_time.wHour
              << std::setw(2) << local_time.wMinute
              << std::setw(2) << local_time.wSecond
              << '.'
              << std::setw(3) << local_time.wMilliseconds
              << ':';
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
      timeval tv;
      gettimeofday(&tv, nullptr);
      time_t t = tv.tv_sec;
      struct tm local_time;
      localtime_r(&t, &local_time);
      struct tm* tm_time = &local_time;
      stream_ << std::setfill('0')
              << std::setw(2) << 1 + tm_time->tm_mon
              << std::setw(2) << tm_time->tm_mday
              << '/'
              << std::setw(2) << tm_time->tm_hour
              << std::setw(2) << tm_time->tm_min
              << std::setw(2) << tm_time->tm_sec
              << '.'
              << std::setw(6) << tv.tv_usec
              << ':';
#else
#error Unsupported platform
#endif
    }
    if (g_log_tickcount)
      stream_ << TickCount() << ':';
    if (severity_ >= 0) {
      stream_ << log_severity_name(severity_);
    } else {
      stream_ << "VERBOSE" << -severity_;
    }
    stream_ << ":" << filename << "(" << line << ")] ";
  }
  message_start_ = stream_.str().length();
}

#if BUILDFLAG(IS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if BUILDFLAG(IS_WIN)
  return ::GetLastError();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return errno;
#endif
}

BASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code) {
#if BUILDFLAG(IS_WIN)
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             std::size(msgbuf), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    return base::CollapseWhitespaceASCII(msgbuf, true) +
           base::StringPrintf(" (0x%lX)", error_code);
  }
  return base::StringPrintf("Error (0x%lX) while retrieving error. (0x%lX)",
                            GetLastError(), error_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::safe_strerror(error_code) +
         base::StringPrintf(" (%d)", error_code);
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  DWORD last_error = err_;
  base::debug::Alias(&last_error);
}
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  int last_error = err_;
  base::debug::Alias(&last_error);
}
#endif  // BUILDFLAG(IS_WIN)

void CloseLogFile() {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::AutoLock guard(GetLoggingLock());
#endif
  CloseLogFileUnlocked();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
FILE* DuplicateLogFILE() {
  if ((g_logging_destination & LOG_TO_FILE) == 0 || !InitializeLogFileHandle())
    return nullptr;

  int log_fd = fileno(g_log_file);
  if (log_fd == -1)
    return nullptr;
  base::ScopedFD dup_fd(dup(log_fd));
  if (dup_fd == -1)
    return nullptr;
  FILE* duplicate = fdopen(dup_fd.get(), "a");
  if (!duplicate)
    return nullptr;
  std::ignore = dup_fd.release();
  return duplicate;
}
#endif

// Used for testing. Declared in test/scoped_logging_settings.h.
ScopedLoggingSettings::ScopedLoggingSettings()
    : min_log_level_(g_min_log_level),
      logging_destination_(g_logging_destination),
#if BUILDFLAG(IS_CHROMEOS)
      log_format_(g_log_format),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      enable_process_id_(g_log_process_id),
      enable_thread_id_(g_log_thread_id),
      enable_timestamp_(g_log_timestamp),
      enable_tickcount_(g_log_tickcount),
      log_prefix_(g_log_prefix),
      message_handler_(g_log_message_handler) {
  if (g_log_file_name)
    log_file_name_ = std::make_unique<PathString>(*g_log_file_name);
  // Duplicating |g_log_file| is complex & unnecessary for this test helpers'
  // use-cases, and so long as |g_log_file_name| is set, it will be re-opened
  // automatically anyway, when required, so just close the existing one.
  if (g_log_file) {
    CHECK(g_log_file_name) << "Un-named |log_file| is not supported.";
    CloseLogFileUnlocked();
  }
}

ScopedLoggingSettings::~ScopedLoggingSettings() {
  // Re-initialize logging via the normal path. This will clean up old file
  // name and handle state, including re-initializing the VLOG internal state.
  CHECK(InitLogging({
    .logging_dest = logging_destination_,
    .log_file_path = log_file_name_ ? log_file_name_->data() : nullptr,
#if BUILDFLAG(IS_CHROMEOS)
    .log_format = log_format_
#endif
  })) << "~ScopedLoggingSettings() failed to restore settings.";

  // Restore plain data settings.
  SetMinLogLevel(min_log_level_);
  SetLogItems(enable_process_id_, enable_thread_id_, enable_timestamp_,
              enable_tickcount_);
  SetLogPrefix(log_prefix_);
  SetLogMessageHandler(message_handler_);
}

#if BUILDFLAG(IS_CHROMEOS)
void ScopedLoggingSettings::SetLogFormat(LogFormat log_format) const {
  g_log_format = log_format;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RawLog(int level, const char* message) {
  if (level >= g_min_log_level && message) {
    const size_t message_len = strlen(message);
    WriteToFd(STDERR_FILENO, message, message_len);

    if (message_len > 0 && message[message_len - 1] != '\n') {
      long rv;
      do {
        rv = HANDLE_EINTR(write(STDERR_FILENO, "\n", 1));
        if (rv < 0) {
          // Give up, nothing we can do now.
          break;
        }
      } while (rv != 1);
    }
  }

  if (level == LOGGING_FATAL)
    base::ImmediateCrash();
}

// This was defined at the beginning of this file.
#undef write

#if BUILDFLAG(IS_WIN)
bool IsLoggingToFileEnabled() {
  return g_logging_destination & LOG_TO_FILE;
}

std::wstring GetLogFileFullPath() {
  if (g_log_file_name)
    return *g_log_file_name;
  return std::wstring();
}
#endif

#if !BUILDFLAG(USE_RUNTIME_VLOG)
int GetDisableAllVLogLevel() {
  return -1;
}
#endif  // !BUILDFLAG(USE_RUNTIME_VLOG)

// Used for testing. Declared in test/scoped_logging_settings.h.
ScopedVmoduleSwitches::ScopedVmoduleSwitches() = default;
#if BUILDFLAG(USE_RUNTIME_VLOG)
VlogInfo* ScopedVmoduleSwitches::CreateVlogInfoWithSwitches(
    const std::string& vmodule_switch) {
  // Try get a VlogInfo on which to base this.
  // First ensure that VLOG has been initialized.
  MaybeInitializeVlogInfo();

  // Getting this now and setting it later is racy, however if a
  // ScopedVmoduleSwitches is being used on multiple threads that requires
  // further coordination and avoids this race.
  VlogInfo* base_vlog_info = GetVlogInfo();
  if (!base_vlog_info) {
    // Base is |nullptr|, so just create it from scratch.
    return new VlogInfo(/*v_switch_=*/"", vmodule_switch, &g_min_log_level);
  }
  return base_vlog_info->WithSwitches(vmodule_switch);
}

void ScopedVmoduleSwitches::InitWithSwitches(
    const std::string& vmodule_switch) {
  // Make sure we are only initialized once.
  CHECK(!scoped_vlog_info_);
  {
#if defined(LEAK_SANITIZER) && !BUILDFLAG(IS_NACL)
    // See comments on |g_vlog_info|.
    ScopedLeakSanitizerDisabler lsan_disabler;
#endif  // defined(LEAK_SANITIZER)
    scoped_vlog_info_ = CreateVlogInfoWithSwitches(vmodule_switch);
  }
  previous_vlog_info_ = ExchangeVlogInfo(scoped_vlog_info_);
}

ScopedVmoduleSwitches::~ScopedVmoduleSwitches() {
  VlogInfo* replaced_vlog_info = ExchangeVlogInfo(previous_vlog_info_);
  // Make sure something didn't replace our scoped VlogInfo while we weren't
  // looking.
  CHECK_EQ(replaced_vlog_info, scoped_vlog_info_);
}
#else
void ScopedVmoduleSwitches::InitWithSwitches(
    const std::string& vmodule_switch) {}

ScopedVmoduleSwitches::~ScopedVmoduleSwitches() = default;
#endif  // BUILDFLAG(USE_RUNTIME_VLOG)

}  // namespace logging

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
  return out << (wstr ? base::WStringPiece(wstr) : base::WStringPiece());
}

std::ostream& std::operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << base::WStringPiece(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const char16_t* str16) {
  return out << (str16 ? base::StringPiece16(str16) : base::StringPiece16());
}

std::ostream& std::operator<<(std::ostream& out, const std::u16string& str16) {
  return out << base::StringPiece16(str16);
}
