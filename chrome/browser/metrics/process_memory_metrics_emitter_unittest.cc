// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#endif

using GlobalMemoryDump = memory_instrumentation::GlobalMemoryDump;
using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using HistogramProcessType = memory_instrumentation::HistogramProcessType;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;
using ProcessType = memory_instrumentation::mojom::ProcessType;

namespace {

using performance_manager::FrameNodeImpl;
using performance_manager::PageNodeImpl;
using performance_manager::TestNodeWrapper;
using performance_manager::TestProcessNodeImpl;

using MetricMap = base::flat_map<const char*, int64_t>;
using UkmEntry = ukm::builders::Memory_Experimental;

int GetResidentValue(const MetricMap& metric_map) {
  auto it = metric_map.find("Resident");
  EXPECT_NE(it, metric_map.end());
  return it->second;
}

// Provide fake to surface ReceivedMemoryDump and GetProcessToPageInfoMap to
// public visibility.
class ProcessMemoryMetricsEmitterFake : public ProcessMemoryMetricsEmitter {
 public:
  explicit ProcessMemoryMetricsEmitterFake(
      ukm::TestAutoSetUkmRecorder& test_ukm_recorder)
      : ukm_recorder_(&test_ukm_recorder) {}

  ProcessMemoryMetricsEmitterFake(
      base::ProcessId pid_scope,
      ukm::TestAutoSetUkmRecorder& test_ukm_recorder)
      : ProcessMemoryMetricsEmitter(pid_scope),
        ukm_recorder_(&test_ukm_recorder) {}

  ProcessMemoryMetricsEmitterFake(const ProcessMemoryMetricsEmitterFake&) =
      delete;
  ProcessMemoryMetricsEmitterFake& operator=(
      const ProcessMemoryMetricsEmitterFake&) = delete;

  using ProcessMemoryMetricsEmitter::GetProcessToPageInfoMap;
  using ProcessMemoryMetricsEmitter::ReceivedMemoryDump;

  ukm::UkmRecorder* GetUkmRecorder() override { return ukm_recorder_; }

  int GetNumberOfExtensions(base::ProcessId pid) override {
    switch (pid) {
      case 401:
        return 1;
      default:
        return 0;
    }
  }

  std::optional<base::TimeDelta> GetProcessUptime(
      base::TimeTicks now,
      const ProcessInfo* process_info) override {
    if (process_info && process_info->pid == 401) {
      return base::Seconds(21);
    }
    return base::Seconds(42);
  }

