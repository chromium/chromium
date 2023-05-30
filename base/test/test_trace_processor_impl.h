// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestTraceProcessorImpl encapsulates Perfetto's TraceProcessor. This is needed
// to prevent symbol conflicts between libtrace_processor and libperfetto.

#ifndef BASE_TEST_TEST_TRACE_PROCESSOR_IMPL_H_
#define BASE_TEST_TEST_TRACE_PROCESSOR_IMPL_H_

#include <memory>
#include "test_trace_processor_export.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace perfetto::trace_processor {
struct Config;
class TraceProcessor;
}  // namespace perfetto::trace_processor

namespace base::test {

class TEST_TRACE_PROCESSOR_EXPORT TestTraceProcessorImpl {
 public:
  using QueryResult = std::vector<std::vector<std::string>>;

  TestTraceProcessorImpl();
  ~TestTraceProcessorImpl();

  TestTraceProcessorImpl(TestTraceProcessorImpl&& other);
  TestTraceProcessorImpl& operator=(TestTraceProcessorImpl&& other);

  absl::Status ParseTrace(std::unique_ptr<uint8_t[]> buf, size_t size);
  absl::Status ParseTrace(const std::vector<char>& raw_trace);

  // Runs the sql query on the parsed trace and returns the result as a
  // vector of strings.
  absl::variant<QueryResult, std::string> ExecuteQuery(
      const std::string& sql) const;

 private:
  std::unique_ptr<perfetto::trace_processor::Config> config_;
  std::unique_ptr<perfetto::trace_processor::TraceProcessor> trace_processor_;
};

}  // namespace base::test

#endif  // BASE_TEST_TEST_TRACE_PROCESSOR_IMPL_H_
