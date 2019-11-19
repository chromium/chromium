// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_delegate_chromeos.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <vector>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"  // kNullProcessHandle.
#include "base/process/process_metrics.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator_chromeos.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/memory_kills_monitor.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "services/service_manager/zygote/zygote_host_linux.h"
#include "ui/wm/public/activation_client.h"

using base::ProcessHandle;
using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;

namespace resource_coordinator {
namespace {

// The default interval after which to adjust OOM scores.
constexpr base::TimeDelta kAdjustmentInterval =
    base::TimeDelta::FromSeconds(10);

// When switching to a new tab the tab's renderer's OOM score needs to be
// updated to reflect its front-most status and protect it from discard.
// However, doing this immediately might slow down tab switch time, so wait
// a little while before doing the adjustment.
const int kFocusedProcessScoreAdjustIntervalMs = 500;

wm::ActivationClient* GetActivationClient() {
  if (!ash::Shell::HasInstance())
    return nullptr;
  return wm::GetActivationClient(ash::Shell::GetPrimaryRootWindow());
}

void OnSetOomScoreAdj(bool success, const std::string& output) {
  VLOG(2) << "OnSetOomScoreAdj " << success << " " << output;
  if (!success)
    LOG(ERROR) << "Set OOM score error: " << output;
  else if (!output.empty())
    LOG(WARNING) << "Set OOM score: " << output;
}

}  // namespace

// static
const int TabManagerDelegate::kPersistentArcAppOomScore = -100;

std::ostream& operator<<(std::ostream& os, const ProcessType& type) {
  switch (type) {
    case ProcessType::FOCUSED_TAB:
      return os << "FOCUSED_TAB";
    case ProcessType::FOCUSED_APP:
      return os << "FOCUSED_APP";
    case ProcessType::UNKNOWN_TYPE:
      return os << "UNKNOWN_TYPE";
    case ProcessType::BACKGROUND:
      return os << "BACKGROUND";
    case ProcessType::PROTECTED_BACKGROUND:
      return os << "PROTECTED_BACKGROUND";
    case ProcessType::CACHED_APP:
      return os << "CACHED_APP";
    default:
      return os << "NOT_IMPLEMENTED_ERROR";
  }
  return os;
}

// TabManagerDelegate::Candidate implementation.
std::ostream& operator<<(std::ostream& out,
                         const TabManagerDelegate::Candidate& candidate) {
  if (candidate.app())
    out << "app " << *candidate.app();
  else if (candidate.lifecycle_unit())
    out << "tab " << candidate.lifecycle_unit()->GetTitle();
  out << ", process_type " << candidate.process_type();
  return out;
}

TabManagerDelegate::Candidate& TabManagerDelegate::Candidate::operator=(
    TabManagerDelegate::Candidate&& other) = default;

bool TabManagerDelegate::Candidate::operator<(
    const TabManagerDelegate::Candidate& rhs) const {
  if (process_type() != rhs.process_type())
    return process_type() < rhs.process_type();
  return LastActivityTime() > rhs.LastActivityTime();
}

ProcessType TabManagerDelegate::Candidate::GetProcessTypeInternal() const {
  if (app()) {
    if (app()->is_focused())
      return ProcessType::FOCUSED_APP;
    if (app()->IsBackgroundProtected())
      return ProcessType::PROTECTED_BACKGROUND;
    if (app()->IsCached())
      return ProcessType::CACHED_APP;
    return ProcessType::BACKGROUND;
  }
  if (lifecycle_unit()) {
    if (LastActivityTime() == base::TimeTicks::Max())
      return ProcessType::FOCUSED_TAB;
    DecisionDetails decision_details;
    if (!lifecycle_unit()->CanDiscard(
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE,
            &decision_details)) {
      return ProcessType::PROTECTED_BACKGROUND;
    }
    return ProcessType::BACKGROUND;
  }
  return ProcessType::UNKNOWN_TYPE;
}

base::TimeTicks TabManagerDelegate::Candidate::LastActivityTime() const {
  if (app()) {
    return base::TimeTicks::FromUptimeMillis(app()->last_activity_time());
  }
  if (lifecycle_unit()) {
    return lifecycle_unit()->GetLastFocusedTime();
  }
  NOTREACHED();
  return base::TimeTicks();
}

// Holds the info of a newly focused tab or app window. The focused process is
// set to highest priority (lowest OOM score), but not immediately. To avoid
// redundant settings the OOM score adjusting only happens after a timeout. If
// the process loses focus before the timeout, the adjustment is canceled.
class TabManagerDelegate::FocusedProcess {
 public:
  static const int kInvalidArcAppNspid = 0;