 private:
  ~ProcessMemoryMetricsEmitterFake() override = default;

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
                              ,
                              uint32_t private_swap_footprint_kb,
                              uint32_t mappings_count,
                              uint32_t pss_kb,
                              uint32_t swap_pss_kb
#endif
) {
  using memory_instrumentation::mojom::VmRegion;

  return memory_instrumentation::mojom::OSMemDump::New(
      resident_set_kb, /*peak_resident_set_kb=*/resident_set_kb,
      /*is_peak_rss_resettable=*/true, private_footprint_kb, shared_footprint_kb
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      ,
      private_swap_footprint_kb, mappings_count, pss_kb, swap_pss_kb
#endif
  );
}

OSMemDumpPtr GetFakeOSMemDump(MetricMap& metrics_mb) {
  return GetFakeOSMemDump(
      /*resident_set_kb=*/GetResidentValue(metrics_mb) * 1024,
      /*private_footprint_kb=*/metrics_mb["PrivateMemoryFootprint"] * 1024,
      /*shared_footprint_kb=*/metrics_mb["SharedMemoryFootprint"] * 1024
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      // accessing PrivateSwapFootprint on other OSes will
      // modify metrics_mb to create the value, which leads
      // to expectation failures.
      ,
      /*private_swap_footprint_kb=*/metrics_mb["PrivateSwapFootprint"] * 1024,
      /* mappings_count= */ metrics_mb.contains("MappingsCount")
          ? metrics_mb["MappingsCount"]
          : 0,
      /*pss_kb=*/metrics_mb.contains("Pss2") ? metrics_mb["Pss2"] * 1024 : 0,
      /*swap_pss_kb=*/metrics_mb.contains("SwapPss2")
          ? metrics_mb["SwapPss2"] * 1024
          : 0
#endif
  );
}

constexpr uint64_t kGpuSharedImagesSizeMB = 32;
constexpr uint64_t kGpuSkiaGpuResourcesMB = 87;
constexpr uint64_t kGpuVulkanResourcesMB = 120;
constexpr uint64_t kGpuVulkanUsedResourcesMB = 52;
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
  OSMemDumpPtr os_dump = GetFakeOSMemDump(metrics_mb);
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

MetricMap GetExpectedBrowserMetrics() {
  return MetricMap({
      {"ProcessType", static_cast<int64_t>(ProcessType::BROWSER)},
      {"Resident", 10},
      {"Malloc", 20},
      {"PrivateMemoryFootprint", 30},
      {"SharedMemoryFootprint", 35},
      {"Uptime", 42},
      {"GpuMemory", kGpuTotalMemory * 1024 * 1024},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
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
  SetAllocatorDumpMetric(
      pmd, "malloc/allocated_objects", "effective_size",
      metrics_mb_or_count["Malloc.AllocatedObjects"] * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "partition_alloc", "effective_size",
                         metrics_mb_or_count["PartitionAlloc"] * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "blink_gc", "effective_size",
                         metrics_mb_or_count["BlinkGC"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "blink_gc", "allocated_objects_size",
      metrics_mb_or_count["BlinkGC.AllocatedObjects"] * 1024 * 1024);
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
  SetAllocatorDumpMetric(
      pmd, "v8/main/global_handles", "effective_size",
      metrics_mb_or_count["V8.Main.GlobalHandles"] * 1024 * 1024);
  SetAllocatorDumpMetric(
      pmd, "v8/main/global_handles", "allocated_objects_size",
      metrics_mb_or_count["V8.Main.GlobalHandles.AllocatedObjects"] * 1024 *
          1024);

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

  OSMemDumpPtr os_dump = GetFakeOSMemDump(metrics_mb_or_count);
  pmd->os_dump = std::move(os_dump);
  pmd->pid = pid;
  global_dump->process_dumps.push_back(std::move(pmd));
}

constexpr int kTestRendererPrivateMemoryFootprint = 130;
constexpr int kTestRendererMalloc = 120;
constexpr int kTestRendererBlinkGC = 150;
constexpr int kTestRendererBlinkGCFragmentation = 10;
constexpr int kTestRendererSharedMemoryFootprint = 135;
constexpr int kNativeLibraryResidentMemoryFootprint = 27560;
constexpr int kNativeLibraryResidentNotOrderedCodeFootprint = 12345;
constexpr int kNativeLibraryNotResidentOrderedCodeFootprint = 23456;
constexpr int kTestRendererResidentSet = 110;
constexpr base::ProcessId kTestRendererPid201 = 201;
constexpr base::ProcessId kTestRendererPid202 = 202;
constexpr base::ProcessId kTestRendererPid203 = 203;

MetricMap GetExpectedRendererMetrics() {
  return MetricMap({
      {"ProcessType", static_cast<int64_t>(ProcessType::RENDERER)},
      {"Resident", kTestRendererResidentSet},
      {"Malloc", kTestRendererMalloc},
      {"PrivateMemoryFootprint", kTestRendererPrivateMemoryFootprint},
      {"SharedMemoryFootprint", kTestRendererSharedMemoryFootprint},
      {"PartitionAlloc", 140},
      {"BlinkGC", 150},
      {"BlinkGC.AllocatedObjects", 140},
      {"V8", 160},
      {"V8.AllocatedObjects", 70},
      {"V8.Main", 100},
      {"V8.Main.AllocatedObjects", 30},
      {"V8.Main.Heap", 98},
      {"V8.Main.GlobalHandles", 3},
      {"V8.Main.GlobalHandles.AllocatedObjects", 2},
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
      {"V8.Main.Malloc", 2},
      {"V8.Workers", 60},
      {"V8.Workers.AllocatedObjects", 40},
      {"NumberOfExtensions", 0},
      {"Uptime", 42},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      {"PrivateSwapFootprint", 50},
#endif
      {"NumberOfAdSubframes", 28},
      {"NumberOfDetachedScriptStates", 11},
      {"NumberOfDocuments", 1},
      {"NumberOfFrames", 2},
      {"NumberOfLayoutObjects", 5},
      {"NumberOfNodes", 3},
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
  // These categories are required for total gpu memory, but do not have a UKM
  // value set for them, so don't appear in metrics_mb.
  SetAllocatorDumpMetric(pmd, "gpu/shared_images", "effective_size",
                         kGpuSharedImagesSizeMB * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "skia/gpu_resources", "effective_size",
                         kGpuSkiaGpuResourcesMB * 1024 * 1024);

  SetAllocatorDumpMetric(pmd, "gpu/vulkan", "allocated_size",
                         kGpuVulkanResourcesMB * 1024 * 1024);
  SetAllocatorDumpMetric(pmd, "gpu/vulkan", "used_size",
                         kGpuVulkanUsedResourcesMB * 1024 * 1024);

  OSMemDumpPtr os_dump = GetFakeOSMemDump(metrics_mb);
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

MetricMap GetExpectedGpuMetrics() {
  return MetricMap({
      {"ProcessType", static_cast<int64_t>(ProcessType::GPU)},
      {"Resident", 210},
      {"Malloc", 220},
      {"PrivateMemoryFootprint", 230},
      {"SharedMemoryFootprint", 235},
      {"CommandBuffer", kGpuCommandBufferMB},
      {"Uptime", 42},
      {"GpuMemory", kGpuTotalMemory * 1024 * 1024},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      {"PrivateSwapFootprint", 50},
#endif
  });
}

void PopulateUtilityMetrics(GlobalMemoryDumpPtr& global_dump,
                            MetricMap& metrics_mb,
                            const std::optional<std::string>& service_name) {
  auto pmd(memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::UTILITY;
  if (service_name.has_value()) {
    pmd->service_name = service_name.value();
  }

  SetAllocatorDumpMetric(pmd, "malloc", "effective_size",
                         metrics_mb["Malloc"] * 1024 * 1024);
  OSMemDumpPtr os_dump = GetFakeOSMemDump(metrics_mb);
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

MetricMap GetExpectedAudioServiceMetrics() {
  return MetricMap({
      {"ProcessType", static_cast<int64_t>(ProcessType::UTILITY)},
      {"Resident", 10},
      {"Malloc", 20},
      {"PrivateMemoryFootprint", 30},
      {"SharedMemoryFootprint", 35},
      {"Uptime", 42},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      {"PrivateSwapFootprint", 50},
#endif
  });
}

MetricMap GetExpectedCdmServiceMetrics() {
  return MetricMap({
    {"ProcessType", static_cast<int64_t>(ProcessType::UTILITY)},
        {"Resident", 11}, {"PrivateMemoryFootprint", 21},
        {"SharedMemoryFootprint", 31},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
        {"PrivateSwapFootprint", 41},
#endif
  });
}

#if BUILDFLAG(IS_WIN)
MetricMap GetExpectedMediaFoundationServiceMetrics() {
  return MetricMap({{"ProcessType", static_cast<int64_t>(ProcessType::UTILITY)},
                    {"Resident", 12},
                    {"PrivateMemoryFootprint", 22},
                    {"SharedMemoryFootprint", 32}});
}
#endif

MetricMap GetExpectedPaintPreviewCompositorMetrics() {
  return MetricMap({
      {"ProcessType", static_cast<int64_t>(ProcessType::UTILITY)},
      {"Resident", 10},
      {"PrivateMemoryFootprint", 30},
      {"SharedMemoryFootprint", 35},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      {"PrivateSwapFootprint", 50},
#endif
  });
}

void PopulateMetrics(GlobalMemoryDumpPtr& global_dump,
                     HistogramProcessType ptype,
                     MetricMap& metrics_mb) {
  switch (ptype) {
    case HistogramProcessType::kAudioService:
      PopulateUtilityMetrics(global_dump, metrics_mb,
                             /*service_name=*/std::nullopt);
      return;
    case HistogramProcessType::kBrowser:
      PopulateBrowserMetrics(global_dump, metrics_mb);
      return;
    case HistogramProcessType::kCdmService:
      PopulateUtilityMetrics(global_dump, metrics_mb,
                             media::mojom::CdmServiceBroker::Name_);
      return;
    case HistogramProcessType::kGpu:
      PopulateGpuMetrics(global_dump, metrics_mb);
      return;
#if BUILDFLAG(IS_WIN)
    case HistogramProcessType::kMediaFoundationService:
      PopulateUtilityMetrics(global_dump, metrics_mb,
                             media::mojom::MediaFoundationServiceBroker::Name_);
      return;
#endif
    case HistogramProcessType::kPaintPreviewCompositor:
      PopulateUtilityMetrics(
          global_dump, metrics_mb,
          paint_preview::mojom::PaintPreviewCompositorCollection::Name_);
      return;
    case HistogramProcessType::kRenderer:
      PopulateRendererMetrics(global_dump, metrics_mb, 101);
      return;
    case HistogramProcessType::kExtension:
    case HistogramProcessType::kNetworkService:
    case HistogramProcessType::kUtility:
      break;
  }

  // We shouldn't reach here.
  NOTREACHED();
}

MetricMap GetExpectedProcessMetrics(HistogramProcessType ptype) {
  switch (ptype) {
    case HistogramProcessType::kAudioService:
      return GetExpectedAudioServiceMetrics();
    case HistogramProcessType::kBrowser:
      return GetExpectedBrowserMetrics();
    case HistogramProcessType::kCdmService:
      return GetExpectedCdmServiceMetrics();
    case HistogramProcessType::kGpu:
      return GetExpectedGpuMetrics();
#if BUILDFLAG(IS_WIN)
    case HistogramProcessType::kMediaFoundationService:
      return GetExpectedMediaFoundationServiceMetrics();
#endif
    case HistogramProcessType::kPaintPreviewCompositor:
      return GetExpectedPaintPreviewCompositorMetrics();
    case HistogramProcessType::kRenderer:
      return GetExpectedRendererMetrics();
    case HistogramProcessType::kExtension:
    case HistogramProcessType::kNetworkService:
    case HistogramProcessType::kUtility:
      break;
  }

  // We shouldn't reach here.
  NOTREACHED();
}

}  // namespace

class ProcessMemoryMetricsEmitterTest
    : public performance_manager::GraphTestHarness,
      public ::testing::WithParamInterface<HistogramProcessType> {
 public:
  ProcessMemoryMetricsEmitterTest() = default;

  ProcessMemoryMetricsEmitterTest(const ProcessMemoryMetricsEmitterTest&) =
      delete;
  ProcessMemoryMetricsEmitterTest& operator=(
      const ProcessMemoryMetricsEmitterTest&) = delete;

  ~ProcessMemoryMetricsEmitterTest() override = default;

 protected:
  // TestNodeWrapper<NodeType> classes don't have a common base class, so they
  // need to be held in a variant.
  using AnyNodeWrapper = std::variant<TestNodeWrapper<FrameNodeImpl>,
                                      TestNodeWrapper<PageNodeImpl>,
                                      TestNodeWrapper<TestProcessNodeImpl>>;

  // Creates a set of graph nodes for complex tests.
  std::vector<AnyNodeWrapper> CreateTestGraphNodes() {
    std::vector<AnyNodeWrapper> nodes;

    // Process 200 always has no URLs. Tests don't call PopulateRendererMetrics
    // for this PID so it should never appear in memory dumps. (This tests that
    // nothing gets confused if a process exists but doesn't appear in the
    // dumps, such as if it was created but not registered with the memory
    // instrumentation yet when the dump request was sent.)
    {
      auto process_node = CreateNode<TestProcessNodeImpl>();
      process_node->SetProcessWithPid(200);
      nodes.push_back(std::move(process_node));
    }

    // Process kTestRendererPid201 always has 1 URL
    {
      auto process_node = CreateNode<TestProcessNodeImpl>();
      process_node->SetProcessWithPid(kTestRendererPid201);
      ukm::SourceId first_source_id = ukm::UkmRecorder::GetNewSourceID();
      test_ukm_recorder_.UpdateSourceURL(first_source_id,
                                         GURL("http://www.url201.com/"));

      auto page_node = CreateNode<PageNodeImpl>();
      page_node->SetUkmSourceId(first_source_id);
      page_node->SetIsVisible(true);
      auto frame_node =
          CreateFrameNodeAutoId(process_node.get(), page_node.get());

      nodes.push_back(std::move(process_node));
      nodes.push_back(std::move(page_node));
      nodes.push_back(std::move(frame_node));
    }

    // Process kTestRendererPid202 always has 2 URL
    {
      auto process_node = CreateNode<TestProcessNodeImpl>();
      process_node->SetProcessWithPid(kTestRendererPid202);
      ukm::SourceId first_source_id = ukm::UkmRecorder::GetNewSourceID();
      ukm::SourceId second_source_id = ukm::UkmRecorder::GetNewSourceID();
      test_ukm_recorder_.UpdateSourceURL(first_source_id,
                                         GURL("http://www.url2021.com/"));
      test_ukm_recorder_.UpdateSourceURL(second_source_id,
                                         GURL("http://www.url2022.com/"));

      auto page_node1 = CreateNode<PageNodeImpl>();
      page_node1->SetUkmSourceId(first_source_id);
      auto frame_node1 =
          CreateFrameNodeAutoId(process_node.get(), page_node1.get());

      auto page_node2 = CreateNode<PageNodeImpl>();
      page_node2->SetUkmSourceId(second_source_id);
      auto frame_node2 =
          CreateFrameNodeAutoId(process_node.get(), page_node2.get());

      nodes.push_back(std::move(process_node));
      nodes.push_back(std::move(page_node1));
      nodes.push_back(std::move(frame_node1));
      nodes.push_back(std::move(page_node2));
      nodes.push_back(std::move(frame_node2));
    }

    return nodes;
  }

  void CheckMemoryUkmEntryMetrics(const std::vector<MetricMap>& expected,
                                  size_t expected_total_memory_entries = 1u) {
    const auto& entries =
        test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
    size_t i = 0;
    size_t total_memory_entries = 0;
    for (const ukm::mojom::UkmEntry* entry : entries) {
      if (test_ukm_recorder_.EntryHasMetric(
              entry, UkmEntry::kTotal2_PrivateMemoryFootprintName)) {
        total_memory_entries++;
        continue;
      }
      if (i >= expected.size()) {
        FAIL() << "Unexpected non-total entry.";
      }
      for (const auto& kv : expected[i]) {
        test_ukm_recorder_.ExpectEntryMetric(entry, kv.first, kv.second);
      }
      i++;
    }
    EXPECT_EQ(expected_total_memory_entries, total_memory_entries);
    EXPECT_EQ(expected.size() + expected_total_memory_entries, entries.size());
  }

  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_P(ProcessMemoryMetricsEmitterTest, CollectsSingleProcessUKMs) {
  MetricMap expected_metrics = GetExpectedProcessMetrics(GetParam());

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(global_dump, GetParam(), expected_metrics);

  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  std::vector<MetricMap> expected_entries;
  expected_entries.push_back(expected_metrics);
  CheckMemoryUkmEntryMetrics(expected_entries);
}

INSTANTIATE_TEST_SUITE_P(
    SinglePtype,
    ProcessMemoryMetricsEmitterTest,
    testing::Values(HistogramProcessType::kAudioService,
                    HistogramProcessType::kBrowser,
                    HistogramProcessType::kCdmService,
                    HistogramProcessType::kGpu,
#if BUILDFLAG(IS_WIN)
                    HistogramProcessType::kMediaFoundationService,
#endif
                    HistogramProcessType::kPaintPreviewCompositor,
                    HistogramProcessType::kRenderer));

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsExtensionProcessUKMs) {
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  expected_metrics["NumberOfExtensions"] = 1;
  expected_metrics["Uptime"] = 21;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateRendererMetrics(global_dump, expected_metrics, 401);

  // Need a ProcessInfo with the correct PID for the GetProcessUptime fake,
  // which will override the `launch_time` with fixed test data.
  auto process_node = CreateNode<TestProcessNodeImpl>();
  process_node->SetProcessWithPid(401);

  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  std::vector<MetricMap> expected_entries;
  expected_entries.push_back(expected_metrics);
  CheckMemoryUkmEntryMetrics(expected_entries);
}

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsManyProcessUKMsSingleDump) {
  std::vector<HistogramProcessType> entries_ptypes = {
      HistogramProcessType::kBrowser,
      HistogramProcessType::kRenderer,
      HistogramProcessType::kGpu,
      HistogramProcessType::kAudioService,
      HistogramProcessType::kCdmService,
      HistogramProcessType::kPaintPreviewCompositor,
      HistogramProcessType::kPaintPreviewCompositor,
      HistogramProcessType::kCdmService,
      HistogramProcessType::kAudioService,
      HistogramProcessType::kGpu,
      HistogramProcessType::kRenderer,
      HistogramProcessType::kBrowser,
  };

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  std::vector<MetricMap> entries_metrics;
  for (const auto& ptype : entries_ptypes) {
    auto expected_metrics = GetExpectedProcessMetrics(ptype);
    PopulateMetrics(global_dump, ptype, expected_metrics);
    entries_metrics.push_back(expected_metrics);
  }

  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  CheckMemoryUkmEntryMetrics(entries_metrics);
}

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsManyProcessUKMsManyDumps) {
  std::vector<std::vector<HistogramProcessType>> entries_ptypes = {
      {HistogramProcessType::kBrowser, HistogramProcessType::kRenderer,
       HistogramProcessType::kGpu,
       HistogramProcessType::kPaintPreviewCompositor,
       HistogramProcessType::kCdmService, HistogramProcessType::kAudioService},
      {HistogramProcessType::kBrowser, HistogramProcessType::kRenderer,
       HistogramProcessType::kGpu,
       HistogramProcessType::kPaintPreviewCompositor,
       HistogramProcessType::kCdmService, HistogramProcessType::kAudioService},
  };

  std::vector<MetricMap> entries_metrics;
  for (int i = 0; i < 2; ++i) {
    auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
        test_ukm_recorder_);
    GlobalMemoryDumpPtr global_dump(
        memory_instrumentation::mojom::GlobalMemoryDump::New());
    for (const auto& ptype : entries_ptypes[i]) {
      auto expected_metrics = GetExpectedProcessMetrics(ptype);
      PopulateMetrics(global_dump, ptype, expected_metrics);
      expected_metrics.erase("TimeSinceLastVisible");
      entries_metrics.push_back(expected_metrics);
    }
    emitter->ReceivedMemoryDump(
        emitter->GetProcessToPageInfoMap(graph()),
        memory_instrumentation::mojom::RequestOutcome::kSuccess,
        GlobalMemoryDump::MoveFrom(std::move(global_dump)));
  }

  CheckMemoryUkmEntryMetrics(entries_metrics, 2u);
}

TEST_F(ProcessMemoryMetricsEmitterTest, GlobalDumpFailed) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  auto nodes = CreateTestGraphNodes();

  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kTimeout,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  // Should not record any metrics since the memory dump failed, and don't
  // crash.
  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 0u);
}

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsRendererProcessUKMs) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid203);

