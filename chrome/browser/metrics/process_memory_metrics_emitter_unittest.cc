// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

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
#include "testing/gtest/include/gtest/gtest.h"

using GlobalMemoryDump = memory_instrumentation::GlobalMemoryDump;
using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;
using PageInfo = ProcessMemoryMetricsEmitter::PageInfo;
using ProcessType = memory_instrumentation::mojom::ProcessType;
using ProcessInfo = ProcessMemoryMetricsEmitter::ProcessInfo;
using ProcessInfoVector = std::vector<ProcessInfo>;

namespace {

using UkmEntry = ukm::builders::Memory_Experimental;

using MetricMap = base::flat_map<const char*, int64_t>;

int GetResidentValue(const MetricMap& metric_map) {
#if defined(OS_MACOSX)
  // Resident set is not populated on Mac.
  return 0;
#else
  auto it = metric_map.find("Resident");
  EXPECT_NE(it, metric_map.end());
  return it->second;
#endif
}

// Provide fake to surface ReceivedMemoryDump and ReceivedProcessInfos to public
// visibility.
class ProcessMemoryMetricsEmitterFake : public ProcessMemoryMetricsEmitter {
 public:
  ProcessMemoryMetricsEmitterFake(
      ukm::TestAutoSetUkmRecorder& test_ukm_recorder)
      : ukm_recorder_(&test_ukm_recorder) {
    MarkServiceRequestsInProgress();
  }

  void ReceivedMemoryDump(bool success,
                          std::unique_ptr<GlobalMemoryDump> ptr) override {
    ProcessMemoryMetricsEmitter::ReceivedMemoryDump(success, std::move(ptr));
  }

  void ReceivedProcessInfos(ProcessInfoVector process_infos) override {
    ProcessMemoryMetricsEmitter::ReceivedProcessInfos(std::move(process_infos));
  }

  ukm::UkmRecorder* GetUkmRecorder() override { return ukm_recorder_; }

  int GetNumberOfExtensions(base::ProcessId pid) override {
    switch (pid) {
      case 401:
        return 1;
      default:
        return 0;
    }
  }

  base::Optional<base::TimeDelta> GetProcessUptime(
      const base::Time& now,
      base::ProcessId pid) override {
    switch (pid) {
      case 401:
        return base::TimeDelta::FromSeconds(21);
      default:
        return base::TimeDelta::FromSeconds(42);
    }
  }

 private:
  ~ProcessMemoryMetricsEmitterFake() override {}

  ukm::UkmRecorder* ukm_recorder_;
  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterFake);
};

void SetAllocatorDumpMetric(ProcessMemoryDumpPtr& pmd,
                            const std::string& dump_name,
                            const std::string& metric_name,
                            uint64_t value) {
  auto it = pmd->chrome_allocator_dumps.find(dump_name);
  if (it == pmd->chrome_allocator_dumps.end()) {
    memory_instrumentation::mojom::AllocatorMemDumpPtr amd(
        memory_instrumentation::mojom::AllocatorMemDump::New());
    amd->numeric_entries.insert(std::make_pair(metric_name, value));
    pmd->chrome_allocator_dumps.insert(
        std::make_pair(dump_name, std::move(amd)));
  } else {
    it->second->numeric_entries.insert(std::make_pair(metric_name, value));
  }
}

OSMemDumpPtr GetFakeOSMemDump(uint32_t resident_set_kb,
                              uint32_t private_footprint_kb,
                              uint32_t shared_footprint_kb
#if defined(OS_LINUX) || defined(OS_ANDROID)
                              ,
                              uint32_t private_swap_footprint_kb
#endif
                              ) {
  using memory_instrumentation::mojom::VmRegion;

  return memory_instrumentation::mojom::OSMemDump::New(
      resident_set_kb, resident_set_kb /* peak_resident_set_kb */,
      true /* is_peak_rss_resettable */, private_footprint_kb,
      shared_footprint_kb
#if defined(OS_LINUX) || defined(OS_ANDROID)
      ,
      private_swap_footprint_kb
#endif
  );
}

