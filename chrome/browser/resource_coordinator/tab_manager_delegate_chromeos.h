// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_DELEGATE_CHROMEOS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/arc/process/arc_process.h"
#include "chrome/browser/chromeos/arc/process/arc_process_service.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/arc/mojom/process.mojom.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/wm/public/activation_change_observer.h"

namespace resource_coordinator {

// Possible types of Apps/Tabs processes. From most important to least
// important.
enum class ProcessType {
  // Conceptually, the system cannot have both FOCUSED_TAB and FOCUSED_APP at
  // the same time, but because Chrome cannot retrieve FOCUSED_APP status
  // synchronously, Chrome may still see both at the same time. When that
  // happens, treat FOCUSED_TAB as the most important since the (synchronously
  // retrieved) tab information is more reliable and up-to-date.
  FOCUSED_TAB = 1,
  FOCUSED_APP = 2,

  // PROTECTED_BACKGROUND processes are those that are in the background but
  // are more important than BACKGROUND processes because they may be disruptive
  // to the user if killed. CACHED_APP marks Android processes which are cached
  // or empty.
  PROTECTED_BACKGROUND = 3,
  BACKGROUND = 4,
  CACHED_APP = 5,
  UNKNOWN_TYPE = 6,
};

// The Chrome OS TabManagerDelegate is responsible for keeping the
// renderers' scores up to date in /proc/<pid>/oom_score_adj.
class TabManagerDelegate : public wm::ActivationChangeObserver,
                           public content::NotificationObserver,
                           public BrowserListObserver {
 public:
  class MemoryStat;

  explicit TabManagerDelegate(const base::WeakPtr<TabManager>& tab_manager);

  TabManagerDelegate(const base::WeakPtr<TabManager>& tab_manager,
                     TabManagerDelegate::MemoryStat* mem_stat);

  ~TabManagerDelegate() override;

  void OnBrowserSetLastActive(Browser* browser) override;

  // aura::ActivationChangeObserver overrides.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Called by TabManager::Start to start a timer that periodically updates
  // OOM scores.
  void StartPeriodicOOMScoreUpdate();

  // Kills a process on memory pressure.
  void LowMemoryKill(::mojom::LifecycleUnitDiscardReason reason,
                     TabManager::TabDiscardDoneCB tab_discard_done);

  // Returns oom_score_adj of a process if the score is cached by |this|.
  // If couldn't find the score in the cache, returns -1001 since the valid
  // range of oom_score_adj is [-1000, 1000].
  int GetCachedOomScore(base::ProcessHandle process_handle);

  // Returns true if the process has recently been killed.
  // Virtual for unit testing.
  virtual bool IsRecentlyKilledArcProcess(const std::string& process_name,
                                          const base::TimeTicks& now);

 protected:
  // Kills an ARC process. Returns true if the kill request is successfully sent
  // to Android. Virtual for unit testing.
  virtual bool KillArcProcess(const int nspid);

  // Kills a tab. Returns true if the tab is killed successfully.
  // Virtual for unit testing.
  virtual bool KillTab(LifecycleUnit* lifecycle_unit,
                       ::mojom::LifecycleUnitDiscardReason reason);

  // Get debugd client instance. Virtual for unit testing.
  virtual chromeos::DebugDaemonClient* GetDebugDaemonClient();

 private:
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest, CandidatesSorted);
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest,
                           CandidatesSortedWithFocusedAppAndTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest,
                           SortLifecycleUnitWithTabRanker);
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest,
                           DoNotKillRecentlyKilledArcProcesses);
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest, IsRecentlyKilledArcProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest, KillMultipleProcesses);
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest, SetOomScoreAdj);

  using OptionalArcProcessList = arc::ArcProcessService::OptionalArcProcessList;

  class Candidate;
  class FocusedProcess;

  friend std::ostream& operator<<(std::ostream& out,
                                  const Candidate& candidate);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Pair to hold child process host id and ProcessHandle.
  typedef std::pair<int, base::ProcessHandle> ProcessInfo;

  // Cache OOM scores in memory.
  typedef base::flat_map<base::ProcessHandle, int> ProcessScoreMap;

  // A map from an ARC process name to a monotonic timestamp when it's killed.
  typedef base::flat_map<std::string, base::TimeTicks> KilledArcProcessesMap;

  typedef base::OnceCallback<void(LifecycleUnitVector*)> LifecycleUnitSorter;

  // Get the list of candidates to kill, sorted by descending importance.
  static std::vector<Candidate> GetSortedCandidates(
      const LifecycleUnitVector& lifecycle_units,
      const OptionalArcProcessList& arc_processes);

  // This is only used for TabRanker experiment for now.
  // If TabRanker is enabled, this will take the last N lifecycle units in the
  // |candidates|; sort these lifecycle units based on the TabRanker order; and
  // put these sorted lifecycle unit back to the vacancies of these lifecycle
  // units in the |candidates|.
  // If TabRanker is disabled, this will log the TabMetrics of these lifecycle
  // units.
  // All apps in the |candidates| will not be influenced.
  static void LogAndMaybeSortLifecycleUnitWithTabRanker(
      std::vector<Candidate>* candidates,
      LifecycleUnitSorter sorter);

  // Returns the LifecycleUnits in TabManager. Virtual for unit tests.
  virtual LifecycleUnitVector GetLifecycleUnits();

  // Sets OOM score for the focused tab.
  void OnFocusTabScoreAdjustmentTimeout();

  // Kills a process after getting all info of tabs and apps.
  void LowMemoryKillImpl(base::TimeTicks start_time,
                         ::mojom::LifecycleUnitDiscardReason reason,
                         TabManager::TabDiscardDoneCB tab_discard_done,
                         OptionalArcProcessList arc_processes);

  // Sets a newly focused tab the highest priority process if it wasn't.
  void AdjustFocusedTabScore(base::ProcessHandle pid);

  // Called when the timer fires, sets oom_adjust_score for all renderers.
  void AdjustOomPriorities();

  // Called by AdjustOomPriorities. Runs on the main thread.
  void AdjustOomPrioritiesImpl(OptionalArcProcessList arc_processes);

  // Sets OOM score for processes in the range [|rbegin|, |rend|) to integers
  // distributed evenly in [|range_begin|, |range_end|).
  // The new score is set in |new_map|.
  void DistributeOomScoreInRange(
      std::vector<TabManagerDelegate::Candidate>::iterator begin,
      std::vector<TabManagerDelegate::Candidate>::iterator end,
      int range_begin,
      int range_end,
      ProcessScoreMap* new_map);

  // Initiates an oom priority adjustment.
  void ScheduleEarlyOomPrioritiesAdjustment();

  // Returns a TimeDelta object that represents a minimum delay for killing
  // the same ARC process again. ARC processes sometimes respawn right after
  // being killed. In that case, killing them every time is just a waste of
  // resources.
  static constexpr base::TimeDelta GetArcRespawnKillDelay() {
    return base::TimeDelta::FromSeconds(60);
  }

  // The OOM adjustment score for persistent ARC processes.
  static const int kPersistentArcAppOomScore;

  // Holds a reference to the owning TabManager.
  const base::WeakPtr<TabManager> tab_manager_;

  // Registrar to receive renderer notifications.
  content::NotificationRegistrar registrar_;

  // Timer to periodically make OOM score adjustments.
  base::RepeatingTimer adjust_oom_priorities_timer_;

  // Timer to guarantee that the tab or app is focused for a certain amount of
  // time.
  base::OneShotTimer focus_process_score_adjust_timer_;
  // Holds the info of the newly focused tab or app. Its OOM score would be
  // adjusted when |focus_process_score_adjust_timer_| is expired.
  std::unique_ptr<FocusedProcess> focused_process_;

  // Map maintaining the process handle - oom_score mapping.
  ProcessScoreMap oom_score_map_;

  // Map maintaing ARC process names and their last killed time.
  KilledArcProcessesMap recently_killed_arc_processes_;

  // Util for getting system memory status.
  std::unique_ptr<TabManagerDelegate::MemoryStat> mem_stat_;

  // Weak pointer factory used for posting tasks to other threads.
  base::WeakPtrFactory<TabManagerDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabManagerDelegate);
};