  auto nodes = CreateTestGraphNodes();

  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  int total_memory_entries = 0;
  int entries_with_urls = 0;
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    if (test_ukm_recorder_.EntryHasMetric(
            entry, UkmEntry::kTotal2_PrivateMemoryFootprintName)) {
      // "Total2.*" metrics are recorded with an anymous source ID.
      total_memory_entries++;
      EXPECT_FALSE(test_ukm_recorder_.GetSourceForSourceId(entry->source_id));
    } else if (test_ukm_recorder_.GetSourceForSourceId(entry->source_id)) {
      // This is kTestRendererPid201, which has one URL.
      entries_with_urls++;
      // Basic process UKM's are recorded with the URL.
      test_ukm_recorder_.ExpectEntrySourceHasUrl(
          entry, GURL("http://www.url201.com/"));
      EXPECT_TRUE(
          test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kProcessTypeName));
      // So are per-page UKM's.
      EXPECT_TRUE(
          test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kIsVisibleName));
    } else {
      // This is kTestRendererPid202, which has two URL's, or
      // kTestRendererPid203, which has no ProcessInfo.
      // Basic process UKM's are recorded with an anymous source ID.
      EXPECT_TRUE(
          test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kProcessTypeName));
      // Per-page UKM's are not.
      EXPECT_FALSE(
          test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kIsVisibleName));
    }
  }
  EXPECT_EQ(4u, entries.size());
  EXPECT_EQ(1, total_memory_entries);
  EXPECT_EQ(1, entries_with_urls);
}