constexpr uint64_t kGpuSharedImagesSizeMB = 32;
constexpr uint64_t kGpuSkiaGpuResourcesMB = 87;
constexpr uint64_t kGpuCommandBufferMB = 240;
constexpr uint64_t kGpuTotalMemory =
    kGpuCommandBufferMB + kGpuSharedImagesSizeMB + kGpuSkiaGpuResourcesMB;

void PopulateBrowserMetrics(GlobalMemoryDumpPtr& global_dump,
                            MetricMap& metrics_mb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::BROWSER;
  SetAllocatorDumpMetric(pmd, "malloc", "effective_size",
                         metrics_mb["Malloc"] * 1024 * 1024);
  // These three categories are required for total gpu memory, but do not
  // have a UKM value set for them, so don't appear in metrics_mb.
  SetAllocatorDumpMetric(pmd, "gpu/gl", "effective_size",
                         kGpuCommandBufferMB * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "gpu/shared_images", "effective_size",
                         kGpuSharedImagesSizeMB * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "skia/gpu_resources", "effective_size",
                         kGpuSkiaGpuResourcesMB * 1024 * 1024);
  OSMemDumpPtr os_dump =
      GetFakeOSMemDump(GetResidentValue(metrics_mb) * 1024,
                       metrics_mb["PrivateMemoryFootprint"] * 1024,
#if defined(OS_LINUX) || defined(OS_ANDROID)
                       // accessing PrivateSwapFootprint on other OSes will
                       // modify metrics_mb to create the value, which leads to
                       // expectation failures.
                       metrics_mb["SharedMemoryFootprint"] * 1024,
                       metrics_mb["PrivateSwapFootprint"] * 1024
#else
                       metrics_mb["SharedMemoryFootprint"] * 1024
#endif
                       );
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

MetricMap GetExpectedBrowserMetrics() {
  return MetricMap(
      {
        {"ProcessType", static_cast<int64_t>(ProcessType::BROWSER)},
#if !defined(OS_MACOSX)
            {"Resident", 10},
#endif
            {"Malloc", 20}, {"PrivateMemoryFootprint", 30},
            {"SharedMemoryFootprint", 35}, {"Uptime", 42},
            {"GpuMemory", kGpuTotalMemory * 1024 * 1024},
#if defined(OS_LINUX) || defined(OS_ANDROID)
            {"PrivateSwapFootprint", 50},
#endif
      });
}

void PopulateRendererMetrics(GlobalMemoryDumpPtr& global_dump,
                             MetricMap& metrics_mb_or_count,
                             base::ProcessId pid) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::RENDERER;
  SetAllocatorDumpMetric(pmd, "malloc", "effective_size",
                         metrics_mb_or_count["Malloc"] * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "partition_alloc", "effective_size",
                         metrics_mb_or_count["PartitionAlloc"] * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "blink_gc", "effective_size",
                         metrics_mb_or_count["BlinkGC"] * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "v8", "effective_size",
                         metrics_mb_or_count["V8"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8", "allocated_objects_size",
      metrics_mb_or_count["V8.AllocatedObjects"] * 1024 * 1024);

  SetAllocatorDumpMetric(pmd, "v8/main", "effective_size",
                         metrics_mb_or_count["V8.Main"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.AllocatedObjects"] * 1024 * 1024);

  SetAllocatorDumpMetric(pmd, "v8/main/heap", "effective_size",
                         metrics_mb_or_count["V8.Main.Heap"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.AllocatedObjects"] * 1024 * 1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/code_large_object_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.CodeLargeObjectSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/code_large_object_space", "allocated_objects_size",
      metrics_mb_or_count
              ["V8.Main.Heap.CodeLargeObjectSpace.AllocatedObjects"] *
          1024 * 1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/code_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.CodeSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/code_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.CodeSpace.AllocatedObjects"] * 1024 *
          1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/large_object_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.LargeObjectSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/large_object_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.LargeObjectSpace.AllocatedObjects"] *
          1024 * 1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/map_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.MapSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/map_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.MapSpace.AllocatedObjects"] * 1024 *
          1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/new_large_object_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.NewLargeObjectSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/new_large_object_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.NewLargeObjectSpace.AllocatedObjects"] *
          1024 * 1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/new_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.NewSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/new_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.NewSpace.AllocatedObjects"] * 1024 *
          1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/old_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.OldSpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/old_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.OldSpace.AllocatedObjects"] * 1024 *
          1024);

  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/read_only_space", "effective_size",
      metrics_mb_or_count["V8.Main.Heap.ReadOnlySpace"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/heap/read_only_space", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.Heap.ReadOnlySpace.AllocatedObjects"] *
          1024 * 1024);

  SetAllocatorDumpMetric(pmd, "v8/main/malloc", "effective_size",
                         metrics_mb_or_count["V8.Main.Malloc"] * 1024 * 1024);

  SetAllocatorDumpMetric(pmd, "v8/workers", "effective_size",
                         metrics_mb_or_count["V8.Workers"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/workers", "allocated_objects_size",
      metrics_mb_or_count["V8.Workers.AllocatedObjects"] * 1024 * 1024);

  SetAllocatorDumpMetric(pmd, "blink_objects/AdSubframe", "object_count",
                         metrics_mb_or_count["NumberOfAdSubframes"]);
  SetAllocatorDumpMetric(pmd, "blink_objects/DetachedScriptState",
                         "object_count",
                         metrics_mb_or_count["NumberOfDetachedScriptStates"]);
  SetAllocatorDumpMetric(pmd, "blink_objects/Document", "object_count",
                         metrics_mb_or_count["NumberOfDocuments"]);
  SetAllocatorDumpMetric(pmd, "blink_objects/Frame", "object_count",
                         metrics_mb_or_count["NumberOfFrames"]);
  SetAllocatorDumpMetric(pmd, "blink_objects/LayoutObject", "object_count",
                         metrics_mb_or_count["NumberOfLayoutObjects"]);
  SetAllocatorDumpMetric(pmd, "blink_objects/Node", "object_count",
                         metrics_mb_or_count["NumberOfNodes"]);
  SetAllocatorDumpMetric(
      pmd, "partition_alloc/partitions/array_buffer", "size",
      metrics_mb_or_count["PartitionAlloc.Partitions.ArrayBuffer"] * 1024 *
          1024);

  OSMemDumpPtr os_dump = GetFakeOSMemDump(
      GetResidentValue(metrics_mb_or_count) * 1024,
      metrics_mb_or_count["PrivateMemoryFootprint"] * 1024,
#if defined(OS_LINUX) || defined(OS_ANDROID)
      // accessing PrivateSwapFootprint on other OSes will
      // modify metrics_mb_or_count to create the value, which leads to
      // expectation failures.
      metrics_mb_or_count["SharedMemoryFootprint"] * 1024,
      metrics_mb_or_count["PrivateSwapFootprint"] * 1024
#else
      metrics_mb_or_count["SharedMemoryFootprint"] * 1024
#endif
      );
  pmd->os_dump = std::move(os_dump);
  pmd->pid = pid;
  global_dump->process_dumps.push_back(std::move(pmd));
}

constexpr int kTestRendererPrivateMemoryFootprint = 130;
constexpr int kTestRendererSharedMemoryFootprint = 135;
constexpr int kNativeLibraryResidentMemoryFootprint = 27560;
constexpr int kNativeLibraryResidentNotOrderedCodeFootprint = 12345;
constexpr int kNativeLibraryNotResidentOrderedCodeFootprint = 23456;

#if !defined(OS_MACOSX)
constexpr int kTestRendererResidentSet = 110;
#endif

constexpr base::ProcessId kTestRendererPid201 = 201;
constexpr base::ProcessId kTestRendererPid202 = 202;
constexpr base::ProcessId kTestRendererPid203 = 203;

MetricMap GetExpectedRendererMetrics() {
  return MetricMap(
      {
        {"ProcessType", static_cast<int64_t>(ProcessType::RENDERER)},
#if !defined(OS_MACOSX)
            {"Resident", kTestRendererResidentSet},
#endif
            {"Malloc", 120},
            {"PrivateMemoryFootprint", kTestRendererPrivateMemoryFootprint},
            {"SharedMemoryFootprint", kTestRendererSharedMemoryFootprint},
            {"PartitionAlloc", 140}, {"BlinkGC", 150}, {"V8", 160},
            {"V8.AllocatedObjects", 70}, {"V8.Main", 100},
            {"V8.Main.AllocatedObjects", 30}, {"V8.Main.Heap", 98},
            {"V8.Main.Heap.AllocatedObjects", 28},
            {"V8.Main.Heap.CodeSpace", 11},
            {"V8.Main.Heap.CodeSpace.AllocatedObjects", 1},
            {"V8.Main.Heap.LargeObjectSpace", 12},
            {"V8.Main.Heap.LargeObjectSpace.AllocatedObjects", 2},
            {"V8.Main.Heap.MapSpace", 13},
            {"V8.Main.Heap.MapSpace.AllocatedObjects", 3},
            {"V8.Main.Heap.NewLargeObjectSpace", 14},
            {"V8.Main.Heap.NewLargeObjectSpace.AllocatedObjects", 4},
            {"V8.Main.Heap.NewSpace", 15},
            {"V8.Main.Heap.NewSpace.AllocatedObjects", 5},
            {"V8.Main.Heap.OldSpace", 16},
            {"V8.Main.Heap.NewSpace.AllocatedObjects", 6},
            {"V8.Main.Heap.ReadOnlySpace", 17},
            {"V8.Main.Heap.ReadOnlySpace.AllocatedObjects", 7},
            {"V8.Main.Malloc", 2}, {"V8.Workers", 60},
            {"V8.Workers.AllocatedObjects", 40}, {"NumberOfExtensions", 0},
            {"Uptime", 42},
#if defined(OS_LINUX) || defined(OS_ANDROID)
            {"PrivateSwapFootprint", 50},
#endif
            {"NumberOfAdSubframes", 28}, {"NumberOfDetachedScriptStates", 11},
            {"NumberOfDocuments", 1}, {"NumberOfFrames", 2},
            {"NumberOfLayoutObjects", 5}, {"NumberOfNodes", 3},
            {"PartitionAlloc.Partitions.ArrayBuffer", 10},
      });
}

void AddPageMetrics(MetricMap& expected_metrics) {
  expected_metrics["IsVisible"] = true;
  expected_metrics["TimeSinceLastNavigation"] = 20;
  expected_metrics["TimeSinceLastVisibilityChange"] = 15;
}

void PopulateGpuMetrics(GlobalMemoryDumpPtr& global_dump,
                        MetricMap& metrics_mb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::GPU;
  SetAllocatorDumpMetric(pmd, "malloc", "effective_size",
                         metrics_mb["Malloc"] * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "gpu/gl", "effective_size",
                         metrics_mb["CommandBuffer"] * 1024 * 1024);
  // These two categories are required for total gpu memory, but do not
  // have a UKM value set for them, so don't appear in metrics_mb.
  SetAllocatorDumpMetric(pmd, "gpu/shared_images", "effective_size",
                         kGpuSharedImagesSizeMB * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "skia/gpu_resources", "effective_size",
                         kGpuSkiaGpuResourcesMB * 1024 * 1024);
  OSMemDumpPtr os_dump =
      GetFakeOSMemDump(GetResidentValue(metrics_mb) * 1024,
                       metrics_mb["PrivateMemoryFootprint"] * 1024,
#if defined(OS_LINUX) || defined(OS_ANDROID)
                       // accessing PrivateSwapFootprint on other OSes will
                       // modify metrics_mb to create the value, which leads to
                       // expectation failures.
                       metrics_mb["SharedMemoryFootprint"] * 1024,
                       metrics_mb["PrivateSwapFootprint"] * 1024
#else
                       metrics_mb["SharedMemoryFootprint"] * 1024
#endif
                       );
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

MetricMap GetExpectedGpuMetrics() {
  return MetricMap(
      {
        {"ProcessType", static_cast<int64_t>(ProcessType::GPU)},
#if !defined(OS_MACOSX)
            {"Resident", 210},
#endif
            {"Malloc", 220}, {"PrivateMemoryFootprint", 230},
            {"SharedMemoryFootprint", 235},
            {"CommandBuffer", kGpuCommandBufferMB}, {"Uptime", 42},
            {"GpuMemory", kGpuTotalMemory * 1024 * 1024},
#if defined(OS_LINUX) || defined(OS_ANDROID)
            {"PrivateSwapFootprint", 50},
#endif
      });
}

void PopulateAudioServiceMetrics(GlobalMemoryDumpPtr& global_dump,
                                 MetricMap& metrics_mb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::UTILITY;
  SetAllocatorDumpMetric(pmd, "malloc", "effective_size",
                         metrics_mb["Malloc"] * 1024 * 1024);
  OSMemDumpPtr os_dump =
      GetFakeOSMemDump(GetResidentValue(metrics_mb) * 1024,
                       metrics_mb["PrivateMemoryFootprint"] * 1024,
#if defined(OS_LINUX) || defined(OS_ANDROID)
                       // accessing PrivateSwapFootprint on other OSes will
                       // modify metrics_mb to create the value, which leads to
                       // expectation failures.
                       metrics_mb["SharedMemoryFootprint"] * 1024,
                       metrics_mb["PrivateSwapFootprint"] * 1024
#else
                       metrics_mb["SharedMemoryFootprint"] * 1024
#endif
                       );
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

MetricMap GetExpectedAudioServiceMetrics() {
  return MetricMap(
      {
        {"ProcessType", static_cast<int64_t>(ProcessType::UTILITY)},
#if !defined(OS_MACOSX)
            {"Resident", 10},
#endif
            {"Malloc", 20}, {"PrivateMemoryFootprint", 30},
            {"SharedMemoryFootprint", 35}, {"Uptime", 42},
#if defined(OS_LINUX) || defined(OS_ANDROID)
            {"PrivateSwapFootprint", 50},
#endif
      });
}

void PopulateMetrics(GlobalMemoryDumpPtr& global_dump,
                     ProcessType ptype,
                     MetricMap& metrics_mb) {
  switch (ptype) {
    case ProcessType::BROWSER:
      PopulateBrowserMetrics(global_dump, metrics_mb);
      return;
    case ProcessType::RENDERER:
      PopulateRendererMetrics(global_dump, metrics_mb, 101);
      return;
    case ProcessType::GPU:
      PopulateGpuMetrics(global_dump, metrics_mb);
      return;
    case ProcessType::UTILITY:
      PopulateAudioServiceMetrics(global_dump, metrics_mb);
      return;
    case ProcessType::PLUGIN:
    case ProcessType::OTHER:
    case ProcessType::ARC:
      break;
  }

  // We shouldn't reach here.
  FAIL() << "Unknown process type case " << ptype << ".";
}

MetricMap GetExpectedProcessMetrics(ProcessType ptype) {
  switch (ptype) {
    case ProcessType::BROWSER:
      return GetExpectedBrowserMetrics();
    case ProcessType::RENDERER:
      return GetExpectedRendererMetrics();
    case ProcessType::GPU:
      return GetExpectedGpuMetrics();
    case ProcessType::UTILITY:
      return GetExpectedAudioServiceMetrics();
    case ProcessType::PLUGIN:
    case ProcessType::OTHER:
    case ProcessType::ARC:
      break;
  }

  // We shouldn't reach here.
  CHECK(false);
  return MetricMap();
}

ProcessInfoVector GetProcessInfo(ukm::TestUkmRecorder& ukm_recorder) {
  ProcessInfoVector process_infos;

  // Process 200 always has no URLs.
  {
    ProcessInfo process_info;
    process_info.pid = 200;
    process_infos.push_back(std::move(process_info));
  }

  // Process kTestRendererPid201 always has 1 URL
  {
    ProcessInfo process_info;
    process_info.pid = kTestRendererPid201;
    ukm::SourceId first_source_id = ukm::UkmRecorder::GetNewSourceID();
    ukm_recorder.UpdateSourceURL(first_source_id,
                                 GURL("http://www.url201.com/"));
    PageInfo page_info;

    page_info.ukm_source_id = first_source_id;
    page_info.tab_id = 201;
    page_info.hosts_main_frame = true;
    page_info.is_visible = true;
    page_info.time_since_last_visibility_change =
        base::TimeDelta::FromSeconds(15);
    page_info.time_since_last_navigation = base::TimeDelta::FromSeconds(20);
    process_info.page_infos.push_back(page_info);
    process_infos.push_back(std::move(process_info));
  }

  // Process kTestRendererPid202 always has 2 URL
  {
    ProcessInfo process_info;
    process_info.pid = kTestRendererPid202;
    ukm::SourceId first_source_id = ukm::UkmRecorder::GetNewSourceID();
    ukm::SourceId second_source_id = ukm::UkmRecorder::GetNewSourceID();
    ukm_recorder.UpdateSourceURL(first_source_id,
                                 GURL("http://www.url2021.com/"));
    ukm_recorder.UpdateSourceURL(second_source_id,
                                 GURL("http://www.url2022.com/"));
    PageInfo page_info1;
    page_info1.ukm_source_id = first_source_id;
    page_info1.tab_id = 2021;
    page_info1.hosts_main_frame = true;
    page_info1.time_since_last_visibility_change =
        base::TimeDelta::FromSeconds(11);
    page_info1.time_since_last_navigation = base::TimeDelta::FromSeconds(21);
    PageInfo page_info2;
    page_info2.ukm_source_id = second_source_id;
    page_info2.tab_id = 2022;
    page_info2.hosts_main_frame = true;
    page_info2.time_since_last_visibility_change =
        base::TimeDelta::FromSeconds(12);
    page_info2.time_since_last_navigation = base::TimeDelta::FromSeconds(22);
    process_info.page_infos.push_back(std::move(page_info1));
    process_info.page_infos.push_back(std::move(page_info2));

    process_infos.push_back(std::move(process_info));
  }
  return process_infos;
}

}  // namespace

class ProcessMemoryMetricsEmitterTest
    : public testing::TestWithParam<ProcessType> {
 public:
  ProcessMemoryMetricsEmitterTest() {}
  ~ProcessMemoryMetricsEmitterTest() override {}

 protected:
  void CheckMemoryUkmEntryMetrics(const std::vector<MetricMap>& expected,
                                  size_t expected_total_memory_entries = 1u) {
    const auto& entries =
        test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
    size_t i = 0;
    size_t total_memory_entries = 0;
    for (const auto* entry : entries) {
      if (test_ukm_recorder_.EntryHasMetric(
              entry, UkmEntry::kTotal2_PrivateMemoryFootprintName)) {
        total_memory_entries++;
        continue;
      }
      if (i >= expected.size()) {
        FAIL() << "Unexpected non-total entry.";
        continue;
      }
      for (const auto& kv : expected[i]) {
        test_ukm_recorder_.ExpectEntryMetric(entry, kv.first, kv.second);
      }
      i++;
    }
    EXPECT_EQ(expected_total_memory_entries, total_memory_entries);
    EXPECT_EQ(expected.size() + expected_total_memory_entries, entries.size());
  }

  content::BrowserTaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterTest);
};

TEST_P(ProcessMemoryMetricsEmitterTest, CollectsSingleProcessUKMs) {
  MetricMap expected_metrics = GetExpectedProcessMetrics(GetParam());

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(global_dump, GetParam(), expected_metrics);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedProcessInfos(ProcessInfoVector());
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  std::vector<MetricMap> expected_entries;
  expected_entries.push_back(expected_metrics);
  CheckMemoryUkmEntryMetrics(expected_entries);
}

INSTANTIATE_TEST_SUITE_P(SinglePtype,
                         ProcessMemoryMetricsEmitterTest,
                         testing::Values(ProcessType::BROWSER,
                                         ProcessType::RENDERER,
                                         ProcessType::GPU,
                                         ProcessType::UTILITY));

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsExtensionProcessUKMs) {
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  expected_metrics["NumberOfExtensions"] = 1;
  expected_metrics["Uptime"] = 21;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateRendererMetrics(global_dump, expected_metrics, 401);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedProcessInfos(ProcessInfoVector());
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  std::vector<MetricMap> expected_entries;
  expected_entries.push_back(expected_metrics);
  CheckMemoryUkmEntryMetrics(expected_entries);
}

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsManyProcessUKMsSingleDump) {
  std::vector<ProcessType> entries_ptypes = {
      ProcessType::BROWSER,  ProcessType::RENDERER, ProcessType::GPU,
      ProcessType::UTILITY,  ProcessType::UTILITY,  ProcessType::GPU,
      ProcessType::RENDERER, ProcessType::BROWSER,
  };

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  std::vector<MetricMap> entries_metrics;
  for (const auto& ptype : entries_ptypes) {
    auto expected_metrics = GetExpectedProcessMetrics(ptype);
    PopulateMetrics(global_dump, ptype, expected_metrics);
    entries_metrics.push_back(expected_metrics);
  }

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedProcessInfos(ProcessInfoVector());
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  CheckMemoryUkmEntryMetrics(entries_metrics);
}

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsManyProcessUKMsManyDumps) {
  std::vector<std::vector<ProcessType>> entries_ptypes = {
      {ProcessType::BROWSER, ProcessType::RENDERER, ProcessType::GPU,
       ProcessType::UTILITY},
      {ProcessType::UTILITY, ProcessType::GPU, ProcessType::RENDERER,
       ProcessType::BROWSER},
  };

  std::vector<MetricMap> entries_metrics;
  for (int i = 0; i < 2; ++i) {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
    GlobalMemoryDumpPtr global_dump(
        memory_instrumentation::mojom::GlobalMemoryDump::New());
    for (const auto& ptype : entries_ptypes[i]) {
      auto expected_metrics = GetExpectedProcessMetrics(ptype);
      PopulateMetrics(global_dump, ptype, expected_metrics);
      expected_metrics.erase("TimeSinceLastVisible");
      entries_metrics.push_back(expected_metrics);
    }
    emitter->ReceivedProcessInfos(ProcessInfoVector());
    emitter->ReceivedMemoryDump(
        true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));
  }

  CheckMemoryUkmEntryMetrics(entries_metrics, 2u);
}