  FocusedProcess() { Reset(); }

  void SetTabPid(const base::ProcessHandle pid) {
    pid_ = pid;
    nspid_ = kInvalidArcAppNspid;
  }

  void SetArcAppNspid(const int nspid) {
    pid_ = base::kNullProcessHandle;
    nspid_ = nspid;
  }

  base::ProcessHandle GetTabPid() const { return pid_; }

  int GetArcAppNspid() const { return nspid_; }

  // Checks whether the containing instance is an ARC app. If so it resets the
  // data and returns true. Useful when canceling an ongoing OOM score setting
  // for a focused ARC app because the focus has been shifted away shortly.
  bool ResetIfIsArcApp() {
    if (nspid_ != kInvalidArcAppNspid) {
      Reset();
      return true;
    }
    return false;
  }

 private:
  void Reset() {
    pid_ = base::kNullProcessHandle;
    nspid_ = kInvalidArcAppNspid;
  }

  // The focused app could be a Chrome tab or an Android app, but not both.
  // At most one of them contains a valid value at any time.

  // If a chrome tab.
  base::ProcessHandle pid_;
  // If an Android app.
  int nspid_;
};

// TabManagerDelegate::MemoryStat implementation.

// static
int TabManagerDelegate::MemoryStat::ReadIntFromFile(const char* file_name,
                                                    const int default_val) {
  std::string file_string;
  if (!base::ReadFileToString(base::FilePath(file_name), &file_string)) {
    LOG(ERROR) << "Unable to read file" << file_name;
    return default_val;
  }
  int val = default_val;
  if (!base::StringToInt(
          base::TrimWhitespaceASCII(file_string, base::TRIM_TRAILING), &val)) {
    LOG(ERROR) << "Unable to parse string" << file_string;
    return default_val;
  }
  return val;
}

// static
int TabManagerDelegate::MemoryStat::LowMemoryMarginKB() {
  constexpr int kDefaultLowMemoryMarginMb = 50;

  // A margin file can contain multiple values but the first one
  // represents the critical memory threshold.
  std::vector<int> margin_parts =
      util::chromeos::SystemMemoryPressureEvaluator::GetMarginFileParts();
  if (!margin_parts.empty()) {
    return margin_parts[0] * 1024;
  }

  return kDefaultLowMemoryMarginMb * 1024;
}

// Target memory to free is the amount which brings available
// memory back to the margin.
int TabManagerDelegate::MemoryStat::TargetMemoryToFreeKB() {
  static constexpr char kLowMemAvailableEntry[] =
      "/sys/kernel/mm/chromeos-low_mem/available";
  const int available_mem_mb = ReadIntFromFile(kLowMemAvailableEntry, 0);
  // available_mem_mb is rounded down in the kernel computation, so even if
  // it's just below the margin, the difference will be at least 1 MB.  This
  // matters because we shouldn't return 0 when we're below the margin.
  return LowMemoryMarginKB() - available_mem_mb * 1024;
}

int TabManagerDelegate::MemoryStat::EstimatedMemoryFreedKB(
    base::ProcessHandle handle) {
  return GetPrivateMemoryKB(handle);
}

TabManagerDelegate::TabManagerDelegate(
    const base::WeakPtr<TabManager>& tab_manager)
    : TabManagerDelegate(tab_manager, new MemoryStat()) {}

TabManagerDelegate::TabManagerDelegate(
    const base::WeakPtr<TabManager>& tab_manager,
    TabManagerDelegate::MemoryStat* mem_stat)
    : tab_manager_(tab_manager),
      focused_process_(new FocusedProcess()),
      mem_stat_(mem_stat) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
  auto* activation_client = GetActivationClient();
  if (activation_client)
    activation_client->AddObserver(this);
  BrowserList::GetInstance()->AddObserver(this);
}

TabManagerDelegate::~TabManagerDelegate() {
  BrowserList::GetInstance()->RemoveObserver(this);
  auto* activation_client = GetActivationClient();
  if (activation_client)
    activation_client->RemoveObserver(this);
}