TEST_F(ProcessMemoryMetricsEmitterTest,
       SingleMeasurement_ProcessInfoHasOneURL) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid203);

  auto nodes = CreateTestGraphNodes();

  auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
      kTestRendererPid201, test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(entries.size(), 1u);
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    // "Total2.*" metrics should not be recorded unless measuring all processes.
    EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
        entry, UkmEntry::kTotal2_PrivateMemoryFootprintName));
    // Only measuring kTestRendererPid201, which has one URL.
    // Basic process UKM's are recorded with the URL.
    test_ukm_recorder_.ExpectEntrySourceHasUrl(entry,
                                               GURL("http://www.url201.com/"));
    EXPECT_TRUE(
        test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kProcessTypeName));
    // So are per-page UKM's.
    EXPECT_TRUE(
        test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kIsVisibleName));
  }
}

TEST_F(ProcessMemoryMetricsEmitterTest,
       SingleMeasurement_ProcessInfoHasTwoURLs) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid203);

  auto nodes = CreateTestGraphNodes();

  auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
      kTestRendererPid202, test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  // Check that if there are two URLs, neither is emitted.
  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    // "Total2.*" metrics should not be recorded unless measuring all processes.
    EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
        entry, UkmEntry::kTotal2_PrivateMemoryFootprintName));
    // Only measuring kTestRendererPid202, which has two URL's.
    EXPECT_FALSE(test_ukm_recorder_.GetSourceForSourceId(entry->source_id));
    // Basic process UKM's are recorded with an anymous source ID.
    EXPECT_TRUE(
        test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kProcessTypeName));
    // Per-page UKM's are not.
    EXPECT_FALSE(
        test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kIsVisibleName));
  }
  EXPECT_EQ(1u, entries.size());
}

