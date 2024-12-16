// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/trace_test_utils.h"

#include "base/trace_event/trace_log.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace base {
namespace test {

TracingEnvironment::TracingEnvironment() {
  trace_event::TraceLog::ResetForTesting();
}

TracingEnvironment::~TracingEnvironment() {
  perfetto::Tracing::ResetForTesting();
}

}  // namespace test
}  // namespace base