TEST_F(ProcessMemoryMetricsEmitterTest, ReceiveProcessInfoFirst) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedProcessInfos(GetProcessInfo(test_ukm_recorder_));
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 2u);
  int total_memory_entries = 0;
  for (const auto* const entry : entries) {
    if (test_ukm_recorder_.EntryHasMetric(
            entry, UkmEntry::kTotal2_PrivateMemoryFootprintName)) {
      total_memory_entries++;
    } else {
      test_ukm_recorder_.ExpectEntrySourceHasUrl(
          entry, GURL("http://www.url201.com/"));
    }
  }
  EXPECT_EQ(1, total_memory_entries);

  std::vector<MetricMap> expected_entries;
  expected_entries.push_back(expected_metrics);
  CheckMemoryUkmEntryMetrics(expected_entries);
}

TEST_F(ProcessMemoryMetricsEmitterTest, ReceiveProcessInfoSecond) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));
  emitter->ReceivedProcessInfos(GetProcessInfo(test_ukm_recorder_));

  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 2u);
  int total_memory_entries = 0;
  for (const auto* const entry : entries) {
    if (test_ukm_recorder_.EntryHasMetric(
            entry, UkmEntry::kTotal2_PrivateMemoryFootprintName)) {
      total_memory_entries++;
    } else {
      test_ukm_recorder_.ExpectEntrySourceHasUrl(
          entry, GURL("http://www.url201.com/"));
    }
  }
  EXPECT_EQ(1, total_memory_entries);

  std::vector<MetricMap> expected_entries;
  expected_entries.push_back(expected_metrics);
  CheckMemoryUkmEntryMetrics(expected_entries);
}

