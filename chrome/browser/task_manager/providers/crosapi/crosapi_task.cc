// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crosapi/crosapi_task.h"

#include "base/strings/utf_string_conversions.h"

namespace {

constexpr char kCrosapiTaskTitlePrefix[] = "Lacros: ";

std::u16string GetCrosapiTaskTitle(const std::u16string& mojo_task_title) {
  return base::UTF8ToUTF16(kCrosapiTaskTitlePrefix) + mojo_task_title;
}

task_manager::Task::Type FromMojo(crosapi::mojom::TaskType mojo_type) {
  switch (mojo_type) {
    case crosapi::mojom::TaskType::kBrowser:
      return task_manager::Task::BROWSER;
    case crosapi::mojom::TaskType::kGpu:
      return task_manager::Task::GPU;
    case crosapi::mojom::TaskType::kZygote:
      return task_manager::Task::ZYGOTE;
    case crosapi::mojom::TaskType::kUtility:
      return task_manager::Task::UTILITY;
    case crosapi::mojom::TaskType::kRenderer:
      return task_manager::Task::RENDERER;
    case crosapi::mojom::TaskType::kExtension:
      return task_manager::Task::EXTENSION;
    case crosapi::mojom::TaskType::kGuest:
      return task_manager::Task::GUEST;
    case crosapi::mojom::TaskType::kPlugin:
      return task_manager::Task::PLUGIN;
    case crosapi::mojom::TaskType::kNacl:
      return task_manager::Task::NACL;
    case crosapi::mojom::TaskType::kSandboxHelper:
      return task_manager::Task::SANDBOX_HELPER;
    case crosapi::mojom::TaskType::kDedicatedWorker:
      return task_manager::Task::DEDICATED_WORKER;
    case crosapi::mojom::TaskType::kSharedWorker:
      return task_manager::Task::SHARED_WORKER;
    case crosapi::mojom::TaskType::kServiceWorker:
      return task_manager::Task::SERVICE_WORKER;
    case crosapi::mojom::TaskType::kUnknown:
      return task_manager::Task::UNKNOWN;
  }
}

blink::WebCacheResourceTypeStat FromMojo(
    const crosapi::mojom::WebCacheResourceTypeStatPtr& mojo_stat) {
  return blink::WebCacheResourceTypeStat{mojo_stat->count, mojo_stat->size,
                                         mojo_stat->decoded_size};
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
           mojo_task->process_id, /* process handle, which is the same as pid on
                                     POSIX */
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
  return mojo_task_->web_cache_stats ? true : false;
}

blink::WebCacheResourceTypeStats CrosapiTask::GetWebCacheStats() const {
  return FromMojo(mojo_task_->web_cache_stats);
}

void CrosapiTask::Refresh(const base::TimeDelta& update_interval,
                          int64_t refresh_flags) {}

void CrosapiTask::Update(const crosapi::mojom::TaskPtr& mojo_task) {
  DCHECK_EQ(mojo_task_->task_uuid, mojo_task->task_uuid);
  DCHECK_EQ(mojo_task_->type, mojo_task_->type);

  set_title(GetCrosapiTaskTitle(mojo_task->title));
  set_icon(mojo_task->icon);
  mojo_task_ = mojo_task.Clone();
}

}  // namespace task_manager