TEST_F(ProcessMemoryMetricsEmitterTest, SingleMeasurement_ProcessInfoNotFound) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid203);

  auto nodes = CreateTestGraphNodes();

  auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
      kTestRendererPid203, test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  auto entries = test_ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    // "Total2.*" metrics should not be recorded unless measuring all processes.
    EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
        entry, UkmEntry::kTotal2_PrivateMemoryFootprintName));
    // Only measuring kTestRendererPid203, which has no ProcessInfo.
    EXPECT_FALSE(test_ukm_recorder_.GetSourceForSourceId(entry->source_id));
    // Basic process UKM's are recorded with an anymous source ID.
    EXPECT_TRUE(
        test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kProcessTypeName));
    // Per-page UKM's are not.
    EXPECT_FALSE(
        test_ukm_recorder_.EntryHasMetric(entry, UkmEntry::kIsVisibleName));
  }
  EXPECT_EQ(1u, entries.size());
}

TEST_F(ProcessMemoryMetricsEmitterTest, RendererAndTotalHistogramsAreRecorded) {
  // Take a snapshot of the current state of the histograms.
  base::HistogramTester histograms;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  global_dump->aggregated_metrics =
      memory_instrumentation::mojom::AggregatedMetrics::New();
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  constexpr size_t kTestMappingsCount = 12;
  constexpr size_t kTestPss = 100;
  constexpr size_t kTestSwapPss = 57;
  expected_metrics["MappingsCount"] = kTestMappingsCount;
  expected_metrics["Pss2"] = kTestPss;
  expected_metrics["SwapPss2"] = kTestSwapPss;
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid202);

  constexpr uint64_t kMiB = 1024 * 1024;
  constexpr uint64_t kKiB = 1024;
  SetAllocatorDumpMetric(global_dump->process_dumps[0], "cc/tile_memory",
                         "size", 12 * kMiB);
  SetAllocatorDumpMetric(global_dump->process_dumps[1], "cc/tile_memory",
                         "size", 22 * kMiB);

  SetAllocatorDumpMetric(global_dump->process_dumps[0], "canvas/hibernated",
                         "size", 22 * kMiB);
  SetAllocatorDumpMetric(global_dump->process_dumps[1], "canvas/hibernated",
                         "size", 12 * kMiB);

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

  histograms.ExpectTotalCount("Memory.Total.HibernatedCanvas.Size", 0);
  histograms.ExpectTotalCount("Memory.Total.PrivateMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Total.RendererPrivateMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Total.RendererMalloc", 0);
  histograms.ExpectTotalCount("Memory.Total.RendererBlinkGC", 0);
  histograms.ExpectTotalCount("Memory.Total.RendererBlinkGC.Fragmentation", 0);
  histograms.ExpectTotalCount("Memory.Total.SharedMemoryFootprint", 0);
  histograms.ExpectTotalCount("Memory.Total.ResidentSet", 0);
  histograms.ExpectTotalCount(
      "Memory.NativeLibrary.MappedAndResidentMemoryFootprint3", 0);
  histograms.ExpectTotalCount(
      "Memory.NativeLibrary.NotResidentOrderedCodeMemoryFootprint", 0);
  histograms.ExpectTotalCount(
      "Memory.NativeLibrary.ResidentNotOrderedCodeMemoryFootprint", 0);
