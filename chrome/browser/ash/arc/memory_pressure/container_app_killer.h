// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_MEMORY_PRESSURE_CONTAINER_APP_KILLER_H_
#define CHROME_BROWSER_ASH_ARC_MEMORY_PRESSURE_CONTAINER_APP_KILLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"  // For FORWARD_DECLARE_TEST.
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"         // For WeakPtrFactory.
#include "base/process/process_handle.h"  // For ProcessId.
#include "base/time/time.h"               // For TimeTicks.
#include "chrome/browser/ash/arc/memory_pressure/container_oom_score_manager.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"

FORWARD_DECLARE_TEST(ContainerAppKillerTest, DoNotKillRecentlyKilled);
FORWARD_DECLARE_TEST(ContainerAppKillerTest, IsRecentlyKilled);
FORWARD_DECLARE_TEST(ContainerAppKillerTest, KillMultipleProcesses);
FORWARD_DECLARE_TEST(ContainerAppKillerTest, SortedCandidates);

namespace arc {

class ContainerAppKiller : public ash::ResourcedClient::ArcContainerObserver {
 public:
  ContainerAppKiller();
  ContainerAppKiller(const ContainerAppKiller&) = delete;
  ContainerAppKiller& operator=(const ContainerAppKiller&) = delete;
  ~ContainerAppKiller() override;

  // ash::ResourcedClient::ArcContainerObserver:
  void OnMemoryPressure(ash::ResourcedClient::PressureLevelArcContainer level,
                        uint64_t reclaim_target_kb) override;

 protected:
  // Returns if the ARC process is killed recently. The mock class in unittest
  // would override this method.
  virtual bool IsRecentlyKilled(const std::string& process_name,
                                const base::TimeTicks& now);

  // Returns the estimated memory usage of this process in KiB. The mock class
  // in unittest would override this method.
  virtual uint64_t GetMemoryFootprintKB(base::ProcessId pid);

  // Kills an ARC process. The mock class in unittest would override this
  // method.
  virtual bool KillArcProcess(base::ProcessId nspid);

 private:
  FRIEND_TEST_ALL_PREFIXES(::ContainerAppKillerTest, DoNotKillRecentlyKilled);
  FRIEND_TEST_ALL_PREFIXES(::ContainerAppKillerTest, IsRecentlyKilled);
  FRIEND_TEST_ALL_PREFIXES(::ContainerAppKillerTest, KillMultipleProcesses);
  FRIEND_TEST_ALL_PREFIXES(::ContainerAppKillerTest, SortedCandidates);

  static constexpr base::TimeDelta kArcRespawnKillDelay = base::Seconds(60);

  class KillCandidate;

  // Callback of RequestAppProcessList to kill ARC processes to reclaim memory.
  void LowMemoryKill(ash::ResourcedClient::PressureLevelArcContainer level,
                     uint64_t reclaim_target_kb,
                     ArcProcessService::OptionalArcProcessList arc_processes);

  // Gets the kill candidate list to. Lowest priority process first.
  static std::vector<KillCandidate> GetSortedCandidates(
      const ArcProcessService::OptionalArcProcessList& arc_processes);

  // A map from an ARC process name to a monotonic timestamp when it's killed.
  using KilledProcessesMap = base::flat_map<std::string, base::TimeTicks>;

  // Map maintaining ARC process names and their last killed time.
  KilledProcessesMap recently_killed_;

  // Assigns oom_score_adj to ARC container processes.
  ContainerOomScoreManager oom_score_manager_;

  // Weak pointer factory used for posting tasks to other threads.
  base::WeakPtrFactory<ContainerAppKiller> weak_ptr_factory_{this};
};

// Copying ArcProcess in sorting might be expensive. KillCandidate contains a
// pointer to ArcProcess and some cached fields to speed up comparison. Sorting
// KillCandidate objects is faster than sorting ArcProcess objects.
class ContainerAppKiller::KillCandidate {
 public:
  explicit KillCandidate(const arc::ArcProcess* process)
      : process_(process), protected_(process_->IsBackgroundProtected()) {}

  // Returns true if this candidate should be killed earlier than rhs.
  bool operator<(const KillCandidate& rhs) const {
    if (protected_ != rhs.protected_) {
      return !protected_;
    }
    return process_->last_activity_time() < rhs.process_->last_activity_time();
  }

  const std::string& process_name() const { return process_->process_name(); }

  base::ProcessId pid() const { return process_->pid(); }

  base::ProcessId nspid() const { return process_->nspid(); }

  bool is_protected() const { return protected_; }

 private:
  raw_ptr<const arc::ArcProcess> process_;

  // The process protection state is cached to speed up sorting.
  bool protected_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_MEMORY_PRESSURE_CONTAINER_APP_KILLER_H_
