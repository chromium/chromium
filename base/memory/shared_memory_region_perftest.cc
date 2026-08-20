// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {

namespace {

constexpr char kMetricPrefix[] = "SharedMemoryRegion.";
constexpr char kMetricCreate[] = "create";
constexpr char kMetricCreateMap[] = "create_and_map";
constexpr char kMetricCreateMapTouch[] = "create_map_and_touch";

constexpr size_t kSizes[] = {4 * 1024, 64 * 1024, 1024 * 1024, 8 * 1024 * 1024};

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefix, story);
  reporter.RegisterImportantMetric(kMetricCreate, "us");
  reporter.RegisterImportantMetric(kMetricCreateMap, "us");
  reporter.RegisterImportantMetric(kMetricCreateMapTouch, "us");
  return reporter;
}

int IterationsFor(size_t size) {
  return size >= 1024 * 1024 ? 300 : 3000;
}

template <typename CreateAndRun>
double MicrosecondsPerIteration(int iterations, CreateAndRun run) {
  // Warm up.
  for (int i = 0; i < iterations / 10; ++i) {
    run();
  }
  ElapsedTimer timer;
  for (int i = 0; i < iterations; ++i) {
    run();
  }
  return timer.Elapsed().InMicrosecondsF() / iterations;
}

// Touches one byte per page so that the cost of populating the region is
// included regardless of whether the platform commits pages up front.
void TouchPages(span<uint8_t> memory) {
  constexpr size_t kPageSize = 4096;
  for (size_t offset = 0; offset < memory.size(); offset += kPageSize) {
    memory[offset] = 1;
  }
}

template <typename RegionType>
void RunStory(const std::string& region_name) {
  for (size_t size : kSizes) {
    const int iterations = IterationsFor(size);
    auto reporter =
        SetUpReporter(region_name + "_" + NumberToString(size / 1024) + "KiB");
    reporter.AddResult(kMetricCreate, MicrosecondsPerIteration(iterations, [&] {
                         auto region = RegionType::Create(size);
                         EXPECT_TRUE(region.IsValid());
                       }));
    reporter.AddResult(kMetricCreateMap,
                       MicrosecondsPerIteration(iterations, [&] {
                         auto region = RegionType::Create(size);
                         auto mapping = region.Map();
                         EXPECT_TRUE(mapping.IsValid());
                       }));
    reporter.AddResult(
        kMetricCreateMapTouch, MicrosecondsPerIteration(iterations, [&] {
          auto region = RegionType::Create(size);
          auto mapping = region.Map();
          TouchPages(mapping.template GetMemoryAsSpan<uint8_t>());
        }));
  }
}

}  // namespace

TEST(SharedMemoryRegionPerfTest, Unsafe) {
  RunStory<UnsafeSharedMemoryRegion>("unsafe");
}

TEST(SharedMemoryRegionPerfTest, Writable) {
  RunStory<WritableSharedMemoryRegion>("writable");
}

TEST(SharedMemoryRegionPerfTest, ReadOnly) {
  for (size_t size : kSizes) {
    const int iterations = IterationsFor(size);
    auto reporter =
        SetUpReporter("read_only_" + NumberToString(size / 1024) + "KiB");
    reporter.AddResult(kMetricCreate, MicrosecondsPerIteration(iterations, [&] {
                         auto mapped = ReadOnlySharedMemoryRegion::Create(size);
                         EXPECT_TRUE(mapped.IsValid());
                       }));
    reporter.AddResult(kMetricCreateMapTouch,
                       MicrosecondsPerIteration(iterations, [&] {
                         auto mapped = ReadOnlySharedMemoryRegion::Create(size);
                         TouchPages(mapped.mapping.GetMemoryAsSpan<uint8_t>());
                         auto ro_mapping = mapped.region.Map();
                         EXPECT_TRUE(ro_mapping.IsValid());
                       }));
  }
}

}  // namespace base
