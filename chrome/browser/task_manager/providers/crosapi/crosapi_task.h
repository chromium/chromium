// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_CROSAPI_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_CROSAPI_TASK_H_

#include "chrome/browser/task_manager/providers/task.h"

#include "chromeos/crosapi/mojom/task_manager.mojom.h"

namespace task_manager {

// Defines the task that represents the one running in lacros and retrieved
// via crosapi. This class holds cached metadata for a task running in Lacros.
class CrosapiTask : public Task {
 public:
  explicit CrosapiTask(const crosapi::mojom::TaskPtr& mojo_task);
  CrosapiTask(const CrosapiTask&) = delete;
  CrosapiTask& operator=(const CrosapiTask&) = delete;
  ~CrosapiTask() override;

  // task_manager::Task:
  void Activate() override;
  void Refresh(const base::TimeDelta& update_interval,
               int64_t refresh_flags) override;
  Type GetType() const override;
  std::u16string GetProfileName() const override;
  int GetChildProcessUniqueID() const override;
  int64_t GetSqliteMemoryUsed() const override;
  int64_t GetV8MemoryAllocated() const override;
  int64_t GetV8MemoryUsed() const override;
  int GetKeepaliveCount() const override;
  int64_t GetNetworkUsageRate() const override;
  int64_t GetCumulativeNetworkUsage() const override;
  bool ReportsWebCacheStats() const override;
  blink::WebCacheResourceTypeStats GetWebCacheStats() const override;

  // Updates task with |mojo_task|.
  void Update(const crosapi::mojom::TaskPtr& mojo_task);

 private:
  // Cached mojo task received via crosapi.
  crosapi::mojom::TaskPtr mojo_task_;
};

}  // namespace task_manager

#endif  //  CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_CROSAPI_TASK_H_
