// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/tracing/trace_test_utils.h"

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/trace_session_observer.h"
#include "base/tracing/perfetto_platform.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace base::test {

TracingEnvironment::TracingEnvironment(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // The tracing service shouldn't have initialized Perfetto in this process,
  // because it's not safe to consume trace data from arbitrary processes
  // through TraceLog as the JSON conversion here isn't sandboxed like with the
  // real tracing service.
  CHECK(!perfetto::Tracing::IsInitialized() ||
        base::trace_event::IsPerfettoInitializedForTesting());

  if (perfetto::Tracing::IsInitialized()) {
    return;
  }
  if (!task_runner) {
    task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  }
  CHECK(task_runner);
  base::tracing::PerfettoPlatform::MaybeCreateInstance();
  base::tracing::PerfettoPlatform::Get().SetupForTesting(task_runner);
  base::trace_event::TraceSessionObserverList::Initialize();
  base::trace_event::SetPerfettoInitializedForTesting();

  base::trace_event::InitializeInProcessPerfettoBackend(
      &base::tracing::PerfettoPlatform::Get());
}

TracingEnvironment::~TracingEnvironment() {
  trace_event::TraceLog::ResetForTesting();
  perfetto::Tracing::ResetForTesting();
}

}  // namespace base::test
