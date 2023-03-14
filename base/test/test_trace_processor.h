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

#include <memory>
#include "test_trace_processor_export.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace perfetto::trace_processor {
struct Config;
class TraceProcessor;
}  // namespace perfetto::trace_processor

namespace base::test {

class TEST_TRACE_PROCESSOR_EXPORT TestTraceProcessor {
 public:
  TestTraceProcessor();
  ~TestTraceProcessor();

  absl::Status ParseTrace(std::unique_ptr<uint8_t[]> buf, size_t size);
  absl::Status ParseTrace(const std::vector<char>& raw_trace);

  // Runs the sql query on the parsed trace and returns the result as a
  // vector of strings.
  std::vector<std::vector<std::string>> ExecuteQuery(const std::string& sql);

 private:
  std::unique_ptr<perfetto::trace_processor::Config> config_;
  std::unique_ptr<perfetto::trace_processor::TraceProcessor> trace_processor_;
};

}  // namespace base::test

#endif  // BASE_TEST_TEST_TRACE_PROCESSOR_H_