#if BUILDFLAG(IS_ANDROID)
  histograms.ExpectTotalCount(
      "Memory.Total.PrivateMemoryFootprintExcludingWaivedRenderers", 0);
  histograms.ExpectTotalCount(
      "Memory.Total.RendererPrivateMemoryFootprintExcludingWaived", 0);
  histograms.ExpectTotalCount(
      "Memory.Total.PrivateMemoryFootprintVisibleOrHigherPriorityRenderers", 0);
  histograms.ExpectTotalCount(
      "Memory.Total.RendererPrivateMemoryFootprintVisibleOrHigherPriority", 0);
#endif

  auto nodes = CreateTestGraphNodes();

  // Simulate some metrics emission.
  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  // Check that the expected values have been emitted to histograms.
  histograms.ExpectBucketCount(
      "Memory.Experimental.Renderer2.Small.HibernatedCanvas.Size", 12 * kKiB,
      1);
  histograms.ExpectBucketCount(
      "Memory.Experimental.Renderer2.Small.HibernatedCanvas.Size", 22 * kKiB,
      1);
  histograms.ExpectUniqueSample("Memory.Renderer.PrivateMemoryFootprint",
                                kTestRendererPrivateMemoryFootprint, 2);
  histograms.ExpectUniqueSample("Memory.Renderer.SharedMemoryFootprint",
                                kTestRendererSharedMemoryFootprint, 2);
  histograms.ExpectUniqueSample("Memory.Renderer.ResidentSet",
                                kTestRendererResidentSet, 2);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  histograms.ExpectUniqueSample("Memory.Renderer.MappingsCount",
                                kTestMappingsCount, 2);
  histograms.ExpectUniqueSample("Memory.Renderer.Pss2", kTestPss, 2);
  histograms.ExpectUniqueSample("Memory.Renderer.SwapPss2", kTestSwapPss, 2);