TEST_F(ProcessMemoryMetricsEmitterTest, ProcessInfoHasTwoURLs) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid203);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));
  emitter->ReceivedProcessInfos(GetProcessInfo(test_ukm_recorder_));

  // Check that if there are two URLs, neither is emitted.
  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  int total_memory_entries = 0;
  int entries_with_urls = 0;
  for (const auto* const entry : entries) {
    if (test_ukm_recorder_.EntryHasMetric(
            entry, UkmEntry::kTotal2_PrivateMemoryFootprintName)) {
      total_memory_entries++;
    } else {
      if (test_ukm_recorder_.GetSourceForSourceId(entry->source_id)) {
        entries_with_urls++;
        test_ukm_recorder_.ExpectEntrySourceHasUrl(
            entry, GURL("http://www.url201.com/"));
      }
    }
  }
  EXPECT_EQ(4u, entries.size());
  EXPECT_EQ(1, total_memory_entries);
  EXPECT_EQ(1, entries_with_urls);
}

TEST_F(ProcessMemoryMetricsEmitterTest, RendererAndTotalHistogramsAreRecorded) {
  // Take a snapshot of the current state of the histograms.
  base::HistogramTester histograms;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  global_dump->aggregated_metrics =
      memory_instrumentation::mojom::AggregatedMetrics::New();
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);
  global_dump->aggregated_metrics->native_library_resident_kb =
      kNativeLibraryResidentMemoryFootprint;
  global_dump->aggregated_metrics->native_library_not_resident_ordered_kb =
      kNativeLibraryNotResidentOrderedCodeFootprint;
  global_dump->aggregated_metrics->native_library_resident_not_ordered_kb =
      kNativeLibraryResidentNotOrderedCodeFootprint;

  // No histograms should have been recorded yet.
  histograms.ExpectTotalCount("Memory.Renderer.PrivateMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Renderer.SharedMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Renderer.ResidentSet", 0);

  histograms.ExpectTotalCount("Memory.Total.PrivateMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Total.RendererPrivateMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Total.SharedMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Total.ResidentSet", 0);
  histograms.ExpectTotalCount(
      "Memory.NativeLibrary.MappedAndResidentMemoryFootprint2", 0);
  histograms.ExpectTotalCount(
      "Memory.NativeLibrary.NotResidentOrderedCodeMemoryFootprint", 0);
  histograms.ExpectTotalCount(
      "Memory.NativeLibrary.ResidentNotOrderedCodeMemoryFootprint", 0);

  // Simulate some metrics emission.
  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));
  emitter->ReceivedProcessInfos(GetProcessInfo(test_ukm_recorder_));

  // Check that the expected values have been emitted to histograms.
  histograms.ExpectUniqueSample("Memory.Renderer.PrivateMemoryFootprint",
                                kTestRendererPrivateMemoryFootprint, 2);
  histograms.ExpectUniqueSample("Memory.Renderer.SharedMemoryFootprint",
                                kTestRendererSharedMemoryFootprint, 2);
