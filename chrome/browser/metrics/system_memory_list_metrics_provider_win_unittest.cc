// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_memory_list_metrics_provider_win.h"

#include <windows.h>

#include <ntstatus.h>

#include "base/byte_size.h"
#include "base/metrics/metrics_hashes.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/sample_metadata.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
constexpr char kZeroMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds.ZeroList";
constexpr char kFreeMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds.FreeList";
constexpr char kStandbyMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds.StandbyList";
constexpr char kAllMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
    "AllLists";
constexpr char kLowZeroMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds.ZeroList";
constexpr char kLowFreeMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds.FreeList";
constexpr char kLowStandbyMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds.StandbyList";
constexpr char kLowAllMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds."
    "AllLists";
constexpr char kIntervalsBelow150MBHistogramName[] =
    "Memory.SystemMemoryLists.IntervalsBelow150MB";
constexpr char kIntervalsBelow250MBHistogramName[] =
    "Memory.SystemMemoryLists.IntervalsBelow250MB";
constexpr char kIntervalsBelow350MBHistogramName[] =
    "Memory.SystemMemoryLists.IntervalsBelow350MB";
constexpr char kIntervalsBelow5PctHistogramName[] =
    "Memory.SystemMemoryLists.IntervalsBelow5Pct";
constexpr char kIntervalsBelow10PctHistogramName[] =
    "Memory.SystemMemoryLists.IntervalsBelow10Pct";
constexpr char kIntervalsBelow15PctHistogramName[] =
    "Memory.SystemMemoryLists.IntervalsBelow15Pct";
constexpr char kTotalIntervalsRecordedHistogramName[] =
    "Memory.SystemMemoryLists.TotalIntervalsRecorded";
constexpr char kFreeListCountHistogramName[] =
    "Memory.SystemMemoryLists.FreePageCount";
constexpr char kZeroListCountHistogramName[] =
    "Memory.SystemMemoryLists.ZeroPageCount";
constexpr char kModifiedListCountHistogramName[] =
    "Memory.SystemMemoryLists.ModifiedPageCount";
constexpr char kMaxModifiedListCountHistogramName[] =
    "Memory.SystemMemoryLists.MaxModifiedPageCount";
constexpr char kMinAvailableMemoryMBHistogramName[] =
    "Memory.SystemMemoryLists.MinAvailableMemoryMB";
constexpr char kMinAvailableMemoryPercentHistogramName[] =
    "Memory.SystemMemoryLists.MinAvailableMemoryPercent";
constexpr char kMaxAvailableMemoryAfterMinMBHistogramName[] =
    "Memory.SystemMemoryLists.MaxAvailableMemoryAfterMinMB";
constexpr char kMaxAvailableMemoryAfterMinPercentHistogramName[] =
    "Memory.SystemMemoryLists.MaxAvailableMemoryAfterMinPercent";
constexpr char kStandbyListCountHistogramName[] =
    "Memory.SystemMemoryLists.StandbyPageCount";

constexpr char kStandbyListByPriorityCatString[] =
    "Memory.SystemMemoryLists.StandbyPageCountByPriority.";

class MemoryListInfoGetter {
 public:
  virtual NTSTATUS Get(SYSTEM_MEMORY_LIST_INFORMATION& info) = 0;
};
}  // namespace

class TestSystemMemoryListMetricsProvider
    : public SystemMemoryListMetricsProvider {
 public:
  TestSystemMemoryListMetricsProvider(base::TimeDelta sampling_interval,
                                      base::TimeDelta recording_interval,
                                      MemoryListInfoGetter& getter)
      : SystemMemoryListMetricsProvider(sampling_interval, recording_interval),
        getter_(getter) {}

 private:
  NTSTATUS GetSystemMemoryListInformation(
      SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) override {
    return getter_->Get(memory_list_info);
  }

  const base::raw_ref<MemoryListInfoGetter> getter_;
};

class MockMemoryListInfoGetter : public MemoryListInfoGetter {
 public:
  MOCK_METHOD(NTSTATUS, Get, (SYSTEM_MEMORY_LIST_INFORMATION&), (override));
};

class WinSystemMemoryListMetricsProviderTest : public testing::Test {
 protected:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  MockMemoryListInfoGetter getter_;
  base::RunLoop run_loop_;
  base::HistogramTester histogram_tester_;
};

