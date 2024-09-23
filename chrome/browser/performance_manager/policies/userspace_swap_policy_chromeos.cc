// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/userspace_swap_policy_chromeos.h"

#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/mechanisms/userspace_swap_chromeos.h"
#include "chromeos/ash/components/memory/userspace_swap/swap_storage.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {
namespace policies {

namespace {
using ::ash::memory::userspace_swap::SwapFile;
using ::ash::memory::userspace_swap::UserspaceSwapConfig;

class UserspaceSwapPolicyData
    : public ExternalNodeAttachedDataImpl<UserspaceSwapPolicyData> {
 public:
  explicit UserspaceSwapPolicyData(const ProcessNode* node) {}
  ~UserspaceSwapPolicyData() override = default;

  static UserspaceSwapPolicyData* EnsureForProcess(
      const ProcessNode* process_node) {
    return UserspaceSwapPolicyData::GetOrCreate(process_node);
  }

  bool initialization_attempted_ = false;
  bool process_initialized_ = false;
  base::TimeTicks last_swap_;
};

constexpr base::TimeDelta kMetricsInterval = base::Seconds(30);

}  // namespace

UserspaceSwapPolicy::UserspaceSwapPolicy(const UserspaceSwapConfig& config)
    : config_(config) {
  // To avoid failures related to chromeos-linux, we validate that we're running
  // on chromeos before enforcing the following check.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    DCHECK(UserspaceSwapPolicy::UserspaceSwapSupportedAndEnabled());
  }

  if (VLOG_IS_ON(1) && !metrics_timer_->IsRunning()) {
    metrics_timer_->Start(
        FROM_HERE, kMetricsInterval,
        base::BindRepeating(&UserspaceSwapPolicy::PrintAllSwapMetrics,
                            weak_factory_.GetWeakPtr()));
  }
}

UserspaceSwapPolicy::UserspaceSwapPolicy()
    : UserspaceSwapPolicy(UserspaceSwapConfig::Get()) {}

UserspaceSwapPolicy::~UserspaceSwapPolicy() = default;

void UserspaceSwapPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddProcessNodeObserver(this);

  // Only handle the memory pressure notifications if the feature to swap on
  // moderate pressure is enabled.
  if (config_->swap_on_moderate_pressure) {
    graph->AddSystemNodeObserver(this);
  }
}

void UserspaceSwapPolicy::OnTakenFromGraph(Graph* graph) {
  if (config_->swap_on_moderate_pressure) {
    graph->RemoveSystemNodeObserver(this);
  }

  graph->RemoveProcessNodeObserver(this);
}

void UserspaceSwapPolicy::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  if (config_->swap_on_freeze) {
    // We don't provide a page node because the visibility requirements don't
    // matter on freeze.
    if (IsEligibleToSwap(process_node, nullptr)) {
      VLOG(1) << "rphid: " << process_node->GetRenderProcessHostId()
              << " pid: " << process_node->GetProcessId() << " swap on freeze";
      UserspaceSwapPolicyData::EnsureForProcess(process_node)->last_swap_ =
          base::TimeTicks::Now();
      SwapProcessNode(process_node);
    }
  }
}

void UserspaceSwapPolicy::OnProcessNodeAdded(const ProcessNode* process_node) {
  // If data was still associated with this node make sure it's blown away and
  // any existing file descriptors are closed.
  if (UserspaceSwapPolicyData::Destroy(process_node)) {
    DLOG(FATAL)
        << "ProcessNode had a UserspaceSwapPolicyData attached when added.";
  }
}

bool UserspaceSwapPolicy::InitializeProcessNode(
    const ProcessNode* process_node) {
  // TODO(bgeffon): Add policy specific initialization or remove once final CLs
  // land.
  return true;
}