#if defined(OS_MACOSX)
  histograms.ExpectTotalCount("Memory.Renderer.ResidentSet", 0);
#else
  histograms.ExpectUniqueSample("Memory.Renderer.ResidentSet",
                                kTestRendererResidentSet, 2);
#endif

  histograms.ExpectUniqueSample("Memory.Total.PrivateMemoryFootprint",
                                2 * kTestRendererPrivateMemoryFootprint, 1);
  histograms.ExpectUniqueSample("Memory.Total.RendererPrivateMemoryFootprint",
                                2 * kTestRendererPrivateMemoryFootprint, 1);
  histograms.ExpectUniqueSample("Memory.Total.SharedMemoryFootprint",
                                2 * kTestRendererSharedMemoryFootprint, 1);
#if defined(OS_MACOSX)
  histograms.ExpectTotalCount("Memory.Total.ResidentSet", 0);
#else
  histograms.ExpectUniqueSample("Memory.Total.ResidentSet",
                                2 * kTestRendererResidentSet, 1);
#endif
  histograms.ExpectUniqueSample(
      "Memory.NativeLibrary.MappedAndResidentMemoryFootprint2",
      kNativeLibraryResidentMemoryFootprint, 1);
  histograms.ExpectUniqueSample(
      "Memory.NativeLibrary.NotResidentOrderedCodeMemoryFootprint",
      kNativeLibraryNotResidentOrderedCodeFootprint, 1);
  histograms.ExpectUniqueSample(
      "Memory.NativeLibrary.ResidentNotOrderedCodeMemoryFootprint",
      kNativeLibraryResidentNotOrderedCodeFootprint, 1);
}

TEST_F(ProcessMemoryMetricsEmitterTest, MainFramePMFEmitted) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::Memory_TabFootprint::kEntryName);
  ASSERT_EQ(entries.size(), 0u);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake(test_ukm_recorder_));
  emitter->ReceivedMemoryDump(
      true, GlobalMemoryDump::MoveFrom(std::move(global_dump)));
  emitter->ReceivedProcessInfos(GetProcessInfo(test_ukm_recorder_));

  entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::Memory_TabFootprint::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries.front();
  ASSERT_TRUE(test_ukm_recorder_.EntryHasMetric(
      entry, ukm::builders::Memory_TabFootprint::kMainFrameProcessPMFName));
}