// Verifies that we correctly set an empty memory list as exhausted.
TEST_F(WinSystemMemoryListMetricsProviderTest, SystemListEmpty) {
  EXPECT_CALL(getter_, Get(testing::_))
      .WillOnce(
          [](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            memory_list_info = SYSTEM_MEMORY_LIST_INFORMATION{0};
            return STATUS_SUCCESS;
          })
      .WillOnce(
          [](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            memory_list_info = SYSTEM_MEMORY_LIST_INFORMATION{0};
            return STATUS_SUCCESS;
          })
      .WillOnce(
          [quit_closure = run_loop_.QuitClosure()](
              SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            quit_closure.Run();
            return STATUS_ACCESS_DENIED;
          })
      .WillRepeatedly(testing::Return(STATUS_ACCESS_DENIED));

  TestSystemMemoryListMetricsProvider sampler(base::Milliseconds(100),
                                              base::Milliseconds(50), getter_);

  sampler.OnRecordingEnabled();

  run_loop_.Run();

  histogram_tester_.ExpectTotalCount(kZeroMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kZeroMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kFreeMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kFreeMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kStandbyMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kStandbyMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kAllMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kAllMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kLowZeroMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kLowZeroMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kLowFreeMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kLowFreeMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kLowStandbyMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kLowStandbyMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kLowAllMemoryListHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kLowAllMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kTotalIntervalsRecordedHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kTotalIntervalsRecordedHistogramName, 1,
                                      2);

  histogram_tester_.ExpectUniqueSample(kFreeListCountHistogramName, 0, 2);
  histogram_tester_.ExpectUniqueSample(kZeroListCountHistogramName, 0, 2);
  histogram_tester_.ExpectUniqueSample(kStandbyListCountHistogramName, 0, 2);
  for (int priority_number = 1; priority_number <= 8; ++priority_number) {
    histogram_tester_.ExpectUniqueSample(
        base::StrCat({kStandbyListByPriorityCatString,
                      base::NumberToString(priority_number)}),
        0, 2);
  }
  histogram_tester_.ExpectUniqueSample(kModifiedListCountHistogramName, 0, 2);
}

// Verifies that we correctly set a non-empty memory list as non-exhausted.
TEST_F(WinSystemMemoryListMetricsProviderTest, SystemListFull) {
  EXPECT_CALL(getter_, Get(testing::_))
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info)
                    -> NTSTATUS {
        memory_list_info = SYSTEM_MEMORY_LIST_INFORMATION{
            1, 1, 1, 1, 1, {1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1},
            1};
        return STATUS_SUCCESS;
      })
      .WillOnce(
          [quit_closure = run_loop_.QuitClosure()](
              SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            quit_closure.Run();
            return STATUS_ACCESS_DENIED;
          })
      .WillRepeatedly(testing::Return(STATUS_ACCESS_DENIED));

  TestSystemMemoryListMetricsProvider sampler(base::Milliseconds(100),
                                              base::Milliseconds(50), getter_);

  sampler.OnRecordingEnabled();

  run_loop_.Run();

  histogram_tester_.ExpectTotalCount(kZeroMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kZeroMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kFreeMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kFreeMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kStandbyMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kStandbyMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kAllMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kAllMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kLowZeroMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kLowZeroMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kLowFreeMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kLowFreeMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kLowStandbyMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kLowStandbyMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kLowAllMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kLowAllMemoryListHistogramName, 0, 1);

  histogram_tester_.ExpectTotalCount(kTotalIntervalsRecordedHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kTotalIntervalsRecordedHistogramName, 1,
                                      1);

  histogram_tester_.ExpectUniqueSample(kFreeListCountHistogramName, 1, 1);
  histogram_tester_.ExpectUniqueSample(kZeroListCountHistogramName, 1, 1);
  histogram_tester_.ExpectUniqueSample(kStandbyListCountHistogramName, 7, 1);
  for (int priority_number = 1; priority_number <= 8; ++priority_number) {
    histogram_tester_.ExpectUniqueSample(
        base::StrCat({kStandbyListByPriorityCatString,
                      base::NumberToString(priority_number)}),
        1, 1);
  }
  histogram_tester_.ExpectUniqueSample(kModifiedListCountHistogramName, 1, 1);
}