void UserspaceSwapPolicy::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  if (!process_node->GetProcess().IsValid()) {
    return;
  }

  UserspaceSwapPolicyData* data =
      UserspaceSwapPolicyData::EnsureForProcess(process_node);
  if (!data->initialization_attempted_) {
    data->initialization_attempted_ = true;

    // If this fails we don't attempt swap ever.
    data->process_initialized_ = InitializeProcessNode(process_node);

    LOG_IF(ERROR, !data->process_initialized_)
        << "Unable to initialize process node";
  }
}

base::TimeTicks UserspaceSwapPolicy::GetLastSwapTime(
    const ProcessNode* process_node) {
  return UserspaceSwapPolicyData::EnsureForProcess(process_node)->last_swap_;
}

void UserspaceSwapPolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  if (new_level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  auto now_ticks = base::TimeTicks::Now();
  // Try not to walk the graph too frequently because we can receive moderate
  // memory pressure notifications every 10s.
  if (now_ticks - last_graph_walk_ < config_->graph_walk_frequency) {
    return;
  }

  last_graph_walk_ = now_ticks;
  SwapNodesOnGraph();
}

void UserspaceSwapPolicy::SwapNodesOnGraph() {
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    // Check that we have a main frame.
    const FrameNode* main_frame_node = page_node->GetMainFrameNode();
    if (!main_frame_node)
      continue;

    const ProcessNode* process_node = main_frame_node->GetProcessNode();
    if (IsEligibleToSwap(process_node, page_node)) {
      VLOG(1) << "rphid: " << process_node->GetRenderProcessHostId()
              << " pid: " << process_node->GetProcessId()
              << " trigger swap for frame " << main_frame_node->GetURL();
      UserspaceSwapPolicyData::EnsureForProcess(process_node)->last_swap_ =
          base::TimeTicks::Now();
      SwapProcessNode(process_node);
    }
  }
}

void UserspaceSwapPolicy::PrintAllSwapMetrics() {
  uint64_t total_reclaimed = 0;
  uint64_t total_on_disk = 0;
  uint64_t total_renderers = 0;
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    const FrameNode* main_frame_node = page_node->GetMainFrameNode();
    if (!main_frame_node) {
      continue;
    }

    const ProcessNode* process_node = main_frame_node->GetProcessNode();

    auto now_ticks = base::TimeTicks::Now();
    if (process_node && process_node->GetProcess().IsValid()) {
      bool is_visible = page_node->IsVisible();
      auto last_visibility_change =
          page_node->GetTimeSinceLastVisibilityChange();
      auto url = main_frame_node->GetURL();

      uint64_t memory_reclaimed = GetProcessNodeReclaimedBytes(process_node);
      uint64_t disk_space_used = GetProcessNodeSwapFileUsageBytes(process_node);
      total_on_disk += disk_space_used;
      total_reclaimed += memory_reclaimed;
      total_renderers++;

      VLOG(1) << "Frame " << url << " visibile: " << is_visible
              << " last_chg: " << last_visibility_change
              << " last_swap: " << (now_ticks - GetLastSwapTime(process_node))
              << " reclaimed: " << (memory_reclaimed >> 10) << "Kb"
              << " on disk: " << (disk_space_used >> 10) << "Kb";
    }
  }

  VLOG(1) << "Swap Summary, Renderers: " << total_renderers
          << " reclaimed: " << (total_reclaimed >> 10)
          << "Kb, total on disk: " << (total_on_disk >> 10) << "Kb"
          << " Backing Store free space: "
          << (GetSwapDeviceFreeSpaceBytes() >> 10) << "Kb";
}

void UserspaceSwapPolicy::SwapProcessNode(const ProcessNode* process_node) {
  performance_manager::mechanism::userspace_swap::SwapProcessNode(process_node);
}

uint64_t UserspaceSwapPolicy::GetProcessNodeSwapFileUsageBytes(
    const ProcessNode* process_node) {
  return performance_manager::mechanism::userspace_swap::
      GetProcessNodeSwapFileUsageBytes(process_node);
}

uint64_t UserspaceSwapPolicy::GetProcessNodeReclaimedBytes(
    const ProcessNode* process_node) {
  return performance_manager::mechanism::userspace_swap::
      GetProcessNodeReclaimedBytes(process_node);
}

