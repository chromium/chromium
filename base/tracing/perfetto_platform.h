// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACING_PERFETTO_PLATFORM_H_
#define BASE_TRACING_PERFETTO_PLATFORM_H_

#include "third_party/perfetto/include/perfetto/tracing/platform.h"

#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_local_storage.h"

namespace base {
class DeferredSequencedTaskRunner;

namespace tracing {

class BASE_EXPORT PerfettoPlatform : public perfetto::Platform {
 public:
  PerfettoPlatform();
  ~PerfettoPlatform() override;

  SequencedTaskRunner* task_runner() const;
  bool did_start_task_runner() const { return did_start_task_runner_; }
  void StartTaskRunner(scoped_refptr<SequencedTaskRunner>);

  // perfetto::Platform implementation:
  ThreadLocalObject* GetOrCreateThreadLocalObject() override;
  std::unique_ptr<perfetto::base::TaskRunner> CreateTaskRunner(
      const CreateTaskRunnerArgs&) override;
  std::string GetCurrentProcessName() override;

 private:
  scoped_refptr<DeferredSequencedTaskRunner> deferred_task_runner_;
  bool did_start_task_runner_ = false;
  ThreadLocalStorage::Slot thread_local_object_;
};

}  // namespace tracing
}  // namespace base

#endif  // BASE_TRACING_PERFETTO_PLATFORM_H_
