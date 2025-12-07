// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/trace_test_utils.h"

#include "base/check.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_log.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace base::test {

TracingEnvironment::TracingEnvironment() {
  InitializeTracing();
}

TracingEnvironment::~TracingEnvironment() {
  trace_event::TraceLog::ResetForTesting();
  perfetto::Tracing::ResetForTesting();
}

void InitializeTracing() {
  // The tracing service shouldn't have initialized Perfetto in this process,
  // because it's not safe to consume trace data from arbitrary processes
  // through TraceLog as the JSON conversion here isn't sandboxed like with the
  // real tracing service.
  CHECK(!perfetto::Tracing::IsInitialized() ||
        base::trace_event::IsPerfettoInitializedForTesting());

  if (perfetto::Tracing::IsInitialized()) {
    return;
  }
  base::trace_event::SetPerfettoInitializedForTesting();
  base::trace_event::InitializeInProcessPerfettoBackend();
}

void SetupTracing() {
  base::trace_event::InitializeInProcessPerfettoBackend();
  perfetto::Tracing::ResetForTesting();
}

}  // namespace base::test
