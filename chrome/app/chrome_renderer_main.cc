// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/app/llvm_profile_util.h"
#include "content/public/app/content_main.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <timeapi.h>

#include "base/dcheck_is_on.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/handle_hooks_win.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/win/current_module.h"
#include "base/win/win_util.h"
#include "chrome/app/startup_timestamps.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_details.h"
#include "content/public/common/content_switches.h"
#define DLLEXPORT __declspec(dllexport)
#endif

extern "C" {

#if BUILDFLAG(IS_WIN)
DLLEXPORT int __cdecl ChromeRendererMain(
    HINSTANCE instance,
    sandbox::SandboxInterfaceInfo* sandbox_info,
    int64_t exe_entry_point_ticks,
    int64_t preread_begin_ticks,
    int64_t preread_end_ticks) {
  SetLLVMProfileProcessType(ProfileProcessType::kRenderer);
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
      base::TimeTicks() + base::Microseconds(exe_entry_point_ticks),
      base::TimeTicks() + base::Microseconds(preread_begin_ticks),
      base::TimeTicks() + base::Microseconds(preread_end_ticks)};
  ChromeMainDelegate chrome_main_delegate(timestamps);
  content::ContentMainParams params(&chrome_main_delegate);

  // The process should crash when going through abnormal termination, but we
  // must be sure to reset this setting when ChromeRendererMain returns
  // normally.
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

  // Verify that chrome_elf and this module (chrome_renderer.dll) have the same
  // version.
  if (install_static::InstallDetails::Get().VersionMismatch()) {
    base::debug::DumpWithoutCrashing();
  }
  base::CommandLine::Init(0, nullptr);
  base::PoissonAllocationSampler::Init();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kRaiseTimerFrequency)) {
    // Raise the timer interrupt frequency and leave it raised.
    timeBeginPeriod(1);
  }

  return content::ContentMain(std::move(params));
}
#elif BUILDFLAG(IS_POSIX)
[[gnu::visibility("default")]] int ChromeRendererMain(int argc,
                                                      const char** argv) {
  SetLLVMProfileProcessType(ProfileProcessType::kRenderer);
  ChromeMainDelegate chrome_main_delegate(
      {.exe_entry_point_ticks = base::TimeTicks::Now()});
  content::ContentMainParams params(&chrome_main_delegate);
  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);
  base::PoissonAllocationSampler::Init();
  return content::ContentMain(std::move(params));
}
#endif

}  // extern "C"

#if BUILDFLAG(IS_POSIX)
// TODO(crbug.com/534570563): Do not define `main` once this becomes a
// shared_library on macOS.
int main(int argc, const char** argv) {
  return ChromeRendererMain(argc, argv);
}
#endif
