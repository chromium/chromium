// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/atomic_flag.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "content/public/browser/browser_thread.h"

namespace memory {

namespace {

base::LazyInstance<MemoryKillsMonitor>::Leaky g_memory_kills_monitor_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

MemoryKillsMonitor::MemoryKillsMonitor() = default;

MemoryKillsMonitor::~MemoryKillsMonitor() {
  NOTREACHED();
}

// static
void MemoryKillsMonitor::Initialize() {
  VLOG(2) << "MemoryKillsMonitor::Initializing on "
          << base::PlatformThread::CurrentId();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* login_state = chromeos::LoginState::Get();
  if (login_state)
    login_state->AddObserver(g_memory_kills_monitor_instance.Pointer());
  else
    LOG(ERROR) << "LoginState is not initialized";
}

// static
void MemoryKillsMonitor::LogLowMemoryKill(
    const std::string& type, int estimated_freed_kb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_memory_kills_monitor_instance.Get().LogLowMemoryKillImpl(
      type, estimated_freed_kb);
}

void MemoryKillsMonitor::LoggedInStateChanged() {
  VLOG(2) << "LoggedInStateChanged";
  auto* login_state = chromeos::LoginState::Get();
  if (login_state) {
    // Note: LoginState never fires a notification when logged out.
    if (login_state->IsUserLoggedIn()) {
      VLOG(2) << "User logged in";
      StartMonitoring();
    }
  }
}

void MemoryKillsMonitor::StartMonitoring() {
  VLOG(2) << "Starting monitor from thread "
          << base::PlatformThread::CurrentId();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (monitoring_started_.IsSet()) {
    LOG(WARNING) << "Monitoring has been started";
    return;
  }

  // Insert a zero kill record at the begining of each login session for easy
  // comparison to those with non-zero kill sessions.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.OOMKills.Count", 0, 1, 1000, 1001);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.LowMemoryKiller.Count", 0, 1, 1000, 1001);

  monitoring_started_.Set();

  base::VmStatInfo vmstat;
  if (base::GetVmStatInfo(&vmstat)) {
    last_oom_kills_count_ = vmstat.oom_kill;
  } else {
    last_oom_kills_count_ = 0;
  }

  checking_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1),
                        base::BindRepeating(&MemoryKillsMonitor::CheckOOMKill,
                                            base::Unretained(this)));
}

void MemoryKillsMonitor::CheckOOMKill() {
  base::VmStatInfo vmstat;
  if (base::GetVmStatInfo(&vmstat)) {
    CheckOOMKillImpl(vmstat.oom_kill);
  }
}

void MemoryKillsMonitor::CheckOOMKillImpl(unsigned long current_oom_kills) {
  DCHECK(monitoring_started_.IsSet());

  unsigned long oom_kills_increased = current_oom_kills - last_oom_kills_count_;
  if (oom_kills_increased == 0)
    return;

  VLOG(1) << "OOM_KILLS " << oom_kills_increased << " times";

  for (int i = 0; i < oom_kills_increased; ++i) {
    ++oom_kills_count_;

    // Report the cumulative count of killed process in one login session. For
    // example if there are 3 processes killed, it would report 1 for the first
    // kill, 2 for the second kill, then 3 for the final kill.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.OOMKills.Count", oom_kills_count_, 1, 1000,
                                1001);
  }
  last_oom_kills_count_ = current_oom_kills;
}

void MemoryKillsMonitor::LogLowMemoryKillImpl(const std::string& type,
                                              int estimated_freed_kb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!monitoring_started_.IsSet()) {
    LOG(WARNING) << "LogLowMemoryKill before monitoring started, "
                    "skipped this log.";
    return;
  }

  VLOG(1) << "LOW_MEMORY_KILL_" << type;

  base::Time now = base::Time::Now();
  const base::TimeDelta time_delta = last_low_memory_kill_time_.is_null()
                                         ? kMaxMemoryKillTimeDelta
                                         : (now - last_low_memory_kill_time_);
  UMA_HISTOGRAM_MEMORY_KILL_TIME_INTERVAL("Arc.LowMemoryKiller.TimeDelta",
                                          time_delta);
  last_low_memory_kill_time_ = now;

  ++low_memory_kills_count_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.LowMemoryKiller.Count",
                              low_memory_kills_count_, 1, 1000, 1001);

  UMA_HISTOGRAM_MEMORY_KB("Arc.LowMemoryKiller.FreedSize", estimated_freed_kb);
}

MemoryKillsMonitor* MemoryKillsMonitor::GetForTesting() {
  return g_memory_kills_monitor_instance.Pointer();
}

}  // namespace memory
