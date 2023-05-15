// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use TestTraceProcessor to load a perfetto trace and run queries on the trace.
// Documentation on how to use the trace processor and write queries can be
// found here: https://perfetto.dev/docs/analysis/trace-processor.
// TODO(b/224531105): Implement EXTRACT_ARGS to return multiple args to simplify
// queries.

#ifndef BASE_TEST_TEST_TRACE_PROCESSOR_H_
#define BASE_TEST_TEST_TRACE_PROCESSOR_H_

#include "base/test/test_trace_processor_impl.h"
#include "base/test/trace_test_utils.h"
#include "base/types/expected.h"

// TODO(rasikan): Put these functions in a class.

namespace base::test {

std::unique_ptr<perfetto::TracingSession> StartTrace(
    const StringPiece& category_filter_string);

std::vector<char> StopTrace(std::unique_ptr<perfetto::TracingSession> session);

base::expected<TestTraceProcessorImpl::QueryResult, std::string> RunQuery(
    const std::string& query,
    const std::vector<char>& trace);

}  // namespace base::test

#endif  // BASE_TEST_TEST_TRACE_PROCESSOR_H_
