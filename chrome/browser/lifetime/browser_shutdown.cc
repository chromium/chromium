// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/browser_shutdown.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/lifetime/switch_utils.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/crash/core/common/crash_key.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/win/browser_util.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/first_run/upgrade_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/termination_notification.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck crbug.com/1125897
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
#include "base/run_loop.h"
#include "content/public/browser/profiling_utils.h"
#endif

namespace browser_shutdown {
namespace {

// Whether the browser is trying to quit (e.g., Quit chosen from menu).
bool g_trying_to_quit = false;

base::Time* g_shutdown_started = nullptr;
ShutdownType g_shutdown_type = ShutdownType::kNotValid;
int g_shutdown_num_processes;
int g_shutdown_num_processes_slow;

const char* ToShutdownTypeString(ShutdownType type) {
  switch (type) {
    case ShutdownType::kNotValid:
      NOTREACHED_IN_MIGRATION();
      break;
    case ShutdownType::kWindowClose:
      return "close";
    case ShutdownType::kBrowserExit:
      return "exit";
    case ShutdownType::kEndSession:
      return "end";
    case ShutdownType::kSilentExit:
      return "silent_exit";
    case ShutdownType::kOtherExit:
      return "other_exit";
  }
  return "";
}

// Utility function to verify that globals are accessed on the UI/Main thread.
void CheckAccessedOnCorrectThread() {
  // Some APIs below are accessed after UI thread has been torn down, so cater
  // for both situations here.
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));
}

}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown, false);
}

void OnShutdownStarting(ShutdownType type) {
  CheckAccessedOnCorrectThread();
  if (g_shutdown_type != ShutdownType::kNotValid) {
    return;
  }

  static crash_reporter::CrashKeyString<11> shutdown_type_key("shutdown-type");
  shutdown_type_key.Set(ToShutdownTypeString(type));

  g_shutdown_type = type;
  // For now, we're only counting the number of renderer processes
  // since we can't safely count the number of plugin processes from this
  // thread, and we'd really like to avoid anything which might add further
  // delays to shutdown time.
  DCHECK(!g_shutdown_started);
  g_shutdown_started = new base::Time(base::Time::Now());

  // TODO(crbug.com/40685224): Check if this should also be enabled for
  // coverage builds.
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
  // Wait for all the child processes to dump their profiling data without
  // blocking the main thread.
  base::RunLoop nested_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::AskAllChildrenToDumpProfilingData(nested_run_loop.QuitClosure());
  nested_run_loop.Run();
#endif  // BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)

  // Call FastShutdown on all of the RenderProcessHosts.  This will be
  // a no-op in some cases, so we still need to go through the normal
  // shutdown path for the ones that didn't exit here.
  if (g_browser_process) {
    g_shutdown_num_processes = 0;
    g_shutdown_num_processes_slow = 0;
    for (content::RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      ++g_shutdown_num_processes;
      if (!i.GetCurrentValue()->FastShutdownIfPossible()) {
        ++g_shutdown_num_processes_slow;
      }
    }
  }
}

bool HasShutdownStarted() {
  CheckAccessedOnCorrectThread();
  return g_shutdown_type != ShutdownType::kNotValid;
}

bool ShouldIgnoreUnloadHandlers() {
  CheckAccessedOnCorrectThread();
  return g_shutdown_type == ShutdownType::kEndSession ||
         g_shutdown_type == ShutdownType::kSilentExit;
}

ShutdownType GetShutdownType() {
  CheckAccessedOnCorrectThread();
  return g_shutdown_type;
}

#if !BUILDFLAG(IS_ANDROID)
bool ShutdownPreThreadsStop() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::BootTimesRecorder::Get()->AddLogoutTimeMarker("BrowserShutdownStarted",
                                                     false);
#endif

  // WARNING: During logoff/shutdown (WM_ENDSESSION) we may not have enough
  // time to get here. If you have something that *must* happen on end session,
  // consider putting it in BrowserProcessImpl::EndSession.
  metrics::MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics) {
    // TODO(crbug.com/40849295): LogCleanShutdown() is called earlier on in
    // shutdown. See whether this call can be removed.
    metrics->LogCleanShutdown();
  }

  bool restart_last_session = RecordShutdownInfoPrefs();
  g_browser_process->local_state()->CommitPendingWrite();

#if BUILDFLAG(ENABLE_RLZ)
  rlz::RLZTracker::CleanupRlz();
#endif

  return restart_last_session;
}

