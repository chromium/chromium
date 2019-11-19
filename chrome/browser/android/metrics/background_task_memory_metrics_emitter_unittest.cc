// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/background_task_memory_metrics_emitter.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/renderer_uptime_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

using ApplicationState = base::android::ApplicationState;
using GlobalMemoryDump = memory_instrumentation::GlobalMemoryDump;
using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using ProcessMemoryDump = memory_instrumentation::mojom::ProcessMemoryDump;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using ProcessType = memory_instrumentation::mojom::ProcessType;
using OSMemDump = memory_instrumentation::mojom::OSMemDump;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;

constexpr uint64_t kBrowserResidentMB = 11;
constexpr uint64_t kBrowserPrivateMB = 13;
constexpr uint64_t kBrowserSharedMB = 17;
constexpr uint64_t kBrowserPrivateSwapMB = 19;
constexpr uint64_t kUtilResidentMB = 23;
constexpr uint64_t kUtilPrivateMB = 29;
constexpr uint64_t kUtilSharedMB = 31;
constexpr uint64_t kUtilPrivateSwapMB = 37;
constexpr uint64_t kRendererResidentMB = 41;
constexpr uint64_t kRendererPrivateMB = 43;
constexpr uint64_t kRendererSharedMB = 47;
constexpr uint64_t kRendererPrivateSwapMB = 53;

