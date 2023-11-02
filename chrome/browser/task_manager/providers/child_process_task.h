// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CHILD_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CHILD_PROCESS_TASK_H_

#include <stdint.h>

#include <memory>

#include "chrome/browser/task_manager/providers/task.h"

class ProcessResourceUsage;

namespace content {
struct ChildProcessData;
}  // namespace content

namespace task_manager {

// Represents several types of the browser's child processes such as
// a plugin or a GPU process, ... etc.
class ChildProcessTask : public Task {
 public:
  // ChildProcessData has a ProcessType but that's not always granular enough to
  // correctly determine what string to show as the name of the task. This enum
  // is used to provide that information.
  enum class ProcessSubtype {
    kNoSubtype,
    // The "spare" render process, a render process used so that there is always
    // a render process ready to go.
    kSpareRenderProcess,
    // A render process that is unknown and for which no provider is available.
    // Should not be used; all processes should be shown in the Task Manager.
    // See https://crbug.com/739782 .
    kUnknownRenderProcess,
  };

  // Creates a child process task given its |data| which is
  // received from observing |content::BrowserChildProcessObserver|.
  ChildProcessTask(const content::ChildProcessData& data,
                   ProcessSubtype subtype);

  ChildProcessTask(const ChildProcessTask&) = delete;
  ChildProcessTask& operator=(const ChildProcessTask&) = delete;
  ~ChildProcessTask() override;

  // task_manager::Task:
  void Refresh(const base::TimeDelta& update_interval,
               int64_t refresh_flags) override;
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;
  int64_t GetV8MemoryAllocated() const override;
  int64_t GetV8MemoryUsed() const override;

 private:
  static gfx::ImageSkia* s_icon_;

  // The Mojo service wrapper that will provide us with the V8 memory usage of
  // the browser child process represented by this object.
  std::unique_ptr<ProcessResourceUsage> process_resources_sampler_;

  // The allocated and used V8 memory (in bytes).
  int64_t v8_memory_allocated_;
  int64_t v8_memory_used_;

  // The unique ID of the child process. It is not the PID of the process.
  // See |content::ChildProcessData::id|.
  const int unique_child_process_id_;

  // The type of the child process. See |content::ProcessType| and
  // |NaClTrustedProcessType|.
  const int process_type_;

  // Depending on the |process_type_|, determines whether this task uses V8
  // memory or not.
  const bool uses_v8_memory_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CHILD_PROCESS_TASK_H_
