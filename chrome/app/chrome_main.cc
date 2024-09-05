// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <iostream>
#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/app/startup_timestamps.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "headless/public/headless_shell.h"
#include "headless/public/switches.h"
#include "partition_alloc/buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/app/chrome_main_mac.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "base/base_switches.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/app/chrome_main_linux.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <timeapi.h>

#include "base/dcheck_is_on.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/handle_hooks_win.h"
#include "base/win/current_module.h"
#include "base/win/win_util.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_details.h"

#define DLLEXPORT __declspec(dllexport)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#define ENABLE_OLD_HEADLESS
#endif

#ifdef ENABLE_OLD_HEADLESS
namespace {
void ShowOldHeadlessWarningMaybe(const base::CommandLine* command_line) {
  // Show warning only if in browser process.
  if (!command_line->GetSwitchValueASCII(::switches::kProcessType).empty()) {
    return;
  }

  constexpr char kChromeDisableOldHeadlessWarning[] =
      "CHROME_DISABLE_OLD_HEADLESS_WARNING";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar(kChromeDisableOldHeadlessWarning)) {
    return;
  }

  std::cerr
      << "Old Headless mode will be removed from the Chrome binary soon. "
         "Please use the new Headless mode "
         "(https://developer.chrome.com/docs/chromium/new-headless) or the "
         "chrome-headless-shell which is a standalone implementation of "
         "the old Headless mode "
         "(https://developer.chrome.com/blog/chrome-headless-shell)."
      << std::endl
      << std::endl;
}
}  // namespace
#endif  // ENABLE_OLD_HEADLESS

#if BUILDFLAG(IS_WIN)
// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_main_entry_point_ticks,
                                 int64_t preread_begin_ticks,
                                 int64_t preread_end_ticks);
}
#elif BUILDFLAG(IS_POSIX)
extern "C" {
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR __attribute__((visibility("default"))) int ChromeMain(
    int argc,
    const char** argv);
}
#else
#error Unknown platform.
#endif

#if BUILDFLAG(IS_WIN)
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_entry_point_ticks,
                                 int64_t preread_begin_ticks,
                                 int64_t preread_end_ticks) {
#elif BUILDFLAG(IS_POSIX)
int ChromeMain(int argc, const char** argv) {
#else
#error Unknown platform.
#endif

#if BUILDFLAG(IS_LINUX)
  PossiblyDetermineFallbackChromeChannel(argv[0]);
#endif

#if BUILDFLAG(IS_WIN)
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
  StartupTimestamps timestamps{
      base::TimeTicks::FromInternalValue(exe_entry_point_ticks),
      base::TimeTicks::FromInternalValue(preread_begin_ticks),
      base::TimeTicks::FromInternalValue(preread_end_ticks)};
  ChromeMainDelegate chrome_main_delegate(timestamps);
#else  // BUILDFLAG(IS_WIN)
  ChromeMainDelegate chrome_main_delegate(
      {.exe_entry_point_ticks = base::TimeTicks::Now()});
#endif
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

#if BUILDFLAG(IS_LINUX)
  AppendExtraArgumentsToCommandLine(command_line);
#endif

  // PoissonAllocationSampler's TLS slots need to be set up before
  // MainThreadStackSamplingProfiler (in ChromeMainDelegate::ThreadPoolCreated),
  // which can allocate TLS slots of its own. On some platforms pthreads can
  // malloc internally to access higher-numbered TLS slots, which can cause
  // reentry in the heap profiler. (See the comment on
  // ReentryGuard::InitTLSSlot().)
  // TODO(crbug.com/40062835): Clean up other paths that call this Init()
  // function, which are now redundant.
  base::PoissonAllocationSampler::Init();

  // Chrome-specific process modes.
  std::unique_ptr<headless::HeadlessModeHandle> headless_mode_handle;
  if (headless::IsHeadlessMode()) {
    if (command_line->GetArgs().size() > 1) {
      LOG(ERROR) << "Multiple targets are not supported in headless mode.";
      return chrome::RESULT_CODE_UNSUPPORTED_PARAM;
    }
    headless_mode_handle = headless::InitHeadlessMode();
  } else {
#ifdef ENABLE_OLD_HEADLESS
    if (headless::IsOldHeadlessMode()) {
      ShowOldHeadlessWarningMaybe(command_line);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      command_line->AppendSwitch(::headless::switches::kEnableCrashReporter);
#endif
      return headless::HeadlessShellMain(std::move(params));
    }
#endif  // ENABLE_OLD_HEADLESS
  }

#if BUILDFLAG(IS_MAC)
  // Gracefully exit if the system tried to launch the macOS notification helper
  // app when a user clicked on a notification.
  if (IsAlertsHelperLaunchedViaNotificationAction()) {
    return 0;
  }
#endif

  int rv = content::ContentMain(std::move(params));

  if (chrome::IsNormalResultCode(static_cast<chrome::ResultCode>(rv)))
    return content::RESULT_CODE_NORMAL_EXIT;
  return rv;
}