#endif
  histograms.ExpectUniqueSample("Memory.Total.HibernatedCanvas.Size", 12 + 22,
                                1);
  histograms.ExpectUniqueSample("Memory.Total.PrivateMemoryFootprint",
                                2 * kTestRendererPrivateMemoryFootprint, 1);
  histograms.ExpectUniqueSample("Memory.Total.RendererPrivateMemoryFootprint",
                                2 * kTestRendererPrivateMemoryFootprint, 1);
  histograms.ExpectUniqueSample("Memory.Total.RendererMalloc",
                                2 * kTestRendererMalloc, 1);
  histograms.ExpectUniqueSample("Memory.Total.RendererBlinkGC",
                                2 * kTestRendererBlinkGC, 1);
  histograms.ExpectUniqueSample("Memory.Total.RendererBlinkGC.Fragmentation",
                                2 * kTestRendererBlinkGCFragmentation, 1);
  histograms.ExpectUniqueSample("Memory.Total.SharedMemoryFootprint",
                                2 * kTestRendererSharedMemoryFootprint, 1);
  histograms.ExpectUniqueSample("Memory.Total.ResidentSet",
                                2 * kTestRendererResidentSet, 1);

  histograms.ExpectUniqueSample("Memory.Total.TileMemory", 12 + 22, 1);

  histograms.ExpectUniqueSample(
      "Memory.NativeLibrary.MappedAndResidentMemoryFootprint3",
      kNativeLibraryResidentMemoryFootprint, 1);
  histograms.ExpectUniqueSample(
      "Memory.NativeLibrary.NotResidentOrderedCodeMemoryFootprint",
      kNativeLibraryNotResidentOrderedCodeFootprint, 1);
  histograms.ExpectUniqueSample(
      "Memory.NativeLibrary.ResidentNotOrderedCodeMemoryFootprint",
      kNativeLibraryResidentNotOrderedCodeFootprint, 1);