void TabManagerDelegate::OnBrowserSetLastActive(Browser* browser) {
  // Set OOM score to the selected tab when a browser window is activated.
  // content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED didn't catch the
  // case (like when switching focus between 2 browser windows) so we need to
  // handle it here.
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int selected_index = tab_strip_model->active_index();
  content::WebContents* contents =
      tab_strip_model->GetWebContentsAt(selected_index);
  if (!contents)
    return;

  base::ProcessHandle pid =
      contents->GetMainFrame()->GetProcess()->GetProcess().Handle();
  AdjustFocusedTabScore(pid);
}

void TabManagerDelegate::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (arc::IsArcAppWindow(gained_active)) {
    // Currently there is no way to know which app is displayed in the ARC
    // window, so schedule an early adjustment for all processes to reflect
    // the change.
    // Put a dummy FocusedProcess with nspid = kInvalidArcAppNspid for now to
    // indicate the focused process is an arc app.
    // TODO(cylee): Fix it when we have nspid info in ARC windows.
    focused_process_->SetArcAppNspid(FocusedProcess::kInvalidArcAppNspid);
    // If the timer is already running (possibly for a tab), it'll be reset
    // here.
    focus_process_score_adjust_timer_.Start(
        FROM_HERE,
        TimeDelta::FromMilliseconds(kFocusedProcessScoreAdjustIntervalMs), this,
        &TabManagerDelegate::ScheduleEarlyOomPrioritiesAdjustment);
  }
  if (arc::IsArcAppWindow(lost_active)) {
    // Do not bother adjusting OOM score if the ARC window is deactivated
    // shortly.
    if (focused_process_->ResetIfIsArcApp() &&
        focus_process_score_adjust_timer_.IsRunning())
      focus_process_score_adjust_timer_.Stop();
  }
}

void TabManagerDelegate::StartPeriodicOOMScoreUpdate() {
  DCHECK(!adjust_oom_priorities_timer_.IsRunning());
  adjust_oom_priorities_timer_.Start(FROM_HERE, kAdjustmentInterval, this,
                                     &TabManagerDelegate::AdjustOomPriorities);
}

void TabManagerDelegate::ScheduleEarlyOomPrioritiesAdjustment() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AdjustOomPriorities();
}

// If able to get the list of ARC processes, prioritize tabs and apps as a
// whole. Otherwise try to kill tabs only.
void TabManagerDelegate::LowMemoryKill(
    ::mojom::LifecycleUnitDiscardReason reason,
    TabManager::TabDiscardDoneCB tab_discard_done) {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  base::TimeTicks now = base::TimeTicks::Now();
  if (arc_process_service) {
    arc_process_service->RequestAppProcessList(base::BindOnce(
        &TabManagerDelegate::LowMemoryKillImpl, weak_ptr_factory_.GetWeakPtr(),
        now, reason, std::move(tab_discard_done)));
  } else {
    LowMemoryKillImpl(now, reason, std::move(tab_discard_done), base::nullopt);
  }
}

int TabManagerDelegate::GetCachedOomScore(ProcessHandle process_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = oom_score_map_.find(process_handle);
  if (it != oom_score_map_.end()) {
    return it->second;
  }
  // An impossible value for oom_score_adj.
  return -1001;
}

LifecycleUnitVector TabManagerDelegate::GetLifecycleUnits() {
  if (tab_manager_)
    return tab_manager_->GetSortedLifecycleUnits();
  return LifecycleUnitVector();
}

void TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ProcessHandle pid = focused_process_->GetTabPid();
  // The focused process doesn't render a tab. Could happen when the focus
  // just switched to an ARC app before the timeout. We can not avoid the race.
  if (pid == base::kNullProcessHandle)
    return;

  // Update the OOM score cache.
  oom_score_map_[pid] = chrome::kLowestRendererOomScore;

  // Sets OOM score.
  VLOG(3) << "Set OOM score " << chrome::kLowestRendererOomScore
          << " for focused tab " << pid;
  if (!base::AdjustOOMScore(pid, chrome::kLowestRendererOomScore))
    LOG(ERROR) << "Failed to set oom_score_adj to "
               << chrome::kLowestRendererOomScore
               << " for focused tab, pid: " << pid;
}

