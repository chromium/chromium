// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/public/headless_shell.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/app/chrome_main_mac.h"
#include "chrome/app/notification_metrics.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "base/base_switches.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/allocator/buildflags.h"
#include "base/dcheck_is_on.h"
#include "base/debug/handle_hooks_win.h"
#include "base/win/current_module.h"
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#endif

#include <timeapi.h>

#include "base/debug/dump_without_crashing.h"
#include "base/win/win_util.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_details.h"

#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_entry_point_ticks);
}
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
extern "C" {
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
__attribute__((visibility("default"))) int NO_STACK_PROTECTOR
ChromeMain(int argc, const char** argv);
}
#else
#error Unknown platform.
#endif

// On linux ChromeMain is the process entry point, whereas on macOS the main
// function is in a different binary in chrome_exe_main_mac.cc. When we get to
// ChromeMain we've already started recording/replaying, but still need to
// initialize V8's record/replay bindings. On linux we need to start
// recording/replaying, so have a bunch of code here duplicated from
// chrome_exec_main_mac.cc.
#ifdef OS_LINUX

static void (*gRecordReplayAttach)(const char* dispatchAddress, const char* buildId);
static void (*gRecordReplaySetApiKey)(const char* apiKey);
static void (*gRecordReplayProfileExecution)(const char* path);
static void (*gRecordReplayRecordCommandLineArguments)(int*, char***);
static void (*gRecordReplaySaveRecording)(const char* dir);

template <typename Src, typename Dst>
static inline void CastPointer(const Src src, Dst* dst) {
  static_assert(sizeof(Src) == sizeof(uintptr_t), "bad size");
  static_assert(sizeof(Dst) == sizeof(uintptr_t), "bad size");
  memcpy((void*)dst, (const void*)&src, sizeof(uintptr_t));
}

template <typename T>
static void RecordReplayLoadSymbol(void* handle, const char* name, T& function) {
  void* sym = dlsym(handle, name);
  if (!sym) {
    fprintf(stderr, "Could not find %s in Record Replay driver.\n", name);
    return;
  }

  CastPointer(sym, &function);
}

namespace recordreplay {
  extern char gBuildId[];
  extern char gRecordReplayDriver[];
  extern int gRecordReplayDriverSize;
}

static void* OpenDriverHandle() {
  const char* driver = getenv("RECORD_REPLAY_DRIVER");
  bool temporaryDriver = false;

  if (!driver) {
    const char* tmpdir = getenv("TMPDIR");
    if (!tmpdir) {
      tmpdir = "/tmp";
    }

    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/recordreplay.so-XXXXXX", tmpdir);
    int fd = mkstemp(filename);
    if (fd < 0) {
      fprintf(stderr, "mkstemp failed, can't create driver.\n");
      return nullptr;
    }

    int nbytes = write(fd, recordreplay::gRecordReplayDriver, recordreplay::gRecordReplayDriverSize);
    if (nbytes != recordreplay::gRecordReplayDriverSize) {
      fprintf(stderr, "write to driver temporary file failed, can't create driver.\n");
      return nullptr;
    }

    temporaryDriver = true;
    driver = strdup(filename);
    close(fd);
  }

  void* handle = dlopen(driver, RTLD_LAZY);

  if (temporaryDriver) {
    unlink(driver);
  }

  return handle;
}

#endif // OS_LINUX

extern "C" void V8SetRecordingOrReplaying(void* handle);

static void MaybeStartProfiling() {
  const char* directory = getenv("RECORD_REPLAY_PROFILE_DIRECTORY");
  if (!directory) {
    return;
  }

  char path[1000];
  snprintf(path, sizeof(path), "%s/profile-%d.log", directory, rand());

  gRecordReplayProfileExecution(path);
}

static void RecordReplayAttach(int* pargc, const char*** pargv) {
  // Figure out what type of process this is.
  const char* type = nullptr;
  for (int i = 0; i < *pargc; i++) {
    if (!strncmp((*pargv)[i], "--type=", 7)) {
      type = (*pargv)[i] + 7;
      break;
    }
  }
  if (type) {
    // Only renderer processes are recorded/replayed.
    if (strcmp(type, "renderer")) {
      return;
    }
  } else {
#ifdef OS_LINUX

    // If there is no type, this is the main process. Add a couple command line
    // arguments which are required to record/replay.
    const char** nargv = new const char*[*pargc + 3];
    memcpy(nargv, *pargv, *pargc * sizeof(char*));
    *pargv = nargv;

    // Recording processes currently need the sandbox disabled in order to
    // write out recording IDs to the specified path name.
    (*pargv)[*pargc] = strdup("--no-sandbox");

    // Recording/replaying currently requires software rendering.
    (*pargv)[*pargc + 1] = strdup("--disable-gpu");

    (*pargv)[*pargc + 2] = nullptr;
    *pargc += 2;

#endif // OS_LINUX
    return;
  }

  // When RECORD_REPLAY_DONT_RECORD we don't record, though the main browser
  // process will still be configured as if we are recording, see above.
  if (getenv("RECORD_REPLAY_DONT_RECORD")) {
    return;
  }

#ifdef OS_LINUX

  base::Optional<std::string> apiKey;
  const char* val = getenv("RECORD_REPLAY_API_KEY");
  if (val) {
    apiKey.emplace(val);
    // Unsetting the env var will make the variable unavailable via
    // getenv and such, and also mutates the 'environ' global, so
    // by the time gRecordReplayAttach runs, it will have no idea that
    // this value existed and won't capture it in the recording itself,
    // which is ideal for security.
    CHECK(!unsetenv("RECORD_REPLAY_API_KEY"));
  }

  void* handle = OpenDriverHandle();
  if (!handle) {
    const char* error = dlerror();
    fprintf(stderr, "Loading Record Replay driver failed: %s\n",
            error ? error : "<no error>");
    return;
  }

  RecordReplayLoadSymbol(handle, "RecordReplayAttach", gRecordReplayAttach);
  RecordReplayLoadSymbol(handle, "RecordReplaySetApiKey", gRecordReplaySetApiKey);
  RecordReplayLoadSymbol(handle, "RecordReplayProfileExecution", gRecordReplayProfileExecution);
  RecordReplayLoadSymbol(handle, "RecordReplaySaveRecording", gRecordReplaySaveRecording);
  RecordReplayLoadSymbol(handle, "RecordReplayRecordCommandLineArguments",
                         gRecordReplayRecordCommandLineArguments);

  if (apiKey) {
    gRecordReplaySetApiKey(apiKey->c_str());
  }

  const char* dispatchAddress = getenv("RECORD_REPLAY_SERVER");

  gRecordReplayAttach(dispatchAddress, recordreplay::gBuildId);
  gRecordReplayRecordCommandLineArguments(pargc, (char***)pargv);
  gRecordReplaySaveRecording(nullptr);

#else // !OS_LINUX

  // Loading the driver handle when it wasn't explicitly specified on macOS is NYI.
  CHECK(0);
  void* handle = nullptr;

#endif // !OS_LINUX

  V8SetRecordingOrReplaying(handle);

  MaybeStartProfiling();
}

