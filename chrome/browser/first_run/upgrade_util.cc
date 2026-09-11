// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/switch_utils.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#endif

namespace {

// A test seam for whole-browser tests to override browser relaunch.
upgrade_util::RelaunchChromeBrowserCallback*
    relaunch_chrome_browser_callback_for_testing = nullptr;

}  // namespace

namespace upgrade_util {

// Forward-declaration of the platform-specific implementation.
#if BUILDFLAG(IS_WIN)
bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line,
                               bool force_breakaway_from_job,
                               bool wait_for_parent);
#else
bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line);
#endif

base::CommandLine GetRelaunchCommandLine(
    const base::CommandLine& old_cl,
    browser_shutdown::RestartMode restart_mode) {
  base::CommandLine new_cl(old_cl.GetProgram());
  base::CommandLine::SwitchMap switches = old_cl.GetSwitches();

  // Remove switches that shouldn't persist across any restart.
  about_flags::RemoveFlagsSwitches(&switches);

  switch (restart_mode) {
    case browser_shutdown::RestartMode::kNoRestart:
      NOTREACHED();

    case browser_shutdown::RestartMode::kRestartInBackground:
      new_cl.AppendSwitch(switches::kNoStartupWindow);
#if BUILDFLAG(IS_WIN)
      new_cl.AppendArgNative(app_launch_prefetch::GetPrefetchSwitch(
          app_launch_prefetch::SubprocessType::kBrowserBackground));
#endif  // BUILDFLAG(IS_WIN)
      [[fallthrough]];

    case browser_shutdown::RestartMode::kRestartLastSession:
      // Relaunch the browser without any command line URLs or certain one-off
      // switches.
      switches::RemoveSwitchesForAutostart(&switches);
      break;

    case browser_shutdown::RestartMode::kRestartThisSession:
      // Copy URLs and other arguments to the new command line.
      if (const auto& old_args = old_cl.GetArgs(); !old_args.empty()) {
        new_cl.AppendArgNative(FILE_PATH_LITERAL("--"));
        for (const auto& arg : old_args) {
          new_cl.AppendArgNative(arg);
        }
      }
      break;
  }

  // Append the old switches to the new command line.
  for (const auto& it : switches) {
    new_cl.AppendSwitchNative(it.first, it.second);
  }

  if (restart_mode == browser_shutdown::RestartMode::kRestartLastSession ||
      restart_mode == browser_shutdown::RestartMode::kRestartThisSession) {
    new_cl.AppendSwitch(switches::kRestart);
  }

  return new_cl;
}

bool RelaunchChromeBrowser(const base::CommandLine& command_line,
                           bool force_breakaway_from_job,
                           bool wait_for_parent) {
  base::CommandLine relaunch_command_line = command_line;
#if BUILDFLAG(IS_WIN)
  if (!wait_for_parent) {
    // When not waiting for the parent handle, any stale handle switch from a
    // previous restart must be removed.
    relaunch_command_line.RemoveSwitch(switches::kWaitForParentHandle);
  }
#endif

  if (relaunch_chrome_browser_callback_for_testing) {
    return relaunch_chrome_browser_callback_for_testing->Run(
        relaunch_command_line);
  }

#if BUILDFLAG(IS_WIN)
  return RelaunchChromeBrowserImpl(relaunch_command_line,
                                   force_breakaway_from_job, wait_for_parent);
#else
  return RelaunchChromeBrowserImpl(relaunch_command_line);
#endif
}

RelaunchChromeBrowserCallback SetRelaunchChromeBrowserCallbackForTesting(
    RelaunchChromeBrowserCallback callback) {
  // Take ownership of the current test callback so it can be returned.
  RelaunchChromeBrowserCallback previous =
      relaunch_chrome_browser_callback_for_testing
          ? std::move(*relaunch_chrome_browser_callback_for_testing)
          : RelaunchChromeBrowserCallback();

  // Move the caller's callback into the global, alloc'ing or freeing as needed.
  auto memory = base::WrapUnique(relaunch_chrome_browser_callback_for_testing);
  if (callback) {
    if (!memory)
      memory = std::make_unique<RelaunchChromeBrowserCallback>();
    *memory = std::move(callback);
  } else if (memory) {
    memory.reset();
  }
  relaunch_chrome_browser_callback_for_testing = memory.release();

  // Return the previous callback.
  return previous;
}

}  // namespace upgrade_util