void TabManagerDelegate::AdjustFocusedTabScore(base::ProcessHandle pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Clear running timer if one was set for a previous focused tab/app.
  if (focus_process_score_adjust_timer_.IsRunning())
    focus_process_score_adjust_timer_.Stop();
  focused_process_->SetTabPid(pid);

  // If the currently focused tab already has a lower score, do not
  // set it. This can happen in case the newly focused tab is script
  // connected to the previous tab.
  ProcessScoreMap::iterator it = oom_score_map_.find(pid);
  const bool not_lowest_score = (it == oom_score_map_.end() ||
                                 it->second != chrome::kLowestRendererOomScore);

  if (not_lowest_score) {
    // By starting a timer we guarantee that the tab is focused for
    // certain amount of time. Secondly, it also does not add overhead
    // to the tab switching time.
    // If there's an existing running timer (could be for ARC app), it
    // would be replaced by a new task.
    focus_process_score_adjust_timer_.Start(
        FROM_HERE,
        TimeDelta::FromMilliseconds(kFocusedProcessScoreAdjustIntervalMs), this,
        &TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout);
  }
}

void TabManagerDelegate::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      oom_score_map_.erase(host->GetProcess().Handle());
      // Coming here we know that a renderer was just killed and memory should
      // come back into the pool. However - the memory pressure observer did
      // not yet update its status and therefore we ask it to redo the
      // measurement, calling us again if we have to release more.
      // Note: We do not only accelerate the discarding speed by doing another
      // check in short succession - we also accelerate it because the timer
      // driven MemoryPressureMonitor will continue to produce timed events
      // on top. So the longer the cleanup phase takes, the more tabs will
      // get discarded in parallel.

      auto* monitor = util::chromeos::SystemMemoryPressureEvaluator::Get();
      if (monitor) {
        monitor->ScheduleEarlyCheck();
      }
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED: {
      bool visible = *content::Details<bool>(details).ptr();
      if (visible) {
        content::RenderProcessHost* render_host =
            content::Source<content::RenderWidgetHost>(source)
                .ptr()
                ->GetProcess();
        AdjustFocusedTabScore(render_host->GetProcess().Handle());
      }
      // Do not handle the "else" case when it changes to invisible because
      // 1. The behavior is a bit awkward in that when switching from tab A to
      // tab B, the event "invisible of B" comes after "visible of A". It can
      // cause problems when the 2 tabs have the same content (e.g., New Tab
      // Page). To be more clear, if we try to cancel the timer when losing
      // focus it may cancel the timer for the same renderer process.
      // 2. When another window is launched on top of an existing browser
      // window, the selected tab in the existing browser didn't receive this
      // event, so an attempt to cancel timer in this case doesn't work.
      break;
    }
    default:
      NOTREACHED() << "Received unexpected notification";
      break;
  }
}