uint64_t UserspaceSwapPolicy::GetTotalSwapFileUsageBytes() {
  return performance_manager::mechanism::userspace_swap::
      GetTotalSwapFileUsageBytes();
}

uint64_t UserspaceSwapPolicy::GetSwapDeviceFreeSpaceBytes() {
  return performance_manager::mechanism::userspace_swap::
      GetSwapDeviceFreeSpaceBytes();
}

bool UserspaceSwapPolicy::IsPageNodeLoadingOrBusy(const PageNode* page_node) {
  const PageNode::LoadingState loading_state = page_node->GetLoadingState();
  return loading_state == PageNode::LoadingState::kLoading ||
         loading_state == PageNode::LoadingState::kLoadedBusy;
}

bool UserspaceSwapPolicy::IsPageNodeAudible(const PageNode* page_node) {
  return page_node->IsAudible();
}

bool UserspaceSwapPolicy::IsPageNodeVisible(const PageNode* page_node) {
  return page_node->IsVisible();
}

base::TimeDelta UserspaceSwapPolicy::GetTimeSinceLastVisibilityChange(
    const PageNode* page_node) {
  return page_node->GetTimeSinceLastVisibilityChange();
}

bool UserspaceSwapPolicy::IsEligibleToSwap(const ProcessNode* process_node,
                                           const PageNode* page_node) {
  if (!process_node || !process_node->GetProcess().IsValid()) {
    LOG(ERROR) << "Process node not valid";
    return false;
  }

  auto* data = UserspaceSwapPolicyData::EnsureForProcess(process_node);
  if (!data->process_initialized_) {
    return false;
  }

  // Always check with the mechanism to make sure that it can still be swapped
  // and that nothing unexpected has happened.
  if (!performance_manager::mechanism::userspace_swap::IsEligibleToSwap(
          process_node)) {
    return false;
  }

  auto now_ticks = base::TimeTicks::Now();
  // Don't swap a renderer too frequently.
  auto time_since_last_swap = now_ticks - GetLastSwapTime(process_node);
  if (time_since_last_swap < config_->process_swap_frequency) {
    return false;
  }

  // If the caller provided a PageNode we will validate the visibility state of
  // it.
  if (page_node) {
    // If we're loading, audible, or visible we will not swap.
    if (IsPageNodeLoadingOrBusy(page_node) || IsPageNodeVisible(page_node) ||
        IsPageNodeAudible(page_node)) {
      return false;
    }

    // Next the page node must have been invisible for longer than the
    // configured time.
    if (GetTimeSinceLastVisibilityChange(page_node) <
        config_->invisible_time_before_swap) {
      return false;
    }
  }

  // To avoid hammering the system with fstat(2) system calls we will cache the
  // available disk space for 30 seconds. But we only check if it's been
  // configured to enforce a swap device minimum.
  if (config_->minimum_swap_disk_space_available > 0) {
    // Check if we can't swap because the device is running low on space.
    if (GetSwapDeviceFreeSpaceBytes() <
        config_->minimum_swap_disk_space_available) {
      return false;
    }
  }

  // Make sure we're not exceeding the total swap file usage across all
  // renderers.
  if (config_->maximum_swap_disk_space_bytes > 0) {
    if (GetTotalSwapFileUsageBytes() >=
        config_->maximum_swap_disk_space_bytes) {
      return false;
    }
  }

  // And make sure we're not exceeding the per-renderer swap file limit.
  if (config_->renderer_maximum_disk_swap_file_size_bytes > 0) {
    if (GetProcessNodeSwapFileUsageBytes(process_node) >=
        config_->renderer_maximum_disk_swap_file_size_bytes) {
      return false;
    }
  }

  return true;
}

// Static
bool UserspaceSwapPolicy::UserspaceSwapSupportedAndEnabled() {
  return ash::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
}

}  // namespace policies
}  // namespace performance_manager