void PopulateBrowserMetrics(GlobalMemoryDumpPtr& global_dump) {
  ProcessMemoryDumpPtr pmd(ProcessMemoryDump::New());
  pmd->process_type = ProcessType::BROWSER;
  OSMemDumpPtr os_dump =
      OSMemDump::New(kBrowserResidentMB * 1024,
                     /*peak_resident_set_kb=*/kBrowserResidentMB * 1024,
                     /*is_peak_rss_resettable=*/true, kBrowserPrivateMB * 1024,
                     kBrowserSharedMB * 1024, kBrowserPrivateSwapMB * 1024);
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

void PopulateUtilMetrics(GlobalMemoryDumpPtr& global_dump) {
  ProcessMemoryDumpPtr pmd(ProcessMemoryDump::New());
  pmd->process_type = ProcessType::UTILITY;
  OSMemDumpPtr os_dump =
      OSMemDump::New(kUtilResidentMB * 1024,
                     /*peak_resident_set_kb=*/kUtilResidentMB * 1024,
                     /*is_peak_rss_resettable=*/true, kUtilPrivateMB * 1024,
                     kUtilSharedMB * 1024, kUtilPrivateSwapMB * 1024);
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

void PopulateRendererMetrics(GlobalMemoryDumpPtr& global_dump) {
  ProcessMemoryDumpPtr pmd(ProcessMemoryDump::New());
  pmd->process_type = ProcessType::RENDERER;
  OSMemDumpPtr os_dump =
      OSMemDump::New(kRendererResidentMB * 1024,
                     /*peak_resident_set_kb=*/kRendererResidentMB * 1024,
                     /*is_peak_rss_resettable=*/true, kRendererPrivateMB * 1024,
                     kRendererSharedMB * 1024, kRendererPrivateSwapMB * 1024);
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

std::unique_ptr<GlobalMemoryDump> CreateEmptyMemoryDump() {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  return GlobalMemoryDump::MoveFrom(std::move(global_dump));
}

std::unique_ptr<GlobalMemoryDump> CreateMemoryDumpOnlyBrowser() {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateBrowserMetrics(global_dump);
  return GlobalMemoryDump::MoveFrom(std::move(global_dump));
}

std::unique_ptr<GlobalMemoryDump> CreateMemoryDumpWithoutBrowser() {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateUtilMetrics(global_dump);
  return GlobalMemoryDump::MoveFrom(std::move(global_dump));
}

std::unique_ptr<GlobalMemoryDump> CreateMemoryDumpWithBrowserFirst() {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateBrowserMetrics(global_dump);
  PopulateUtilMetrics(global_dump);
  return GlobalMemoryDump::MoveFrom(std::move(global_dump));
}

std::unique_ptr<GlobalMemoryDump> CreateMemoryDumpWithBrowserLast() {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateUtilMetrics(global_dump);
  PopulateBrowserMetrics(global_dump);
  return GlobalMemoryDump::MoveFrom(std::move(global_dump));
}

std::unique_ptr<GlobalMemoryDump> CreateMemoryDumpWithBrowserAndRenderer() {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateBrowserMetrics(global_dump);
  PopulateRendererMetrics(global_dump);
  return GlobalMemoryDump::MoveFrom(std::move(global_dump));
}

void ExpectHistogramsRecorded(const base::HistogramTester& histograms,
                              const std::string& prefix,
                              const std::string& suffix) {
  histograms.ExpectUniqueSample(prefix + "ResidentSet" + suffix,
                                kBrowserResidentMB, 1);
  histograms.ExpectUniqueSample(prefix + "PrivateMemoryFootprint" + suffix,
                                kBrowserPrivateMB, 1);
  histograms.ExpectUniqueSample(prefix + "SharedMemoryFootprint" + suffix,
                                kBrowserSharedMB, 1);
  histograms.ExpectUniqueSample(prefix + "PrivateSwapFootprint" + suffix,
                                kBrowserPrivateSwapMB, 1);
}

void ExpectHistogramsNotRecorded(const base::HistogramTester& histograms,
                                 const std::string& prefix,
                                 const std::string& suffix) {
  histograms.ExpectTotalCount(prefix + "ResidentSet" + suffix, 0);
  histograms.ExpectTotalCount(prefix + "PrivateMemoryFootprint" + suffix, 0);
  histograms.ExpectTotalCount(prefix + "SharedMemoryFootprint" + suffix, 0);
  histograms.ExpectTotalCount(prefix + "PrivateSwapFootprint" + suffix, 0);
}

void ExpectOnlyFullBrowserHistograms(const base::HistogramTester& histograms) {
  ExpectHistogramsRecorded(histograms, "Memory.BackgroundTask.Browser.",
                           ".FullBrowser");
  ExpectHistogramsNotRecorded(histograms, "Memory.BackgroundTask.Browser.",
                              ".ReducedMode");
}

void ExpectOnlyReducedModeHistograms(const base::HistogramTester& histograms) {
  ExpectHistogramsNotRecorded(histograms, "Memory.BackgroundTask.Browser.",
                              ".FullBrowser");
  ExpectHistogramsRecorded(histograms, "Memory.BackgroundTask.Browser.",
                           ".ReducedMode");
}

void ExpectNoHistograms(const base::HistogramTester& histograms) {
  ExpectHistogramsNotRecorded(histograms, "Memory.BackgroundTask.Browser.",
                              ".FullBrowser");
  ExpectHistogramsNotRecorded(histograms, "Memory.BackgroundTask.Browser.",
                              ".ReducedMode");
}

void ExpectOnlyFullBrowserHistogramsWithAffix(
    const base::HistogramTester& histograms) {
  ExpectHistogramsRecorded(
      histograms, "Memory.BackgroundTask.TestAffix.Browser.", ".FullBrowser");
  ExpectHistogramsNotRecorded(
      histograms, "Memory.BackgroundTask.TestAffix.Browser.", ".ReducedMode");
}

void ExpectOnlyReducedModeHistogramsWithAffix(
    const base::HistogramTester& histograms) {
  ExpectHistogramsNotRecorded(
      histograms, "Memory.BackgroundTask.TestAffix.Browser.", ".FullBrowser");
  ExpectHistogramsRecorded(
      histograms, "Memory.BackgroundTask.TestAffix.Browser.", ".ReducedMode");
}

void ExpectNoHistogramsWithAffix(const base::HistogramTester& histograms) {
  ExpectHistogramsNotRecorded(
      histograms, "Memory.BackgroundTask.TestAffix.Browser.", ".FullBrowser");
  ExpectHistogramsNotRecorded(
      histograms, "Memory.BackgroundTask.TestAffix.Browser.", ".ReducedMode");
}

class TestBackgroundTaskMemoryMetricsEmitter
    : public BackgroundTaskMemoryMetricsEmitter {
 public:
  explicit TestBackgroundTaskMemoryMetricsEmitter(
      bool is_reduced_mode,
      const std::string& task_type_affix)
      : BackgroundTaskMemoryMetricsEmitter(is_reduced_mode, task_type_affix) {}

  void SetApplicationState(ApplicationState state) {
    application_state_for_testing_ = state;
  }

  void CallReceivedMemoryDump(bool success,
                              std::unique_ptr<GlobalMemoryDump> dump) {
    ASSERT_TRUE(callback_);
    std::move(callback_).Run(success, std::move(dump));
  }

  bool HasRequestedGlobalDump() { return requested_global_dump_; }

 protected:
  ~TestBackgroundTaskMemoryMetricsEmitter() override;
  ApplicationState GetApplicationState() override;
  void RequestGlobalDump(ReceivedMemoryDumpCallback callback) override;

 private:
  ApplicationState application_state_for_testing_ =
      base::android::APPLICATION_STATE_UNKNOWN;
  ReceivedMemoryDumpCallback callback_;
  bool requested_global_dump_ = false;
};

TestBackgroundTaskMemoryMetricsEmitter::
    ~TestBackgroundTaskMemoryMetricsEmitter() {}

ApplicationState TestBackgroundTaskMemoryMetricsEmitter::GetApplicationState() {
  return application_state_for_testing_;
}

void TestBackgroundTaskMemoryMetricsEmitter::RequestGlobalDump(
    ReceivedMemoryDumpCallback callback) {
  ASSERT_FALSE(requested_global_dump_);
  callback_ = std::move(callback);
  requested_global_dump_ = true;
}

class BackgroundTaskMemoryMetricsEmitterTest : public testing::Test {
 public:
  BackgroundTaskMemoryMetricsEmitterTest() {}
  ~BackgroundTaskMemoryMetricsEmitterTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskMemoryMetricsEmitterTest);
};

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitOnlyBrowserFullBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    CreateMemoryDumpOnlyBrowser());
    ExpectOnlyFullBrowserHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitOnlyBrowserReducedMode) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/true,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    CreateMemoryDumpOnlyBrowser());
    ExpectOnlyReducedModeHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitWithAffixOnlyBrowserFullBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(
          /*is_reduced_mode=*/false,
          /*task_type_affix=*/"TestAffix"));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    CreateMemoryDumpOnlyBrowser());
    ExpectOnlyFullBrowserHistograms(histograms);
    ExpectOnlyFullBrowserHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitWithAffixOnlyBrowserReducedMode) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(
          /*is_reduced_mode=*/true,
          /*task_type_affix=*/"TestAffix"));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    CreateMemoryDumpOnlyBrowser());
    ExpectOnlyReducedModeHistograms(histograms);
    ExpectOnlyReducedModeHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitBrowserFirstFullBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(
        /*success=*/true,
        /*dump=*/CreateMemoryDumpWithBrowserFirst());
    ExpectOnlyFullBrowserHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitBrowserFirstReducedMode) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/true,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(
        /*success=*/true,
        /*dump=*/CreateMemoryDumpWithBrowserFirst());
    ExpectOnlyReducedModeHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitBrowserLastFullBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    /*dump=*/CreateMemoryDumpWithBrowserLast());
    ExpectOnlyFullBrowserHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitBrowserLastReducedMode) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/true,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    /*dump=*/CreateMemoryDumpWithBrowserLast());
    ExpectOnlyReducedModeHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitBrowserRendererFullBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(
        /*success=*/true,
        /*dump=*/CreateMemoryDumpWithBrowserAndRenderer());
    ExpectOnlyFullBrowserHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitBrowserRendererReducedMode) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/true,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(
        /*success=*/true,
        /*dump=*/CreateMemoryDumpWithBrowserAndRenderer());
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest, FetchAndEmitFailDump) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/false, /*dump=*/nullptr);
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest, FetchAndEmitNullDump) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true, /*dump=*/nullptr);
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest, FetchAndEmitEmptyDump) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    /*dump=*/CreateEmptyMemoryDump());
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest, FetchAndEmitWithoutBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    /*dump=*/CreateMemoryDumpWithoutBrowser());
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitActivityStartedFullBrowser) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/false,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->SetApplicationState(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    CreateMemoryDumpOnlyBrowser());
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