// Here we collect most of the information we need to sort the existing
// renderers in priority order, and hand out oom_score_adj scores based on that
// sort order.
//
// Things we need to collect on the browser thread (because
// TabStripModel isn't thread safe):
// 1) whether or not a tab is pinned
// 2) last time a tab was selected
// 3) is the tab currently selected
void TabManagerDelegate::AdjustOomPriorities() {
  // If Chrome is shutting down, do not do anything
  if (g_browser_process->IsShuttingDown())
    return;

  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  // TODO(b/135633925): Design and implement OOM handling for ARCVM.
  if (arc_process_service && !arc::IsArcVmEnabled()) {
    arc_process_service->RequestAppProcessList(
        base::BindOnce(&TabManagerDelegate::AdjustOomPrioritiesImpl,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Pass in nullopt if unable to get ARC processes.
    AdjustOomPrioritiesImpl(base::nullopt);
  }
}

// Get a list of candidates to kill, sorted by descending importance.
// static
std::vector<TabManagerDelegate::Candidate>
TabManagerDelegate::GetSortedCandidates(
    const LifecycleUnitVector& lifecycle_units,
    const OptionalArcProcessList& arc_processes) {
  std::vector<Candidate> candidates;
  candidates.reserve(lifecycle_units.size() +
                     (arc_processes ? (*arc_processes).size() : 0));

  for (LifecycleUnit* lifecycle_unit : lifecycle_units) {
    candidates.emplace_back(lifecycle_unit);
  }

  if (arc_processes) {
    for (const auto& app : *arc_processes) {
      candidates.emplace_back(&app);
    }
  }

  // Sort candidates according to priority.
  std::sort(candidates.begin(), candidates.end());

  return candidates;
}

void TabManagerDelegate::LogAndMaybeSortLifecycleUnitWithTabRanker(
    std::vector<Candidate>* candidates,
    LifecycleUnitSorter sorter) {
  const uint32_t num_of_tab_to_score = GetNumOldestTabsToScoreWithTabRanker();
  if (num_of_tab_to_score <= 1)
    return;

  const ProcessType process_type =
      static_cast<ProcessType>(GetProcessTypeToScoreWithTabRanker());

  // Put the oldest num_of_tab_to_score lifecycle units into a vector.
  LifecycleUnitVector oldest_lifecycle_units;
  for (auto it = candidates->rbegin(); it != candidates->rend(); ++it) {
    auto& candidate = *it;
    if (oldest_lifecycle_units.size() == num_of_tab_to_score ||
        candidate.process_type() < process_type)
      break;
    if (candidate.lifecycle_unit()) {
      oldest_lifecycle_units.push_back(candidate.lifecycle_unit());
    }
  }

  // log and possibly Re-sort them with TabRanker.
  std::move(sorter).Run(&oldest_lifecycle_units);

  if (base::FeatureList::IsEnabled(features::kTabRanker)) {
    // Put the sorted lifecycle units back to their original vacancies.
    for (auto it = candidates->rbegin(); it != candidates->rend(); ++it) {
      const auto& candidate = *it;
      if (oldest_lifecycle_units.empty() ||
          candidate.process_type() < process_type)
        break;
      if (candidate.lifecycle_unit()) {
        *it = Candidate(oldest_lifecycle_units.back());
        oldest_lifecycle_units.pop_back();
      }
    }
  }
}

bool TabManagerDelegate::IsRecentlyKilledArcProcess(
    const std::string& process_name,
    const TimeTicks& now) {
  const auto it = recently_killed_arc_processes_.find(process_name);
  if (it == recently_killed_arc_processes_.end())
    return false;
  return (now - it->second) <= GetArcRespawnKillDelay();
}

bool TabManagerDelegate::KillArcProcess(const int nspid) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* arc_process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->process(), KillProcess);
  if (!arc_process_instance)
    return false;

  arc_process_instance->KillProcess(nspid, "LowMemoryKill");
  return true;
}

bool TabManagerDelegate::KillTab(LifecycleUnit* lifecycle_unit,
                                 ::mojom::LifecycleUnitDiscardReason reason) {
  bool did_discard = lifecycle_unit->Discard(reason);
  return did_discard;
}

chromeos::DebugDaemonClient* TabManagerDelegate::GetDebugDaemonClient() {
  return chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
}

