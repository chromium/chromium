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
#include "content/public/app/content_main.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/dcheck_is_on.h"
#include "base/debug/handle_hooks_win.h"
#include "base/win/current_module.h"
#include "chrome/app/startup_timestamps.h"
#include "chrome/install_static/initialize_from_primary_module.h"
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
  params.instance = instance;
  params.sandbox_info = sandbox_info;
  base::PoissonAllocationSampler::Init();
  return content::ContentMain(std::move(params));
}
#elif BUILDFLAG(IS_POSIX)
[[gnu::visibility("default")]] int ChromeRendererMain(int argc,
                                                      const char** argv) {
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
