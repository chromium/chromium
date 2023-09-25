// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/memory_pressure/container_oom_score_manager.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/process/process_handle.h"  // For ProcessId.
#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "content/public/common/content_constants.h"  // For kLowestRendererOomScore.

namespace arc {

namespace {

constexpr base::TimeDelta kOomScoreAssignmentInterval = base::Seconds(10);

// Distribute the oom_score_adj to processes in the [oom_score_adj_low,
// oom_score_adj_high) range. The results are saved to the pid to oom_score_adj
// map.
void DistributeOomScoreInRange(
    const std::vector<const arc::ArcProcess*>& processes,
    int32_t oom_score_adj_low,
    int32_t oom_score_adj_high,
    base::flat_map<base::ProcessId, int32_t>& new_map) {
  const size_t process_count = processes.size();
  if (process_count == 0) {
    return;
  }

  const float range =
      static_cast<float>(oom_score_adj_high - oom_score_adj_low);
  const float adj_increment = range / process_count;
  float adj_raw = oom_score_adj_low;

  for (const auto* process : processes) {
    const int32_t adj = round(adj_raw);

    base::ProcessId pid = process->pid();
    if (pid != base::kNullProcessHandle && new_map.find(pid) == new_map.end()) {
      new_map[pid] = adj;
    }
    adj_raw += adj_increment;
  }
}

void OnSetOomScoreAdj(bool success, const std::string& output) {
  if (!success) {
    LOG(ERROR) << "Set OOM score error: " << output;
  }
}

// Returns true if lhs is newer process.
bool IsNewerProcess(const arc::ArcProcess* lhs, const arc::ArcProcess* rhs) {
  return lhs->last_activity_time() > rhs->last_activity_time();
}

}  // namespace

ContainerOomScoreManager::ContainerOomScoreManager()
    : ContainerOomScoreManager(false) {}

ContainerOomScoreManager::ContainerOomScoreManager(bool testing)
    : testing_(testing) {
  if (!testing) {
    timer_.Start(FROM_HERE, kOomScoreAssignmentInterval,
                 base::BindRepeating(&ContainerOomScoreManager::OnTimer,
                                     weak_ptr_factory_.GetWeakPtr()));
  }
}

ContainerOomScoreManager::~ContainerOomScoreManager() = default;

void ContainerOomScoreManager::OnTimer() {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service) {
    return;
  }

  arc_process_service->RequestAppProcessList(
      base::BindOnce(&ContainerOomScoreManager::AssignOomScoreAdjs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContainerOomScoreManager::AssignOomScoreAdjs(
    ArcProcessService::OptionalArcProcessList arc_processes) {
  if (!arc_processes) {
    return;
  }

  std::vector<const arc::ArcProcess*> protected_processes;
  std::vector<const arc::ArcProcess*> other_processes;

  ProcessScoreMap new_map;

  for (const auto& process : *arc_processes) {
    if (process.IsPersistent()) {
      new_map[process.pid()] = kPersistentArcAppOomScore;
    } else if (process.is_focused()) {
      new_map[process.pid()] = kFocusedArcAppOomScore;
    } else if (process.IsBackgroundProtected()) {
      protected_processes.emplace_back(&process);
    } else {
      other_processes.emplace_back(&process);
    }
  }

  int32_t mid_oom_score =
      (content::kLowestRendererOomScore + content::kHighestRendererOomScore) /
      2;

  // Sorts the process lists according to last_activity_time. Newer process
  // (larger last_activity_time) first.
  std::sort(protected_processes.begin(), protected_processes.end(),
            IsNewerProcess);
  std::sort(other_processes.begin(), other_processes.end(), IsNewerProcess);

  DistributeOomScoreInRange(protected_processes,
                            content::kLowestRendererOomScore, mid_oom_score,
                            new_map);
  DistributeOomScoreInRange(other_processes, mid_oom_score,
                            content::kHighestRendererOomScore, new_map);

  SetOomScoreAdjs(new_map);
}

void ContainerOomScoreManager::SetOomScoreAdjs(const ProcessScoreMap& new_map) {
  std::map<base::ProcessId, int32_t> oom_scores_to_change;

  for (auto it = new_map.begin(); it != new_map.end(); ++it) {
    auto pid = it->first;
    auto score = it->second;
    if (GetCachedOomScore(pid) != score) {
      oom_scores_to_change[pid] = score;
    }
  }

  if (oom_scores_to_change.size() && !testing_) {
    ash::DebugDaemonClient::Get()->SetOomScoreAdj(
        oom_scores_to_change, base::BindOnce(&OnSetOomScoreAdj));
  }

  oom_score_map_ = std::move(new_map);
}

int32_t ContainerOomScoreManager::GetCachedOomScore(base::ProcessId pid) {
  auto it = oom_score_map_.find(pid);
  if (it == oom_score_map_.end()) {
    return -1;
  }
  return it->second;
}

}  // namespace arc
