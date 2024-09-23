// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "chrome/browser/memory/oom_kills_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace memory {

namespace {

base::LazyInstance<MemoryKillsMonitor>::Leaky g_memory_kills_monitor_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

MemoryKillsMonitor::MemoryKillsMonitor() = default;

MemoryKillsMonitor::~MemoryKillsMonitor() {
  NOTREACHED_IN_MIGRATION();
}

// static
void MemoryKillsMonitor::Initialize() {
  VLOG(2) << "MemoryKillsMonitor::Initializing on "
          << base::PlatformThread::CurrentId();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* login_state = ash::LoginState::Get();
  if (login_state)
    login_state->AddObserver(g_memory_kills_monitor_instance.Pointer());
  else
    LOG(ERROR) << "LoginState is not initialized";
}

// static
void MemoryKillsMonitor::LogLowMemoryKill(const std::string& type,
                                          int estimated_freed_kb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_memory_kills_monitor_instance.Get().LogLowMemoryKillImpl(
      type, estimated_freed_kb);
}

void MemoryKillsMonitor::LoggedInStateChanged() {
  VLOG(2) << "LoggedInStateChanged";
  auto* login_state = ash::LoginState::Get();
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
  base::UmaHistogramCustomCounts("Memory.LowMemoryKiller.Count", 0, 1, 1000,
                                 1001);

  monitoring_started_.Set();

  // Starts the OOM kills monitor.
  if (g_browser_process != nullptr &&
      g_browser_process->local_state() != nullptr) {
    OOMKillsMonitor::GetInstance().Initialize(g_browser_process->local_state());
  }
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

  ++low_memory_kills_count_;
  base::UmaHistogramCustomCounts("Memory.LowMemoryKiller.Count",
                                 low_memory_kills_count_, 1, 1000, 1001);

  base::UmaHistogramMemoryKB("Memory.LowMemoryKiller.FreedSize",
                             estimated_freed_kb);
}

MemoryKillsMonitor* MemoryKillsMonitor::GetForTesting() {
  return g_memory_kills_monitor_instance.Pointer();
}

}  // namespace memory