TEST_F(BackgroundTaskMemoryMetricsEmitterTest,
       FetchAndEmitActivityStartedReducedMode) {
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/true,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_TRUE(emitter->HasRequestedGlobalDump());

    emitter->SetApplicationState(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    emitter->CallReceivedMemoryDump(/*success=*/true,
                                    CreateMemoryDumpOnlyBrowser());
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

class BackgroundTaskMemoryMetricsEmitterTestWithState
    : public testing::TestWithParam<ApplicationState> {
 public:
  BackgroundTaskMemoryMetricsEmitterTestWithState() {}
  ~BackgroundTaskMemoryMetricsEmitterTestWithState() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskMemoryMetricsEmitterTestWithState);
};

TEST_P(BackgroundTaskMemoryMetricsEmitterTestWithState,
       DoNotFetchAndEmitWithActivity) {
  ApplicationState application_state_param = GetParam();
  scoped_refptr<TestBackgroundTaskMemoryMetricsEmitter> emitter(
      new TestBackgroundTaskMemoryMetricsEmitter(/*is_reduced_mode=*/true,
                                                 /*task_type_affix=*/""));
  emitter->SetApplicationState(application_state_param);

  {
    base::HistogramTester histograms;

    emitter->FetchAndEmitProcessMemoryMetrics();
    EXPECT_FALSE(emitter->HasRequestedGlobalDump());
    ExpectNoHistograms(histograms);
    ExpectNoHistogramsWithAffix(histograms);
  }
}

INSTANTIATE_TEST_SUITE_P(
    State,
    BackgroundTaskMemoryMetricsEmitterTestWithState,
    testing::Values(base::android::APPLICATION_STATE_UNKNOWN,
                    base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES,
                    base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES,
                    base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES));