// On ARC enabled machines, either a tab or an app could be a possible
// victim of low memory kill process. This is a helper class which holds a
// pointer to an app or a tab (but not both) to facilitate prioritizing the
// victims.
class TabManagerDelegate::Candidate {
 public:
  explicit Candidate(LifecycleUnit* lifecycle_unit)
      : lifecycle_unit_(lifecycle_unit) {
    DCHECK(lifecycle_unit_);
  }

  // Candidates are sorted by a pair of sort keys <TabRanker score,
  // last_activity_time>. Apps are not scored by TabRanker, so their score
  // is set to kMaxScore. When TabRanker is off, tabs also have a score of
  // kMaxScore so this has the effect of sorting by last_activity_time only.
  // But if TabRanker is on, kMaxScore guarantees all apps are sorted before
  // tabs.
  explicit Candidate(const arc::ArcProcess* app) : app_(app) { DCHECK(app_); }

  // Move-only class.
  Candidate(Candidate&&) = default;
  Candidate& operator=(Candidate&& other);

  // Candidates are sorted by descending importance. A candidate is more
  // important if:
  // (1) it has lower respective ProcessTypes.
  // (2) it has the same ProcessTypes, but larger LastActivityTime().
  bool operator<(const Candidate& rhs) const;
  // Returns the last activity time of this Candidate.
  base::TimeTicks LastActivityTime() const;

  LifecycleUnit* lifecycle_unit() { return lifecycle_unit_; }
  const LifecycleUnit* lifecycle_unit() const { return lifecycle_unit_; }
  const arc::ArcProcess* app() const { return app_; }
  ProcessType process_type() const { return process_type_; }
 private:
  // Derive process type for this candidate. Used to initialize |process_type_|.
  ProcessType GetProcessTypeInternal() const;

  LifecycleUnit* lifecycle_unit_ = nullptr;
  const arc::ArcProcess* app_ = nullptr;
  ProcessType process_type_ = GetProcessTypeInternal();
  DISALLOW_COPY_AND_ASSIGN(Candidate);
};

// A thin wrapper over library process_metric.h to get memory status so unit
// test get a chance to mock out.
class TabManagerDelegate::MemoryStat {
 public:
  MemoryStat() {}
  virtual ~MemoryStat() {}

  // Returns target size of memory to free given current memory pressure and
  // pre-configured low memory margin.
  virtual int TargetMemoryToFreeKB();

  // Returns estimated memory to be freed if the process |handle| is killed.
  virtual int EstimatedMemoryFreedKB(base::ProcessHandle handle);

 private:
  // Returns the low memory margin system config. Low memory condition is
  // reported if available memory is under the number.
  static int LowMemoryMarginKB();

  // Reads in an integer.
  static int ReadIntFromFile(const char* file_name, int default_val);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_DELEGATE_CHROMEOS_H_
