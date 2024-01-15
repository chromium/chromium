// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/arc_shared_sampler.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "content/public/browser/browser_thread.h"

namespace task_manager {

namespace {

enum MemoryDumpType {
  kAppMemoryDump = 1 << 0,
  kSystemMemoryDump = 1 << 1,
};

}  // namespace

ArcSharedSampler::ArcSharedSampler() = default;

ArcSharedSampler::~ArcSharedSampler() = default;

void ArcSharedSampler::RegisterCallback(
    base::ProcessId process_id,
    OnSamplingCompleteCallback on_sampling_complete) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(process_id, base::kNullProcessId);
  const bool result =
      callbacks_.emplace(process_id, std::move(on_sampling_complete)).second;
  DCHECK(result);
}

void ArcSharedSampler::UnregisterCallback(base::ProcessId process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.erase(process_id);
}

void ArcSharedSampler::Refresh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service)
    return;

  auto is_pending_type = [this](MemoryDumpType type) -> bool {
    return pending_memory_dump_types_ & type;
  };

  const auto now = base::Time::Now();
  if (!is_pending_type(MemoryDumpType::kAppMemoryDump) &&
      now - last_app_refresh_ >=
          arc::ArcProcessService::kProcessSnapshotRefreshTime) {
    arc_process_service->RequestAppMemoryInfo(base::BindOnce(
        &ArcSharedSampler::OnReceiveMemoryDump, weak_ptr_factory_.GetWeakPtr(),
        MemoryDumpType::kAppMemoryDump));
    pending_memory_dump_types_ |= MemoryDumpType::kAppMemoryDump;
  }

  if (!is_pending_type(MemoryDumpType::kSystemMemoryDump) &&
      now - last_system_refresh_ >=
          arc::ArcProcessService::kProcessSnapshotRefreshTime) {
    arc_process_service->RequestSystemMemoryInfo(base::BindOnce(
        &ArcSharedSampler::OnReceiveMemoryDump, weak_ptr_factory_.GetWeakPtr(),
        MemoryDumpType::kSystemMemoryDump));
    pending_memory_dump_types_ |= MemoryDumpType::kSystemMemoryDump;
  }
}

void ArcSharedSampler::OnReceiveMemoryDump(
    int type,
    std::vector<arc::mojom::ArcMemoryDumpPtr> process_dump) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_memory_dump_types_ &= ~type;

  const auto now = base::Time::Now();
  if (type == MemoryDumpType::kAppMemoryDump)
    last_app_refresh_ = now;
  else
    last_system_refresh_ = now;

  for (const auto& proc : process_dump) {
    auto it = callbacks_.find(proc->pid);
    if (it == callbacks_.end())
      continue;
    const MemoryFootprintBytes result = proc->private_footprint_kb * 1024;
    it->second.Run(std::make_optional<MemoryFootprintBytes>(result));
  }
}

}  // namespace task_manager
