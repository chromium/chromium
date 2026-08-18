// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shutdown_watchdog_mac.h"

#include <signal.h>
#include <unistd.h>

#include <atomic>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/shutdown_signal_handlers_posix.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

namespace shutdown_watchdog {
namespace {

// Emergency deadline once shutdown has been requested. Deliberately equal to
// kEndSessionTimeout in browser_process_impl.cc, the budget the codebase
// already grants end-of-session critical writes. Long enough for a normal
// graceful shutdown (typically <2s), short enough to record a dump and get
// out of the way of an OS-driven reboot or software update.
base::TimeDelta GetEmergencyTimeout() {
#if defined(NDEBUG) && !defined(ADDRESS_SANITIZER) &&           \
    !defined(MEMORY_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(LEAK_SANITIZER)
  return base::Seconds(10);
#else
  // Slow build configurations get headroom, mirroring ShutdownWatcherHelper.
  return base::Seconds(60);
#endif
}

bool WatchdogsEnabled() {
  // Tests (including Debug/ASan bots) must not be terminated early, and a
  // developer at a breakpoint mid-shutdown is not a hang.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kTestType) &&
         !base::debug::BeingDebugged();
}

// Returns the event on which the watchdogs wait. It is signaled late in
// browser shutdown (see OnShutdownComplete()) to stand the watchdogs down.
base::WaitableEvent& GetShutdownCompleteEvent() {
  // Manual-reset (default): `Signal()` releases all current and future
  // waiters.
  static base::NoDestructor<base::WaitableEvent> event;
  return *event;
}

// True once the SIGTERM watchdog has started in response to receiving
// SIGTERM.
std::atomic_bool g_sigterm_watchdog_armed{false};

class TearDownWatchdog : public base::PlatformThread::Delegate {
 public:
  void ThreadMain() override {
    base::PlatformThread::SetName("MacShutdownWatchdog");
    if (GetShutdownCompleteEvent().TimedWait(GetEmergencyTimeout())) {
      return;  // Shutdown reached its terminal phase; stand down.
    }
    if (base::debug::BeingDebugged()) {
      // A debugger attached after arming. This is a shutdown-hang
      // investigation in progress, not a hang to kill.
      return;
    }
    RAW_LOG(ERROR,
            "Teardown watchdog expired; recording dump and terminating.");
    base::debug::DumpWithoutCrashing();
    _exit(content::RESULT_CODE_HUNG);
  }
};

}  // namespace

void BlockOnSigtermShutdown() {
  if (!WatchdogsEnabled()) {
    return;  // Caller parks the thread (preserves the legacy marker frame).
  }
  g_sigterm_watchdog_armed.store(true, std::memory_order_relaxed);
  if (GetShutdownCompleteEvent().TimedWait(GetEmergencyTimeout())) {
    return;  // Shutdown completed; caller parks while the process exits.
  }
  if (base::debug::BeingDebugged()) {
    return;  // A debugger attached after arming. Caller parks the thread.
  }
  RAW_LOG(ERROR,
          "SIGTERM shutdown watchdog expired; recording dump and re-raising.");
  base::debug::DumpWithoutCrashing();
  ReraiseSignalAndExit(SIGTERM);  // Does not return.
}

void OnBrowserTearDownStarted() {
  if (!WatchdogsEnabled()) {
    return;
  }
  if (g_sigterm_watchdog_armed.load(std::memory_order_relaxed)) {
    // The SIGTERM watchdog started earlier with the same duration; it always
    // expires first. Arming here would only add a redundant thread and a
    // double-dump race between its dump and its `_exit`. Best-effort: the
    // flag is set after the exit task is posted, so an extremely fast UI
    // thread could still arm both, which reduces to the same benign
    // double-arm.
    return;
  }
  // PlatformThread does not delete its delegate; intentionally leaked.
  auto* watchdog = new TearDownWatchdog();
  ANNOTATE_LEAKING_OBJECT_PTR(watchdog);
  base::PlatformThread::CreateNonJoinable(0, watchdog);
}

void OnShutdownComplete() {
  GetShutdownCompleteEvent().Signal();
}

}  // namespace shutdown_watchdog