void TabManagerDelegate::LowMemoryKillImpl(
    base::TimeTicks start_time,
    ::mojom::LifecycleUnitDiscardReason reason,
    TabManager::TabDiscardDoneCB tab_discard_done,
    OptionalArcProcessList arc_processes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(2) << "LowMemoryKillImpl";

  // Prevent persistent ARC processes from being killed.
  if (arc_processes) {
    base::EraseIf(*arc_processes,
                  [](auto& proc) { return proc.IsPersistent(); });
  }

  std::vector<Candidate> candidates =
      GetSortedCandidates(GetLifecycleUnits(), arc_processes);

  // Log and Re-order oldest N LifecycleUnits if TabRanker is enabled; otherwise
  // only log N LifecycleUnits and the candidates will be unchanged.
  LogAndMaybeSortLifecycleUnitWithTabRanker(
      &candidates,
      base::BindOnce(
          &TabActivityWatcher::LogAndMaybeSortLifecycleUnitWithTabRanker,
          base::Unretained(TabActivityWatcher::GetInstance())));

  // TODO(semenzato): decide if TargetMemoryToFreeKB is doing real
  // I/O and if it is, move to I/O thread (crbug.com/778703).
  int target_memory_to_free_kb = 0;
  {
    base::ScopedAllowBlocking allow_blocking;
    target_memory_to_free_kb = mem_stat_->TargetMemoryToFreeKB();
  }

  MEMORY_LOG(ERROR) << "List of low memory kill candidates "
                       "(sorted from low priority to high priority):";
  for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
    MEMORY_LOG(ERROR) << *it;
  }

  // Kill processes until the estimated amount of freed memory is sufficient to
  // bring the system memory back to a normal level.
  // The list is sorted by descending importance, so we go through the list
  // backwards.
  const TimeTicks now = TimeTicks::Now();
  base::TimeTicks first_kill_time;
  for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
    MEMORY_LOG(ERROR) << "Target memory to free: " << target_memory_to_free_kb
                      << " KB";
    if (target_memory_to_free_kb <= 0)
      break;

    const ProcessType process_type = it->process_type();

    // Never kill selected tab and foreground app regardless of whether they're
    // in the active window. Since the user experience would be bad.
    if (it->app()) {
      if (process_type == ProcessType::FOCUSED_APP) {
        MEMORY_LOG(ERROR) << "Skipped killing focused app "
                          << it->app()->process_name();
        continue;
      }
      if (IsRecentlyKilledArcProcess(it->app()->process_name(), now)) {
        MEMORY_LOG(ERROR) << "Avoided killing " << it->app()->process_name()
                          << " too often";
        continue;
      }
      int estimated_memory_freed_kb =
          mem_stat_->EstimatedMemoryFreedKB(it->app()->pid());
      if (KillArcProcess(it->app()->nspid())) {
        if (first_kill_time.is_null()) {
          first_kill_time = base::TimeTicks::Now();
        }
        recently_killed_arc_processes_[it->app()->process_name()] = now;
        target_memory_to_free_kb -= estimated_memory_freed_kb;
        memory::MemoryKillsMonitor::LogLowMemoryKill("APP",
                                                     estimated_memory_freed_kb);
        MEMORY_LOG(ERROR) << "Killed app " << it->app()->process_name() << " ("
                          << it->app()->pid() << ")"
                          << ", estimated " << estimated_memory_freed_kb
                          << " KB freed";
      } else {
        MEMORY_LOG(ERROR) << "Failed to kill " << it->app()->process_name();
      }
    } else if (it->lifecycle_unit()) {
      if (process_type == ProcessType::FOCUSED_TAB) {
        MEMORY_LOG(ERROR) << "Skipped killing focused tab "
                          << it->lifecycle_unit()->GetTitle();
        continue;
      }

      // The estimation is problematic since multiple tabs may share the same
      // process, while the calculation counts memory used by the whole process.
      // So |estimated_memory_freed_kb| is an over-estimation.
      int estimated_memory_freed_kb =
          it->lifecycle_unit()->GetEstimatedMemoryFreedOnDiscardKB();
      if (KillTab(it->lifecycle_unit(), reason)) {
        if (first_kill_time.is_null()) {
          first_kill_time = base::TimeTicks::Now();
        }
        target_memory_to_free_kb -= estimated_memory_freed_kb;
        memory::MemoryKillsMonitor::LogLowMemoryKill("TAB",
                                                     estimated_memory_freed_kb);
        MEMORY_LOG(ERROR) << "Killed tab " << it->lifecycle_unit()->GetTitle()
                          << ", estimated " << estimated_memory_freed_kb
                          << " KB freed";
      }
    }
  }
  if (target_memory_to_free_kb > 0) {
    MEMORY_LOG(ERROR)
        << "Unable to kill enough candidates to meet target_memory_to_free_kb ";
  }
  if (!first_kill_time.is_null()) {
    TimeDelta delta = first_kill_time - start_time;
    MEMORY_LOG(ERROR) << "Time to first kill " << delta;
    UMA_HISTOGRAM_MEDIUM_TIMES("Arc.LowMemoryKiller.FirstKillLatency", delta);
  }

  // tab_discard_done runs when it goes out of the scope.
}

