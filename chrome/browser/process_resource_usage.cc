// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_resource_usage.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"

ProcessResourceUsage::ProcessResourceUsage(
    mojo::PendingRemote<content::mojom::ResourceUsageReporter> service)
    : service_(std::move(service)), update_in_progress_(false) {
  service_.set_disconnect_handler(
      base::BindOnce(&ProcessResourceUsage::RunPendingRefreshCallbacks,
                     base::Unretained(this)));
}

ProcessResourceUsage::~ProcessResourceUsage() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProcessResourceUsage::RunPendingRefreshCallbacks() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  base::circular_deque<base::OnceClosure> callbacks;
  std::swap(callbacks, refresh_callbacks_);
  for (auto& callback : callbacks)
    task_runner->PostTask(FROM_HERE, std::move(callback));
}

void ProcessResourceUsage::Refresh(base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!service_ || !service_.is_connected()) {
    if (!callback.is_null())
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
    return;
  }

  if (!callback.is_null())
    refresh_callbacks_.push_back(std::move(callback));

  if (!update_in_progress_) {
    update_in_progress_ = true;
    service_->GetUsageData(base::BindOnce(&ProcessResourceUsage::OnRefreshDone,
                                          base::Unretained(this)));
  }
}

void ProcessResourceUsage::OnRefreshDone(
    content::mojom::ResourceUsageDataPtr data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_in_progress_ = false;
  stats_ = std::move(data);
  RunPendingRefreshCallbacks();
}

bool ProcessResourceUsage::ReportsV8MemoryStats() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_)
    return stats_->reports_v8_stats;
  return false;
}

size_t ProcessResourceUsage::GetV8MemoryAllocated() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->reports_v8_stats)
    return stats_->v8_bytes_allocated;
  return 0;
}

size_t ProcessResourceUsage::GetV8MemoryUsed() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->reports_v8_stats)
    return stats_->v8_bytes_used;
  return 0;
}

blink::WebCacheResourceTypeStats
ProcessResourceUsage::GetBlinkMemoryCacheStats() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->web_cache_stats)
    return stats_->web_cache_stats->To<blink::WebCacheResourceTypeStats>();
  return {};
}
