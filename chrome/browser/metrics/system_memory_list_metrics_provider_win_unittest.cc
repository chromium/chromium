// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_memory_list_metrics_provider_win.h"

#include <windows.h>

#include <ntstatus.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
constexpr char kZeroMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds.ZeroList";
constexpr char kFreeMemoryListHistogramName[] =
    "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds.FreeList";
constexpr char kTotalIntervalsRecordedHistogramName[] =
    "Memory.SystemMemoryLists.TotalIntervalsRecorded";
constexpr char kFreeListCountHistogramName[] =
    "Memory.SystemMemoryLists.FreePageCount";
constexpr char kZeroListCountHistogramName[] =
    "Memory.SystemMemoryLists.ZeroPageCount";
constexpr char kModifiedListCountHistogramName[] =
    "Memory.SystemMemoryLists.ModifiedPageCount";

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
  histogram_tester_.ExpectBucketCount(kZeroMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kFreeMemoryListHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kFreeMemoryListHistogramName, 1, 1);

  histogram_tester_.ExpectTotalCount(kTotalIntervalsRecordedHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kTotalIntervalsRecordedHistogramName, 1,
                                      1);

  histogram_tester_.ExpectUniqueSample(kFreeListCountHistogramName, 0, 1);
  histogram_tester_.ExpectUniqueSample(kZeroListCountHistogramName, 0, 1);
  histogram_tester_.ExpectUniqueSample(kModifiedListCountHistogramName, 0, 1);
}

// Verifies that we correctly set a non-empty memory list as non-exhausted.
TEST_F(WinSystemMemoryListMetricsProviderTest, SystemListFull) {
  EXPECT_CALL(getter_, Get(testing::_))
      .WillOnce(
          [](SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) -> NTSTATUS {
            memory_list_info = SYSTEM_MEMORY_LIST_INFORMATION{1, 1, 1, 1};
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

  histogram_tester_.ExpectTotalCount(kTotalIntervalsRecordedHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kTotalIntervalsRecordedHistogramName, 1,
                                      1);

  histogram_tester_.ExpectUniqueSample(kFreeListCountHistogramName, 1, 1);
  histogram_tester_.ExpectUniqueSample(kZeroListCountHistogramName, 1, 1);
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

  histogram_tester_.ExpectUniqueSample(kZeroMemoryListHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kFreeMemoryListHistogramName, 1, 2);
  histogram_tester_.ExpectUniqueSample(kTotalIntervalsRecordedHistogramName, 1,
                                       2);
  histogram_tester_.ExpectUniqueSample(kFreeListCountHistogramName, 0, 2);
  histogram_tester_.ExpectUniqueSample(kZeroListCountHistogramName, 0, 2);
  histogram_tester_.ExpectUniqueSample(kModifiedListCountHistogramName, 0, 2);
}
