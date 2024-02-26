// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/memory_pressure/container_app_killer.h"

#include <algorithm>  // For sort.
#include <vector>

#include "ash/components/arc/arc_util.h"             // For IsArcVmEnabled.
#include "ash/components/arc/mojom/process.mojom.h"  // For arc::mojom::ProcessInstance.
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/logging.h"                 // For LOG.
#include "base/process/process_metrics.h"
#include "chrome/browser/memory/memory_kills_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

ContainerAppKiller::ContainerAppKiller() {
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (client) {
    client->AddArcContainerObserver(this);
  }
}

ContainerAppKiller::~ContainerAppKiller() {
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (client) {
    client->RemoveArcContainerObserver(this);
  }
}

void ContainerAppKiller::OnMemoryPressure(
    ash::ResourcedClient::PressureLevelArcContainer level,
    uint64_t reclaim_target_kb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // ContainerAppKiller shall not be created when ARCVM is enabled.
  DCHECK(!arc::IsArcVmEnabled());

  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service) {
    return;
  }

  // RequestAppProcessList() must be called in UI thread.
  arc_process_service->RequestAppProcessList(
      base::BindOnce(&ContainerAppKiller::LowMemoryKill,
                     weak_ptr_factory_.GetWeakPtr(), level, reclaim_target_kb));
}

void ContainerAppKiller::LowMemoryKill(
    ash::ResourcedClient::PressureLevelArcContainer level,
    uint64_t reclaim_target_kb,
    ArcProcessService::OptionalArcProcessList arc_processes) {
  if (!arc_processes) {
    return;
  }

  // Don't kill persistent and focused ARC processes.
  std::erase_if(*arc_processes, [](auto& proc) {
    return proc.IsPersistent() || proc.is_focused();
  });

  bool is_critical_level =
      (level == ash::ResourcedClient::PressureLevelArcContainer::kPerceptible ||
       level == ash::ResourcedClient::PressureLevelArcContainer::kForeground);

  // Don't kill background protected ARC processes on non-critical level.
  if (!is_critical_level) {
    std::erase_if(*arc_processes,
                  [](auto& proc) { return proc.IsBackgroundProtected(); });
  }

  if (arc_processes->empty()) {
    return;
  }

  std::vector<KillCandidate> candidates = GetSortedCandidates(arc_processes);

  base::TimeTicks now = base::TimeTicks::Now();

  uint64_t total_freed_kb = 0;

  for (KillCandidate& candidate : candidates) {
    // If it's in critical memory pressure level, all non-protected processes
    // should be freed, it would keep reclaiming until the candidate is
    // protected.
    if ((reclaim_target_kb <= total_freed_kb) &&
        (!is_critical_level || candidate.is_protected())) {
      break;
    }

    if (IsRecentlyKilled(candidate.process_name(), now)) {
      LOG(WARNING) << "Avoided killing " << candidate.process_name()
                   << " too often";
      continue;
    }

    // Estimate the process memory usage before it's killed.
    int estimated_memory_freed_kb = GetMemoryFootprintKB(candidate.pid());

    if (KillArcProcess(candidate.nspid())) {
      recently_killed_[candidate.process_name()] = now;

      total_freed_kb += estimated_memory_freed_kb;

      memory::MemoryKillsMonitor::LogLowMemoryKill("APP",
                                                   estimated_memory_freed_kb);
      LOG(WARNING) << "Killed " << candidate.process_name()
                   << ", pid: " << candidate.pid()
                   << ", protected: " << candidate.is_protected() << ", "
                   << estimated_memory_freed_kb << " KB freed";
    } else {
      LOG(ERROR) << "Failed to kill " << candidate.process_name()
                 << ", pid:" << candidate.pid()
                 << ", protected: " << candidate.is_protected();
    }
  }

  if (is_critical_level) {
    LOG(WARNING) << "Reclaim target(KB): " << reclaim_target_kb
                 << ", estimated total freed(KB): " << total_freed_kb
                 << " (All non-protected apps are reclaimed)";
  } else {
    LOG(WARNING) << "Reclaim target(KB): " << reclaim_target_kb
                 << ", estimated total freed(KB): " << total_freed_kb;
  }
}

std::vector<ContainerAppKiller::KillCandidate>
ContainerAppKiller::GetSortedCandidates(
    const ArcProcessService::OptionalArcProcessList& arc_processes) {
  std::vector<KillCandidate> candidates;

  for (const auto& process : *arc_processes) {
    candidates.emplace_back(&process);
  }

  std::sort(candidates.begin(), candidates.end());

  return candidates;
}

bool ContainerAppKiller::IsRecentlyKilled(const std::string& process_name,
                                          const base::TimeTicks& now) {
  const auto it = recently_killed_.find(process_name);
  if (it == recently_killed_.end()) {
    return false;
  }
  return (now - it->second) <= kArcRespawnKillDelay;
}

uint64_t ContainerAppKiller::GetMemoryFootprintKB(base::ProcessId pid) {
  auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(pid);
  return (process_metrics->GetVmSwapBytes() +
          process_metrics->GetResidentSetSize()) /
         1024;
}

bool ContainerAppKiller::KillArcProcess(base::ProcessId nspid) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return false;
  }

  auto* arc_process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->process(), KillProcess);
  if (!arc_process_instance) {
    return false;
  }

  arc_process_instance->KillProcess(nspid, "LowMemoryKill");
  return true;
}

}  // namespace arc