#if BUILDFLAG(IS_WIN)
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_entry_point_ticks) {
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
int ChromeMain(int argc, const char** argv) {
  int64_t exe_entry_point_ticks = 0;
#else
#error Unknown platform.
#endif

  RecordReplayAttach(&argc, &argv);

#if BUILDFLAG(IS_WIN)
#if BUILDFLAG(USE_ALLOCATOR_SHIM) && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Call this early on in order to configure heap workarounds. This must be
  // called from chrome.dll. This may be a NOP on some platforms.
  allocator_shim::ConfigurePartitionAlloc();
#endif

  install_static::InitializeFromPrimaryModule();
#if !defined(COMPONENT_BUILD) && DCHECK_IS_ON()
  // Patch the main EXE on non-component builds when DCHECKs are enabled.
  // This allows detection of third party code that might attempt to meddle with
  // Chrome's handles. This must be done when single-threaded to avoid other
  // threads attempting to make calls through the hooks while they are being
  // emplaced.
  // Note: The EXE is patched separately, in chrome/app/chrome_exe_main_win.cc.
  base::debug::HandleHooks::AddIATPatch(CURRENT_MODULE());
#endif  // !defined(COMPONENT_BUILD) && DCHECK_IS_ON()
#endif  // BUILDFLAG(IS_WIN)

  ChromeMainDelegate chrome_main_delegate(
      base::TimeTicks::FromInternalValue(exe_entry_point_ticks));
  content::ContentMainParams params(&chrome_main_delegate);

#if BUILDFLAG(IS_WIN)
  // The process should crash when going through abnormal termination, but we
  // must be sure to reset this setting when ChromeMain returns normally.
  auto crash_on_detach_resetter = base::ScopedClosureRunner(
      base::BindOnce(&base::win::SetShouldCrashOnProcessDetach,
                     base::win::ShouldCrashOnProcessDetach()));
  base::win::SetShouldCrashOnProcessDetach(true);
  base::win::SetAbortBehaviorForCrashReporting();
  params.instance = instance;
  params.sandbox_info = sandbox_info;

  // Pass chrome_elf's copy of DumpProcessWithoutCrash resolved via load-time
  // dynamic linking.
  base::debug::SetDumpWithoutCrashingFunction(&DumpProcessWithoutCrash);

  // Verify that chrome_elf and this module (chrome.dll and chrome_child.dll)
  // have the same version.
  if (install_static::InstallDetails::Get().VersionMismatch())
    base::debug::DumpWithoutCrashing();
#else
  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);
#endif  // BUILDFLAG(IS_WIN)
  base::CommandLine::Init(0, nullptr);
  [[maybe_unused]] base::CommandLine* command_line(
      base::CommandLine::ForCurrentProcess());

#if BUILDFLAG(IS_WIN)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kRaiseTimerFrequency)) {
    // Raise the timer interrupt frequency and leave it raised.
    timeBeginPeriod(1);
  }
#endif

#if BUILDFLAG(IS_MAC)
  SetUpBundleOverrides();
#endif

  // Start the sampling profiler as early as possible - namely, once the command
  // line data is available. Allocated as an object on the stack to ensure that
  // the destructor runs on shutdown, which is important to avoid the profiler
  // thread's destruction racing with main thread destruction.
  MainThreadStackSamplingProfiler scoped_sampling_profiler;

  // Chrome-specific process modes.
  if (headless::IsChromeNativeHeadless()) {
    headless::SetUpCommandLine(command_line);
  } else {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    if (command_line->HasSwitch(switches::kHeadless)) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      command_line->AppendSwitch(::headless::switches::kEnableCrashReporter);
#endif
      return headless::HeadlessShellMain(std::move(params));
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_MAC)
  // Gracefully exit if the system tried to launch the macOS notification helper
  // app when a user clicked on a notification.
  if (IsAlertsHelperLaunchedViaNotificationAction()) {
    LogLaunchedViaNotificationAction(NotificationActionSource::kHelperApp);
    return 0;
  }
#endif

  int rv = content::ContentMain(std::move(params));

  if (chrome::IsNormalResultCode(static_cast<chrome::ResultCode>(rv)))
    return content::RESULT_CODE_NORMAL_EXIT;
  return rv;
}