void RecordShutdownMetrics() {
  base::UmaHistogramEnumeration("Shutdown.ShutdownType2", g_shutdown_type);

  const char* time_metric_name = nullptr;
  switch (g_shutdown_type) {
    case ShutdownType::kNotValid:
      time_metric_name = "Shutdown.NotValid.Time2";
      break;

    case ShutdownType::kSilentExit:
      time_metric_name = "Shutdown.SilentExit.Time2";
      break;

    case ShutdownType::kWindowClose:
      time_metric_name = "Shutdown.WindowClose.Time2";
      break;

    case ShutdownType::kBrowserExit:
      time_metric_name = "Shutdown.BrowserExit.Time2";
      break;

    case ShutdownType::kEndSession:
      time_metric_name = "Shutdown.EndSession.Time2";
      break;

    case ShutdownType::kOtherExit:
      time_metric_name = "Shutdown.OtherExit.Time2";
      break;
  }
  DCHECK(time_metric_name);

  if (g_shutdown_started) {
    base::TimeDelta shutdown_delta = base::Time::Now() - *g_shutdown_started;
    base::UmaHistogramMediumTimes(time_metric_name, shutdown_delta);
  }

  base::UmaHistogramCounts100("Shutdown.Renderers.Total2",
                              g_shutdown_num_processes);
  base::UmaHistogramCounts100("Shutdown.Renderers.Slow2",
                              g_shutdown_num_processes_slow);
}

bool RecordShutdownInfoPrefs() {
  CheckAccessedOnCorrectThread();
  PrefService* prefs = g_browser_process->local_state();

  // Check local state for the restart flag so we can restart the session later.
  bool restart_last_session = false;
  if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown)) {
    restart_last_session =
        prefs->GetBoolean(prefs::kRestartLastSessionOnShutdown);
    prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
  }
  return restart_last_session;
}

void ShutdownPostThreadsStop(RestartMode restart_mode) {
  CheckAccessedOnCorrectThread();
  // At this point, no BrowserProcess instance should exist.
  CHECK(!g_browser_process);

  // crbug.com/95079 - This needs to happen after the browser process object
  // goes away.
  NukeDeletedProfilesFromDisk();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::BootTimesRecorder::Get()->AddLogoutTimeMarker("BrowserDeleted",
                                                     /*send_to_uma=*/false);
#endif

#if BUILDFLAG(IS_WIN)
  if (!browser_util::IsBrowserAlreadyRunning() &&
      g_shutdown_type != ShutdownType::kEndSession) {
    upgrade_util::SwapNewChromeExeIfPresent();
  }
#endif

  if (restart_mode != RestartMode::kNoRestart) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTIMPLEMENTED();
#else
    const base::CommandLine& old_cl(*base::CommandLine::ForCurrentProcess());
    base::CommandLine new_cl(old_cl.GetProgram());
    base::CommandLine::SwitchMap switches = old_cl.GetSwitches();

    // Remove switches that shouldn't persist across any restart.
    about_flags::RemoveFlagsSwitches(&switches);

    switch (restart_mode) {
      case RestartMode::kNoRestart:
        NOTREACHED_IN_MIGRATION();
        break;

      case RestartMode::kRestartInBackground:
        new_cl.AppendSwitch(switches::kNoStartupWindow);
        [[fallthrough]];

      case RestartMode::kRestartLastSession:
        // Relaunch the browser without any command line URLs or certain one-off
        // switches.
        switches::RemoveSwitchesForAutostart(&switches);
        break;

      case RestartMode::kRestartThisSession:
        // Copy URLs and other arguments to the new command line.
        for (const auto& arg : old_cl.GetArgs()) {
          new_cl.AppendArgNative(arg);
        }
        break;
    }

    // Append the old switches to the new command line.
    for (const auto& it : switches) {
      new_cl.AppendSwitchNative(it.first, it.second);
    }

    if (restart_mode == RestartMode::kRestartLastSession ||
        restart_mode == RestartMode::kRestartThisSession) {
      new_cl.AppendSwitch(switches::kRestart);
    }
    upgrade_util::RelaunchChromeBrowser(new_cl);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::StopSession();
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

void SetTryingToQuit(bool quitting) {
  CheckAccessedOnCorrectThread();
  g_trying_to_quit = quitting;

  if (quitting) {
    return;
  }

  // Reset the restart-related preferences. They get set unconditionally through
  // calls such as chrome::AttemptRestart(), and need to be reset if the restart
  // attempt is cancelled.
  PrefService* pref_service = g_browser_process->local_state();
  if (pref_service) {
#if !BUILDFLAG(IS_ANDROID)
    pref_service->ClearPref(prefs::kWasRestarted);
#endif  // !BUILDFLAG(IS_ANDROID)
    pref_service->ClearPref(prefs::kRestartLastSessionOnShutdown);
  }

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager::set_should_restart_in_background(false);
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
}

bool IsTryingToQuit() {
  CheckAccessedOnCorrectThread();
  return g_trying_to_quit;
}

base::AutoReset<ShutdownType> SetShutdownTypeForTesting(
    ShutdownType shutdown_type) {
  CheckAccessedOnCorrectThread();
  return base::AutoReset<ShutdownType>(&g_shutdown_type, shutdown_type);
}

void ResetShutdownGlobalsForTesting() {
  CheckAccessedOnCorrectThread();
  if (g_shutdown_started) {
    delete g_shutdown_started;
    g_shutdown_started = nullptr;
  }

  g_trying_to_quit = false;
  g_shutdown_type = ShutdownType::kNotValid;
}

}  // namespace browser_shutdown
