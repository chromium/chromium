// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>

#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/safe_strerror.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/re2/src/re2/re2.h"

namespace memory {

using base::TimeDelta;

namespace {

base::LazyInstance<MemoryKillsMonitor>::Leaky g_memory_kills_monitor_instance =
    LAZY_INSTANCE_INITIALIZER;

int64_t GetTimestamp(const std::string& line) {
  std::vector<std::string> fields = base::SplitString(
      line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  int64_t timestamp = -1;
  // Timestamp is the third field in a line of /dev/kmsg.
  if (fields.size() < 3 || !base::StringToInt64(fields[2], &timestamp))
    return -1;
  return timestamp;
}

void LogEvent(const base::Time& time_stamp, const std::string& event) {
  VLOG(1) << time_stamp.ToJavaTime() << ", " << event;
}

}  // namespace

MemoryKillsMonitor::Handle::Handle(MemoryKillsMonitor* outer) : outer_(outer) {
  DCHECK(outer_);
}

MemoryKillsMonitor::Handle::~Handle() {
  if (outer_) {
    VLOG(2) << "Chrome is shutting down" << outer_;
    outer_->is_shutting_down_.Set();
  }
}

MemoryKillsMonitor::MemoryKillsMonitor()
    : low_memory_kills_count_(0),
      last_oom_kill_time_(-1),
      oom_kills_count_(0) {}

MemoryKillsMonitor::~MemoryKillsMonitor() {
  // The instance has to be leaked on shutdown as it is referred to by a
  // non-joinable thread but ~MemoryKillsMonitor() can't be explicitly deleted
  // as it overrides ~SimpleThread(), it should nevertheless never be invoked.
  NOTREACHED();
}

// static
std::unique_ptr<MemoryKillsMonitor::Handle> MemoryKillsMonitor::Initialize() {
  VLOG(2) << "MemoryKillsMonitor::Initializing on "
          << base::PlatformThread::CurrentId();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* login_state = chromeos::LoginState::Get();
  if (login_state)
    login_state->AddObserver(g_memory_kills_monitor_instance.Pointer());
  else
    LOG(ERROR) << "LoginState is not initialized";

  // The MemoryKillsMonitor::Handle will notify the MemoryKillsMonitor
  // when it is destroyed so that the underlying thread can at a minimum not
  // do extra work during shutdown.
  return std::make_unique<Handle>(g_memory_kills_monitor_instance.Pointer());
}

// static
void MemoryKillsMonitor::LogLowMemoryKill(
    const std::string& type, int estimated_freed_kb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_memory_kills_monitor_instance.Get().LogLowMemoryKillImpl(
      type, estimated_freed_kb);
}

// static
void MemoryKillsMonitor::TryMatchOomKillLine(const std::string& line) {
  // Sample OOM log line:
  // 3,1362,97646497541,-;Out of memory: Kill process 29582 (android.vending)
  // score 961 or sacrifice child.
  int oom_badness;

  // Precompile the regex object since the pattern is constant.
  static const LazyRE2 kOomKillPattern = {
      R"(Out of memory: Kill process .* score (\d+))"};
  if (RE2::PartialMatch(line, *kOomKillPattern, &oom_badness)) {
    int64_t time_stamp = GetTimestamp(line);
    g_memory_kills_monitor_instance.Get().LogOOMKill(time_stamp, oom_badness);
  }
}

// TODO(cylee): Consider adding a unit test for this fuction.
void MemoryKillsMonitor::Run() {
  VLOG(2) << "Started monitoring OOM kills on thread "
          << base::PlatformThread::CurrentId();

  std::ifstream kmsg_stream("/dev/kmsg", std::ifstream::in);
  if (kmsg_stream.fail()) {
    LOG(WARNING) << "Open /dev/kmsg failed: " << base::safe_strerror(errno);
    return;
  }
  // Skip kernel messages prior to the instantiation of this object to avoid
  // double reporting.
  // Note: there's a small gap between login the fseek here, and events in that
  // period will not be recorded.
  kmsg_stream.seekg(0, std::ios_base::end);

  std::string line;
  while (std::getline(kmsg_stream, line)) {
    if (is_shutting_down_.IsSet()) {
      // Not guaranteed to execute when the process is shutting down,
      // because the thread might be blocked in fgets().
      VLOG(1) << "Chrome is shutting down, MemoryKillsMonitor exits.";
      break;
    }
    TryMatchOomKillLine(line);
  }
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

  base::SimpleThread::Options non_joinable_options;
  non_joinable_options.joinable = false;
  non_joinable_worker_thread_ = std::make_unique<base::DelegateSimpleThread>(
      this, "memory_kills_monitor", non_joinable_options);
  non_joinable_worker_thread_->StartAsync();
  monitoring_started_.Set();
}

void MemoryKillsMonitor::LogLowMemoryKillImpl(const std::string& type,
                                              int estimated_freed_kb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!monitoring_started_.IsSet()) {
    LOG(WARNING) << "LogLowMemoryKill before monitoring started, "
                    "skipped this log.";
    return;
  }

  base::Time now = base::Time::Now();
  LogEvent(now, "LOW_MEMORY_KILL_" + type);

  const TimeDelta time_delta = last_low_memory_kill_time_.is_null()
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

void MemoryKillsMonitor::LogOOMKill(int64_t time_stamp, int oom_badness) {
  if (!monitoring_started_.IsSet()) {
    LOG(WARNING) << "LogOOMKill before monitoring started, "
                    "skipped this log.";
    return;
  }

  // Ideally the timestamp should be parsed from /dev/kmsg, but the timestamp
  // there is the elapsed time since system boot. So the timestamp |now| used
  // here is a bit delayed.
  base::Time now = base::Time::Now();
  LogEvent(now, "OOM_KILL");

  ++oom_kills_count_;
  // Report the cumulative count of killed process in one login session.
  // For example if there are 3 processes killed, it would report 1 for the
  // first kill, 2 for the second kill, then 3 for the final kill.
  // It doesn't report a final count at the end of a user session because
  // the code runs in a dedicated thread and never ends until browser shutdown
  // (or logout on Chrome OS). And on browser shutdown the thread may be
  // terminated brutally so there's no chance to execute a "final" block.
  // More specifically, code outside the main loop of MemoryKillsMonitor::Run()
  // are not guaranteed to be executed.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.OOMKills.Count", oom_kills_count_, 1, 1000,
                              1001);

  // In practice most process has oom_badness < 1000, but
  // strictly speaking the number could be [1, 2000]. What it really
  // means is the baseline, proportion of memory used (normalized to
  // [0, 1000]), plus an adjustment score oom_score_adj [-1000, 1000],
  // truncated to 1 if negative (0 means never kill).
  // Ref: https://lwn.net/Articles/396552/
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.OOMKills.Score", oom_badness, 1, 2000, 2001);

  if (time_stamp > 0) {
    // Sets to |kMaxMemoryKillTimeDelta| for the first kill event.
    const TimeDelta time_delta =
        last_oom_kill_time_ < 0
            ? kMaxMemoryKillTimeDelta
            : TimeDelta::FromMicroseconds(time_stamp - last_oom_kill_time_);

    last_oom_kill_time_ = time_stamp;

    UMA_HISTOGRAM_MEMORY_KILL_TIME_INTERVAL("Arc.OOMKills.TimeDelta",
                                            time_delta);
  }
}

MemoryKillsMonitor* MemoryKillsMonitor::GetForTesting() {
  return g_memory_kills_monitor_instance.Pointer();
}

}  // namespace memory
