// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/debugger.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string_view>

#include "base/check_op.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif

#if BUILDFLAG(IS_APPLE)
#include <AvailabilityMacros.h>
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_BSD)
#include <sys/sysctl.h>
#endif

#if BUILDFLAG(IS_FREEBSD)
#include <sys/user.h>
#endif

#include <ostream>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/debugging_buildflags.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"

#if defined(USE_SYMBOLIZE)
#include "base/third_party/symbolize/symbolize.h"  // nogncheck
#endif

namespace base {
namespace debug {

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_BSD)

// Based on Apple's recommended method as described in
// http://developer.apple.com/qa/qa2004/qa1361.html
bool BeingDebugged() {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.
  //
  // While some code used below may be async-signal unsafe, note how
  // the result is cached (see |is_set| and |being_debugged| static variables
  // right below). If this code is properly warmed-up early
  // in the start-up process, it should be safe to use later.

  // If the process is sandboxed then we can't use the sysctl, so cache the
  // value.
  static bool is_set = false;
  static bool being_debugged = false;

  if (is_set)
    return being_debugged;

  // Initialize mib, which tells sysctl what info we want.  In this case,
  // we're looking for information about a specific process ID.
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid()
#if BUILDFLAG(IS_OPENBSD)
        ,
    sizeof(struct kinfo_proc),
    0
#endif
  };

  // Caution: struct kinfo_proc is marked __APPLE_API_UNSTABLE.  The source and
  // binary interfaces may change.
  struct kinfo_proc info;
  size_t info_size = sizeof(info);

#if BUILDFLAG(IS_OPENBSD)
  if (sysctl(mib, std::size(mib), NULL, &info_size, NULL, 0) < 0)
    return -1;

  mib[5] = (info_size / sizeof(struct kinfo_proc));
#endif

  int sysctl_result = sysctl(mib, std::size(mib), &info, &info_size, NULL, 0);
  DCHECK_EQ(sysctl_result, 0);
  if (sysctl_result != 0) {
    is_set = true;
    being_debugged = false;
    return being_debugged;
  }

  // This process is being debugged if the P_TRACED flag is set.
  is_set = true;
#if BUILDFLAG(IS_FREEBSD)
  being_debugged = (info.ki_flag & P_TRACED) != 0;
#elif BUILDFLAG(IS_BSD)
  being_debugged = (info.p_flag & P_TRACED) != 0;
#else
  being_debugged = (info.kp_proc.p_flag & P_TRACED) != 0;
#endif
  return being_debugged;
}

void VerifyDebugger() {
#if BUILDFLAG(ENABLE_LLDBINIT_WARNING)
  if (Environment::Create()->HasVar("CHROMIUM_LLDBINIT_SOURCED"))
    return;
  if (!BeingDebugged())
    return;
  DCHECK(false)
      << "Detected lldb without sourcing //tools/lldb/lldbinit.py. lldb may "
         "not be able to find debug symbols. Please see debug instructions for "
         "using //tools/lldb/lldbinit.py:\n"
         "https://chromium.googlesource.com/chromium/src/+/main/docs/"
         "lldbinit.md\n"
         "To continue anyway, type 'continue' in lldb. To always skip this "
         "check, define an environment variable CHROMIUM_LLDBINIT_SOURCED=1";
#endif
}

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_AIX)

// We can look in /proc/self/status for TracerPid.  We are likely used in crash
// handling, so we are careful not to use the heap or have side effects.
// Another option that is common is to try to ptrace yourself, but then we
// can't detach without forking(), and that's not so great.
// static
Process GetDebuggerProcess() {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return Process();

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf)));
  if (IGNORE_EINTR(close(status_fd)) < 0)
    return Process();

  if (num_read <= 0)
    return Process();

  std::string_view status(buf, static_cast<size_t>(num_read));
  std::string_view tracer("TracerPid:\t");

  std::string_view::size_type pid_index = status.find(tracer);
  if (pid_index == std::string_view::npos) {
    return Process();
  }
  pid_index += tracer.size();
  std::string_view::size_type pid_end_index = status.find('\n', pid_index);
  if (pid_end_index == std::string_view::npos) {
    return Process();
  }

  std::string_view pid_str(buf + pid_index, pid_end_index - pid_index);
  int pid = 0;
  if (!StringToInt(pid_str, &pid))
    return Process();

  return Process(pid);
}

bool BeingDebugged() {
  return GetDebuggerProcess().IsValid();
}