// Verifies that we correctly reset all the counters by ensuring we don't record
// '2' for any of the values.
TEST_F(WinSystemMemoryListMetricsProviderTest, SystemListCounterReset) {
  auto success_closure =
      [](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
    memory_list_info = SYSTEM_MEMORY_LIST_INFORMATION{0};
    return STATUS_SUCCESS;
  };
  EXPECT_CALL(getter_, Get(testing::_))
      .WillOnce(success_closure)
      .WillOnce(success_closure)
      .WillOnce(
          [quit_closure = run_loop_.QuitClosure()](
              SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            quit_closure.Run();
            return STATUS_ACCESS_DENIED;
          })
      .WillRepeatedly(testing::Return(STATUS_ACCESS_DENIED));

  TestSystemMemoryListMetricsProvider sampler(base::Milliseconds(100),
                                              base::Milliseconds(50), getter_);

  sampler.OnRecordingEnabled();

  run_loop_.Run();

  histogram_tester_.ExpectBucketCount(kZeroMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kFreeMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kStandbyMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kAllMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kLowZeroMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kLowFreeMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kLowStandbyMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectBucketCount(kLowAllMemoryListHistogramName, 1, 1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow150MBHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow250MBHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow350MBHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow5PctHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow10PctHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow15PctHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kTotalIntervalsRecordedHistogramName, 1,
                                       2);
  histogram_tester_.ExpectUniqueSample(kFreeListCountHistogramName, 0, 2);
  histogram_tester_.ExpectUniqueSample(kZeroListCountHistogramName, 0, 2);
  histogram_tester_.ExpectUniqueSample(kStandbyListCountHistogramName, 0, 2);
  for (int priority_number = 1; priority_number <= 8; ++priority_number) {
    histogram_tester_.ExpectUniqueSample(
        base::StrCat({kStandbyListByPriorityCatString,
                      base::NumberToString(priority_number)}),
        0, 2);
  }

  histogram_tester_.ExpectUniqueSample(kModifiedListCountHistogramName, 0, 2);
}

TEST_F(WinSystemMemoryListMetricsProviderTest, SampleMetadataSetDuringRuns) {
  base::MetadataRecorder::ItemArray items;
  size_t initial_item_count =
      base::MetadataRecorder::MetadataProvider(
          base::GetSampleMetadataRecorder(), base::PlatformThread::CurrentId())
          .GetItems(&items);

  auto success_closure =
      [](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
    memory_list_info = SYSTEM_MEMORY_LIST_INFORMATION{0};
    return STATUS_SUCCESS;
  };
  EXPECT_CALL(getter_, Get(testing::_))
      .WillOnce(success_closure)
      .WillOnce(
          [&](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            EXPECT_EQ(initial_item_count + 2,
                      base::MetadataRecorder::MetadataProvider(
                          base::GetSampleMetadataRecorder(),
                          base::PlatformThread::CurrentId())
                          .GetItems(&items));
            EXPECT_EQ(base::HashMetricName("WindowsZeroPageCount"),
                      items[initial_item_count].name_hash);
            EXPECT_FALSE(items[initial_item_count].key.has_value());
            EXPECT_EQ(0, items[initial_item_count].value);
            run_loop_.Quit();
            return STATUS_ACCESS_DENIED;
          });

  TestSystemMemoryListMetricsProvider sampler(base::Milliseconds(100),
                                              base::Milliseconds(50), getter_);

  sampler.OnRecordingEnabled();
  run_loop_.Run();
}

// Verifies 30-second extrema and low-Available interval counters across
// multiple sampling ticks.
TEST_F(WinSystemMemoryListMetricsProviderTest,
       ExtremaAndIntervalsBelowThresholds) {
  base::test::ScopedAmountOfPhysicalMemoryOverride physical_memory_override(
      base::GiB(16));
  constexpr uintptr_t kPagesPerMiB = 256;

  EXPECT_CALL(getter_, Get(testing::_))
      // Tick 1 (pre-trough peak): Available = 3200 MiB (19%), Modified = 900
      // MiB.
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& info) -> NTSTATUS {
        info = SYSTEM_MEMORY_LIST_INFORMATION{0};
        info.FreePageCount = 3200 * kPagesPerMiB;
        info.ModifiedPageCount = 900 * kPagesPerMiB;
        return STATUS_SUCCESS;
      })
      // Tick 2: Available = 200 MiB (1%), Modified = 920 MiB.
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& info) -> NTSTATUS {
        info = SYSTEM_MEMORY_LIST_INFORMATION{0};
        info.FreePageCount = 200 * kPagesPerMiB;
        info.ModifiedPageCount = 920 * kPagesPerMiB;
        return STATUS_SUCCESS;
      })
      // Tick 3 (Available trough & Modified peak): Available = 100 MiB (0%),
      // Modified = 960 MiB (5%).
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& info) -> NTSTATUS {
        info = SYSTEM_MEMORY_LIST_INFORMATION{0};
        info.FreePageCount = 100 * kPagesPerMiB;
        info.ModifiedPageCount = 960 * kPagesPerMiB;
        return STATUS_SUCCESS;
      })
      // Tick 4: Available = 600 MiB (3%), Modified = 300 MiB.
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& info) -> NTSTATUS {
        info = SYSTEM_MEMORY_LIST_INFORMATION{0};
        info.PageCountByPriority[5] = 600 * kPagesPerMiB;
        info.ModifiedPageCount = 300 * kPagesPerMiB;
        return STATUS_SUCCESS;
      })
      // Tick 5 (post-trough peak): Available = 2400 MiB (14%), Modified = 64
      // MiB.
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& info) -> NTSTATUS {
        info = SYSTEM_MEMORY_LIST_INFORMATION{0};
        info.PageCountByPriority[5] = 2400 * kPagesPerMiB;
        info.ModifiedPageCount = 64 * kPagesPerMiB;
        return STATUS_SUCCESS;
      })
      // Tick 6: Available = 1200 MiB (7%), Modified = 65 MiB. Sleep so that the
      // 30s window closes on this tick regardless of timer resolution.
      .WillOnce([](SYSTEM_MEMORY_LIST_INFORMATION& info) -> NTSTATUS {
        base::PlatformThread::Sleep(base::Milliseconds(500));
        info = SYSTEM_MEMORY_LIST_INFORMATION{0};
        info.PageCountByPriority[5] = 1200 * kPagesPerMiB;
        info.ModifiedPageCount = 65 * kPagesPerMiB;
        return STATUS_SUCCESS;
      })
      .WillOnce(
          [quit_closure = run_loop_.QuitClosure()](
              SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            quit_closure.Run();
            return STATUS_ACCESS_DENIED;
          })
      .WillRepeatedly(testing::Return(STATUS_ACCESS_DENIED));

  TestSystemMemoryListMetricsProvider sampler(base::Milliseconds(1),
                                              base::Milliseconds(500), getter_);

  sampler.OnRecordingEnabled();
  run_loop_.Run();

  histogram_tester_.ExpectUniqueSample(kTotalIntervalsRecordedHistogramName, 6,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow150MBHistogramName, 1, 1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow250MBHistogramName, 2, 1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow350MBHistogramName, 2, 1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow5PctHistogramName, 3, 1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow10PctHistogramName, 4, 1);
  histogram_tester_.ExpectUniqueSample(kIntervalsBelow15PctHistogramName, 5, 1);

  histogram_tester_.ExpectUniqueSample(kMinAvailableMemoryMBHistogramName, 100,
                                       1);
  histogram_tester_.ExpectUniqueSample(kMinAvailableMemoryPercentHistogramName,
                                       0, 1);
  // Expect the peak after the trough (2400 MiB / 14%), not the pre-trough peak
  // (3200 MiB / 19%) or the end-of-window value (1200 MiB / 7%).
  histogram_tester_.ExpectUniqueSample(
      kMaxAvailableMemoryAfterMinMBHistogramName, 2400, 1);
  histogram_tester_.ExpectUniqueSample(
      kMaxAvailableMemoryAfterMinPercentHistogramName, 14, 1);
  histogram_tester_.ExpectUniqueSample(kModifiedListCountHistogramName,
                                       65 * kPagesPerMiB, 1);
  histogram_tester_.ExpectUniqueSample(kMaxModifiedListCountHistogramName,
                                       960 * kPagesPerMiB, 1);
}
