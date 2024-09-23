// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_trace_processor.h"

#include "base/test/task_environment.h"
#include "base/test/trace_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace base::test {


class TestTraceProcessorExample : public ::testing::Test {
 private:
  base::test::TracingEnvironment tracing_environment_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TestTraceProcessorExample, Basic) {
  TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace("");

  {
    // A simple trace event inside of a scope so it gets flushed properly.
    TRACE_EVENT("test_category", "test_event");
  }

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    SELECT
      name
    FROM slice
    WHERE category = 'test_category'
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"}));
}

TEST_F(TestTraceProcessorExample, BasicTraceConfig) {
  TestTraceProcessor test_trace_processor;

  // Start tracing with a category filter string set in the trace config.
  test_trace_processor.StartTrace(base::test::DefaultTraceConfig(
      "test_category", /*privacy_filtering=*/false));

  {
    // A simple trace event inside of a scope so it gets flushed properly.
    TRACE_EVENT("test_category", "test_event");
  }

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    SELECT
      name
    FROM slice
  )");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"}));
}


}  // namespace base::test
