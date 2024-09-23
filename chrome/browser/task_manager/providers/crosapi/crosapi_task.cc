// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crosapi/crosapi_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"

namespace {

constexpr char16_t kCrosapiTaskTitlePrefix[] = u"Lacros: ";

std::u16string GetCrosapiTaskTitle(const std::u16string& mojo_task_title) {
  return kCrosapiTaskTitlePrefix + mojo_task_title;
}

blink::WebCacheResourceTypeStat FromMojo(
    const crosapi::mojom::WebCacheResourceTypeStatPtr& mojo_stat) {
  return blink::WebCacheResourceTypeStat{
      static_cast<size_t>(mojo_stat->count),
      static_cast<size_t>(mojo_stat->size),
      static_cast<size_t>(mojo_stat->decoded_size)};
}

blink::WebCacheResourceTypeStats FromMojo(
    const crosapi::mojom::WebCacheResourceTypeStatsPtr& mojo_stats) {
  return blink::WebCacheResourceTypeStats{
      FromMojo(mojo_stats->images),  FromMojo(mojo_stats->css_style_sheets),
      FromMojo(mojo_stats->scripts), FromMojo(mojo_stats->xsl_style_sheets),
      FromMojo(mojo_stats->fonts),   FromMojo(mojo_stats->other)};
}

}  // namespace

namespace task_manager {

CrosapiTask::CrosapiTask(const crosapi::mojom::TaskPtr& mojo_task)
    : Task(GetCrosapiTaskTitle(mojo_task->title),
           &mojo_task->icon,
           mojo_task->process_id,  // process handle, which is the same as pid
                                   // on POSIX
           mojo_task->process_id),
      // cache the |mojo_task| received from crosapi.
      mojo_task_(mojo_task.Clone()) {}

CrosapiTask::~CrosapiTask() = default;

Task::Type CrosapiTask::GetType() const {
  return Task::LACROS;
}

std::u16string CrosapiTask::GetProfileName() const {
  return mojo_task_->profile_name;
}

int CrosapiTask::GetChildProcessUniqueID() const {
  return 0;
}

int64_t CrosapiTask::GetSqliteMemoryUsed() const {
  return mojo_task_->used_sqlite_memory;
}

int64_t CrosapiTask::GetV8MemoryAllocated() const {
  return mojo_task_->v8_memory_allocated;
}

int64_t CrosapiTask::GetV8MemoryUsed() const {
  return mojo_task_->v8_memory_used;
}

int CrosapiTask::GetKeepaliveCount() const {
  return mojo_task_->keep_alive_count;
}

int64_t CrosapiTask::GetNetworkUsageRate() const {
  return mojo_task_->network_usage_rate;
}

int64_t CrosapiTask::GetCumulativeNetworkUsage() const {
  return mojo_task_->cumulative_network_usage;
}

bool CrosapiTask::ReportsWebCacheStats() const {
  return !!mojo_task_->web_cache_stats;
}

blink::WebCacheResourceTypeStats CrosapiTask::GetWebCacheStats() const {
  return FromMojo(mojo_task_->web_cache_stats);
}

void CrosapiTask::Activate() {
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->task_manager_ash()
        ->ActivateTask(mojo_task_->task_uuid);
  }
}

void CrosapiTask::Refresh(const base::TimeDelta& update_interval,
                          int64_t refresh_flags) {}

void CrosapiTask::Update(const crosapi::mojom::TaskPtr& task) {
  DCHECK_EQ(mojo_task_->task_uuid, task->task_uuid);
  DCHECK_EQ(mojo_task_->type, task->type);

  set_title(GetCrosapiTaskTitle(task->title));
  set_icon(task->icon);
  mojo_task_ = task.Clone();
}

}  // namespace task_manager