void TabManagerDelegate::AdjustOomPrioritiesImpl(
    OptionalArcProcessList arc_processes) {
  std::vector<TabManagerDelegate::Candidate> candidates;
  std::vector<TabManagerDelegate::Candidate> apps_persistent;

  // Least important first.
  LifecycleUnitVector lifecycle_units = GetLifecycleUnits();
  auto all_candidates =
      GetSortedCandidates(std::move(lifecycle_units), arc_processes);
  for (auto& candidate : all_candidates) {
    // TODO(cylee|yusukes): Consider using IsImportant() instead of
    // IsPersistent() for simplicity.
    // TODO(cylee): Also consider protecting FOCUSED_TAB from the kernel OOM
    // killer so that Chrome and the kernel do the same regarding OOM handling.
    if (candidate.app() && candidate.app()->IsPersistent()) {
      // Add persistent apps to |apps_persistent|.
      apps_persistent.emplace_back(std::move(candidate));
    } else {
      // Add tabs and killable apps to |candidates|.
      candidates.emplace_back(std::move(candidate));
    }
  }

  // Now we assign priorities based on the sorted list. We're assigning
  // priorities in the range of kLowestRendererOomScore to
  // kHighestRendererOomScore (defined in chrome_constants.h). oom_score_adj
  // takes values from -1000 to 1000. Negative values are reserved for system
  // processes, and we want to give some room below the range we're using to
  // allow for things that want to be above the renderers in priority, so the
  // defined range gives us some variation in priority without taking up the
  // whole range. In the end, however, it's a pretty arbitrary range to use.
  // Higher values are more likely to be killed by the OOM killer.

  // Break the processes into 2 parts. This is to help lower the chance of
  // altering OOM score for many processes on any small change.
  int range_middle =
      (chrome::kLowestRendererOomScore + chrome::kHighestRendererOomScore) / 2;

  // Find some pivot point. FOCUSED_TAB, FOCUSED_APP, and PROTECTED_BACKGROUND
  // processes are in the first half and BACKGROUND and CACHED_APP processes
  // are in the second half.
  auto lower_priority_part = candidates.end();
  ProcessType pivot_type = ProcessType::BACKGROUND;
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->process_type() >= pivot_type) {
      lower_priority_part = it;
      break;
    }
  }

  ProcessScoreMap new_map;

  // Make the apps harder to kill.
  DistributeOomScoreInRange(apps_persistent.begin(), apps_persistent.end(),
                            kPersistentArcAppOomScore,
                            kPersistentArcAppOomScore, &new_map);

  // Higher priority part.
  DistributeOomScoreInRange(candidates.begin(), lower_priority_part,
                            chrome::kLowestRendererOomScore, range_middle,
                            &new_map);
  // Lower priority part.
  DistributeOomScoreInRange(lower_priority_part, candidates.end(), range_middle,
                            chrome::kHighestRendererOomScore, &new_map);

  oom_score_map_.swap(new_map);
}

void TabManagerDelegate::DistributeOomScoreInRange(
    std::vector<TabManagerDelegate::Candidate>::iterator begin,
    std::vector<TabManagerDelegate::Candidate>::iterator end,
    int range_begin,
    int range_end,
    ProcessScoreMap* new_map) {
  // Processes whose OOM scores should be updated. Ignore duplicated pids but
  // the last occurrence.
  std::map<base::ProcessHandle, int32_t> oom_scores_to_change;

  // Though there might be duplicate process handles, it doesn't matter to
  // overestimate the number of processes here since the we don't need to
  // use up the full range.
  int num = (end - begin);
  const float priority_increment =
      static_cast<float>(range_end - range_begin) / num;

  float priority = range_begin;
  for (auto cur = begin; cur != end; ++cur) {
    const int score = round(priority);

    base::ProcessHandle pid = base::kNullProcessHandle;
    if (cur->app()) {
      pid = cur->app()->pid();
    } else {
      pid = cur->lifecycle_unit()->GetProcessHandle();
      // 1. tab_list contains entries for already-discarded tabs. If the PID
      // (renderer_handle) is zero, we don't need to adjust the oom_score.
      // 2. Only add unseen process handle so if there's multiple tab maps to
      // the same process, the process is set to an OOM score based on its "most
      // important" tab.
      if (pid == base::kNullProcessHandle ||
          new_map->find(pid) != new_map->end())
        continue;
    }

    if (pid == base::kNullProcessHandle)
      continue;

    // Update the to-be-cached OOM score map. Use pid as map keys so it's
    // globally unique.
    (*new_map)[pid] = score;

    // Need to update OOM score if the calculated score is different from
    // current cached score.
    if (oom_score_map_[pid] != score) {
      VLOG(3) << "Update OOM score " << score << " for " << *cur;
      if (cur->app()) {
        oom_scores_to_change[pid] = static_cast<int32_t>(score);
      } else {
        if (!base::AdjustOOMScore(pid, score))
          LOG(ERROR) << "Failed to set oom_score_adj to " << score
                     << " for process " << pid;
      }
    }
    priority += priority_increment;
  }

  if (oom_scores_to_change.size()) {
    GetDebugDaemonClient()->SetOomScoreAdj(oom_scores_to_change,
                                           base::Bind(&OnSetOomScoreAdj));
  }
}

}  // namespace resource_coordinator
