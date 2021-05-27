// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_monitor.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(MemoryPressureMonitorTest, RecordMemoryPressure) {
  base::HistogramTester tester;
  const char* kHistogram = "Memory.PressureLevel";

  MemoryPressureMonitor::RecordMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, 3);
  tester.ExpectTotalCount(kHistogram, 3);
  tester.ExpectBucketCount(kHistogram, 0, 3);

  MemoryPressureMonitor::RecordMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, 2);
  tester.ExpectTotalCount(kHistogram, 5);
  tester.ExpectBucketCount(kHistogram, 1, 2);

  MemoryPressureMonitor::RecordMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, 1);
  tester.ExpectTotalCount(kHistogram, 6);
  tester.ExpectBucketCount(kHistogram, 2, 1);
}
}  // namespace base