#if BUILDFLAG(IS_ANDROID)
  // Expect values of 0 as the Renderer is not in the list of active processes
  // and is therefore considered UNBOUND and will not emit these values.
  histograms.ExpectUniqueSample(
      "Memory.Total.PrivateMemoryFootprintExcludingWaivedRenderers", 0, 1);
  histograms.ExpectUniqueSample(
      "Memory.Total.RendererPrivateMemoryFootprintExcludingWaived", 0, 1);
  histograms.ExpectUniqueSample(
      "Memory.Total.PrivateMemoryFootprintVisibleOrHigherPriorityRenderers", 0,
      1);
  histograms.ExpectUniqueSample(
      "Memory.Total.RendererPrivateMemoryFootprintVisibleOrHigherPriority", 0,
      1);
#endif
}

TEST_F(ProcessMemoryMetricsEmitterTest,
       SingleMeasurement_RendererAndTotalHistogramsNotRecorded) {
  // Take a snapshot of the current state of the histograms.
  base::HistogramTester histograms;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  global_dump->aggregated_metrics =
      memory_instrumentation::mojom::AggregatedMetrics::New();
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  // No histograms should have been recorded yet.
  histograms.ExpectTotalCount("Memory.Renderer.PrivateMemoryFootprint", 0);

  auto nodes = CreateTestGraphNodes();

  // Simulate some metrics emission.
  auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
      kTestRendererPid201, test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  // Histograms should only be emitted when measuring all processes.
  histograms.ExpectTotalCount("Memory.Renderer.PrivateMemoryFootprint", 0);
}

TEST_F(ProcessMemoryMetricsEmitterTest, GpuHistogramsAreRecorded) {
  // Take a snapshot of the current state of the histograms.
  base::HistogramTester histograms;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  global_dump->aggregated_metrics =
      memory_instrumentation::mojom::AggregatedMetrics::New();
  MetricMap expected_metrics = GetExpectedGpuMetrics();
  PopulateGpuMetrics(global_dump, expected_metrics);

  auto nodes = CreateTestGraphNodes();

  // Simulate some metrics emission.
  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  // Check that the expected values have been emitted to histograms.
  histograms.ExpectBucketCount("Memory.Experimental.Gpu2.Vulkan2",
                               kGpuVulkanResourcesMB, 1);
  histograms.ExpectBucketCount(
      "Memory.Experimental.Gpu2.Vulkan2.AllocatedObjects",
      kGpuVulkanUsedResourcesMB, 1);
}

TEST_F(ProcessMemoryMetricsEmitterTest, MainFramePMFEmitted) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  auto nodes = CreateTestGraphNodes();

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::Memory_TabFootprint::kEntryName);
  ASSERT_EQ(entries.size(), 0u);

  auto emitter =
      base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::Memory_TabFootprint::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries.front().get();
  ASSERT_TRUE(test_ukm_recorder_.EntryHasMetric(
      entry, ukm::builders::Memory_TabFootprint::kMainFrameProcessPMFName));
}

TEST_F(ProcessMemoryMetricsEmitterTest,
       SingleMeasurement_MainFramePMFNotEmitted) {
  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  MetricMap expected_metrics = GetExpectedRendererMetrics();
  AddPageMetrics(expected_metrics);
  PopulateRendererMetrics(global_dump, expected_metrics, kTestRendererPid201);

  auto nodes = CreateTestGraphNodes();

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::Memory_TabFootprint::kEntryName);
  ASSERT_EQ(entries.size(), 0u);

  auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
      kTestRendererPid201, test_ukm_recorder_);
  emitter->ReceivedMemoryDump(
      emitter->GetProcessToPageInfoMap(graph()),
      memory_instrumentation::mojom::RequestOutcome::kSuccess,
      GlobalMemoryDump::MoveFrom(std::move(global_dump)));

  // Per-tab UKM's should only be emitted when measuring all processes.
  entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::Memory_TabFootprint::kEntryName);
  ASSERT_EQ(entries.size(), 0u);
}