void VerifyDebugger() {
#if BUILDFLAG(ENABLE_GDBINIT_WARNING)
  // Quick check before potentially slower GetDebuggerProcess().
  if (Environment::Create()->HasVar("CHROMIUM_GDBINIT_SOURCED"))
    return;

  Process proc = GetDebuggerProcess();
  if (!proc.IsValid())
    return;

  FilePath cmdline_file =
      FilePath("/proc").Append(NumberToString(proc.Handle())).Append("cmdline");
  std::string cmdline;
  if (!ReadFileToString(cmdline_file, &cmdline))
    return;

  // /proc/*/cmdline separates arguments with null bytes, but we only care about
  // the executable name, so interpret |cmdline| as a null-terminated C string
  // to extract the exe portion.
  std::string_view exe(cmdline.c_str());

  DCHECK(ToLowerASCII(exe).find("gdb") == std::string::npos)
      << "Detected gdb without sourcing //tools/gdb/gdbinit.  gdb may not be "
         "able to find debug symbols, and pretty-printing of STL types may not "
         "work.  Please see debug instructions for using //tools/gdb/gdbinit:\n"
         "https://chromium.googlesource.com/chromium/src/+/main/docs/"
         "gdbinit.md\n"
         "To continue anyway, type 'continue' in gdb.  To always skip this "
         "check, define an environment variable CHROMIUM_GDBINIT_SOURCED=1";
#endif
}

#else

bool BeingDebugged() {
  NOTIMPLEMENTED();
  return false;
}

void VerifyDebugger() {}

#endif

// We want to break into the debugger in Debug mode, and cause a crash dump in
// Release mode. Breakpad behaves as follows:
//
// +-------+-----------------+-----------------+
// | OS    | Dump on SIGTRAP | Dump on SIGABRT |
// +-------+-----------------+-----------------+
// | Linux |       N         |        Y        |
// | Mac   |       Y         |        N        |
// +-------+-----------------+-----------------+
//
// Thus we do the following:
// Linux: Debug mode if a debugger is attached, send SIGTRAP; otherwise send
//        SIGABRT
// Mac: Always send SIGTRAP.

#if defined(ARCH_CPU_ARMEL)
#define DEBUG_BREAK_ASM() asm("bkpt 0")
#elif defined(ARCH_CPU_ARM64)
#define DEBUG_BREAK_ASM() asm("brk 0")
#elif defined(ARCH_CPU_MIPS_FAMILY)
#define DEBUG_BREAK_ASM() asm("break 2")
#elif defined(ARCH_CPU_X86_FAMILY)
#define DEBUG_BREAK_ASM() asm("int3")
#endif

#if defined(NDEBUG) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
#define DEBUG_BREAK() abort()
#elif BUILDFLAG(IS_NACL)
// The NaCl verifier doesn't let use use int3.  For now, we call abort().  We
// should ask for advice from some NaCl experts about the optimum thing here.
// http://code.google.com/p/nativeclient/issues/detail?id=645
#define DEBUG_BREAK() abort()
#elif !BUILDFLAG(IS_APPLE)
// Though Android has a "helpful" process called debuggerd to catch native
// signals on the general assumption that they are fatal errors. If no debugger
// is attached, we call abort since Breakpad needs SIGABRT to create a dump.
// When debugger is attached, for ARM platform the bkpt instruction appears
// to cause SIGBUS which is trapped by debuggerd, and we've had great
// difficulty continuing in a debugger once we stop from SIG triggered by native
// code, use GDB to set |go| to 1 to resume execution; for X86 platform, use
// "int3" to setup breakpiont and raise SIGTRAP.
//
// On other POSIX architectures, except Mac OS X, we use the same logic to
// ensure that breakpad creates a dump on crashes while it is still possible to
// use a debugger.
namespace {
void DebugBreak() {
  if (!BeingDebugged()) {
    abort();
  } else {
#if defined(DEBUG_BREAK_ASM)
    DEBUG_BREAK_ASM();
#else
    volatile int go = 0;
    while (!go)
      PlatformThread::Sleep(Milliseconds(100));
#endif
  }
}
}  // namespace
#define DEBUG_BREAK() DebugBreak()
#elif defined(DEBUG_BREAK_ASM)
#define DEBUG_BREAK() DEBUG_BREAK_ASM()
#else
#error "Don't know how to debug break on this architecture/OS"
#endif

void BreakDebuggerAsyncSafe() {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

  // Linker's ICF feature may merge this function with other functions with the
  // same definition (e.g. any function whose sole job is to call abort()) and
  // it may confuse the crash report processing system. http://crbug.com/508489
  static int static_variable_to_make_this_function_unique = 0;
  Alias(&static_variable_to_make_this_function_unique);

  DEBUG_BREAK();
#if BUILDFLAG(IS_ANDROID) && !defined(OFFICIAL_BUILD)
  // For Android development we always build release (debug builds are
  // unmanageably large), so the unofficial build is used for debugging. It is
  // helpful to be able to insert BreakDebugger() statements in the source,
  // attach the debugger, inspect the state of the program and then resume it by
  // setting the 'go' variable above.
#elif defined(NDEBUG)
  // Terminate the program after signaling the debug break.
  // When DEBUG_BREAK() expands to abort(), this is unreachable code. Rather
  // than carefully tracking in which cases DEBUG_BREAK()s is noreturn, just
  // disable the unreachable code warning here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
  _exit(1);
#pragma GCC diagnostic pop
#endif
}

}  // namespace debug
}  // namespace base
