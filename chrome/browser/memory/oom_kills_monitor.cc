// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_kills_monitor.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/process/process_metrics.h"
#include "content/public/browser/browser_thread.h"

namespace memory {

namespace {

void UmaHistogramOOMKills(int oom_kills) {
  base::UmaHistogramCustomCounts("Memory.OOMKills.Count", oom_kills, 1, 1000,
                                 1001);
}

}  // namespace

OOMKillsMonitor::OOMKillsMonitor() = default;

OOMKillsMonitor::~OOMKillsMonitor() = default;

// static
OOMKillsMonitor& OOMKillsMonitor::GetInstance() {
  static base::NoDestructor<OOMKillsMonitor> instance;
  return *instance;
}

void OOMKillsMonitor::Initialize() {
  VLOG(2) << "Starting OOM kills monitor from thread "
          << base::PlatformThread::CurrentId();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (monitoring_started_) {
    NOTREACHED() << "OOM kiils monitor should only be initialized once";
    return;
  }

  monitoring_started_ = true;

  // Insert a zero kill record at the beginning for easy comparison to those
  // with non-zero kill sessions.
  UmaHistogramOOMKills(0);

  base::VmStatInfo vmstat;
  if (base::GetVmStatInfo(&vmstat)) {
    last_oom_kills_count_ = vmstat.oom_kill;
  } else {
    last_oom_kills_count_ = 0;
  }

  checking_timer_.Start(FROM_HERE, base::Minutes(1),
                        base::BindRepeating(&OOMKillsMonitor::CheckOOMKill,
                                            base::Unretained(this)));
}

// Both host and guest(ARCVM) oom kills are logged to the same histogram
// Memory.OOMKills.Count.
//
// TODO(vovoy): Log ARCVM oom kills to a new histogram.
void OOMKillsMonitor::LogArcOOMKill(unsigned long current_oom_kills) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(monitoring_started_);

  unsigned long oom_kills_delta = current_oom_kills - last_arc_oom_kills_count_;
  if (oom_kills_delta == 0)
    return;

  VLOG(1) << "ARC_OOM_KILLS " << oom_kills_delta << " times";

  ReportOOMKills(oom_kills_delta);

  last_arc_oom_kills_count_ = current_oom_kills;
}

void OOMKillsMonitor::CheckOOMKill() {
  base::VmStatInfo vmstat;
  if (base::GetVmStatInfo(&vmstat)) {
    CheckOOMKillImpl(vmstat.oom_kill);
  }
}

void OOMKillsMonitor::CheckOOMKillImpl(unsigned long current_oom_kills) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(monitoring_started_);

  unsigned long oom_kills_delta = current_oom_kills - last_oom_kills_count_;
  if (oom_kills_delta == 0)
    return;

  VLOG(1) << "OOM_KILLS " << oom_kills_delta << " times";

  ReportOOMKills(oom_kills_delta);

  last_oom_kills_count_ = current_oom_kills;
}

void OOMKillsMonitor::ReportOOMKills(unsigned long oom_kills_delta) {
  for (size_t i = 0; i < oom_kills_delta; ++i) {
    ++oom_kills_count_;

    // Report the cumulative count of killed process. For example if there are
    // 3 processes killed, it would report 1 for the first kill, 2 for the
    // second kill, then 3 for the final kill.
    UmaHistogramOOMKills(oom_kills_count_);
  }
}

}  // namespace memory
