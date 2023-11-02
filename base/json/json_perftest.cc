// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {

namespace {

constexpr char kMetricPrefixJSON[] = "JSON.";
constexpr char kMetricReadTime[] = "read_time";
constexpr char kMetricWriteTime[] = "write_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixJSON, story_name);
  reporter.RegisterImportantMetric(kMetricReadTime, "ms");
  reporter.RegisterImportantMetric(kMetricWriteTime, "ms");
  return reporter;
}

// Generates a simple dictionary value with simple data types, a string and a
// list.
Value::Dict GenerateDict() {
  Value::Dict root;
  root.Set("Double", 3.141);
  root.Set("Bool", true);
  root.Set("Int", 42);
  root.Set("String", "Foo");

  Value::List list;
  list.Append(2.718);
  list.Append(false);
  list.Append(123);
  list.Append("Bar");
  root.Set("List", std::move(list));

  return root;
}

// Generates a tree-like dictionary value with a size of O(breadth ** depth).
Value::Dict GenerateLayeredDict(int breadth, int depth) {
  if (depth == 1)
    return GenerateDict();

  Value::Dict root = GenerateDict();
  Value::Dict next = GenerateLayeredDict(breadth, depth - 1);

  for (int i = 0; i < breadth; ++i) {
    root.Set("Dict" + base::NumberToString(i), next.Clone());
  }

  return root;
}

}  // namespace

class JSONPerfTest : public testing::Test {
 public:
  void TestWriteAndRead(int breadth, int depth) {
    std::string description = "Breadth: " + base::NumberToString(breadth) +
                              ", Depth: " + base::NumberToString(depth);
    Value::Dict dict = GenerateLayeredDict(breadth, depth);
    std::string json;

    TimeTicks start_write = TimeTicks::Now();
    JSONWriter::Write(dict, &json);
    TimeTicks end_write = TimeTicks::Now();
    auto reporter = SetUpReporter("breadth_" + base::NumberToString(breadth) +
                                  "_depth_" + base::NumberToString(depth));
    reporter.AddResult(kMetricWriteTime, end_write - start_write);

    TimeTicks start_read = TimeTicks::Now();
    JSONReader::Read(json);
    TimeTicks end_read = TimeTicks::Now();
    reporter.AddResult(kMetricReadTime, end_read - start_read);
  }
};

TEST_F(JSONPerfTest, StressTest) {
  // These loop ranges are chosen such that this test will complete in a
  // reasonable amount of time and will work on a 32-bit build without hitting
  // an out-of-memory failure. Having j go to 10 uses over 2 GiB of memory and
  // might hit Android timeouts so be wary of going that high.
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 10; ++j) {
      TestWriteAndRead(i + 1, j + 1);
    }
  }
}

}  // namespace base
