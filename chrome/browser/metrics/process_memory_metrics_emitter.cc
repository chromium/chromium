// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/tab_footprint_aggregator.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/audio_service_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#endif

using base::trace_event::MemoryAllocatorDump;
using memory_instrumentation::GetPrivateFootprintHistogramName;
using memory_instrumentation::GlobalMemoryDump;
using memory_instrumentation::HistogramProcessType;
using memory_instrumentation::HistogramProcessTypeToString;
using memory_instrumentation::kMemoryHistogramPrefix;
using ukm::builders::Memory_Experimental;

namespace {

const char kEffectiveSize[] = "effective_size";
const char kSize[] = "size";
const char kAllocatedObjectsSize[] = "allocated_objects_size";
const bool kLargeMetric = true;

enum class EmitTo {
  kCountsInUkmOnly,
  kSizeInUkmAndUma,
  kSizeInUmaOnly,
  kIgnored
};

struct Metric {
  // The root dump name that represents the required metric.
  const char* const dump_name;
  // The name of the metric to be recorded in UMA.
  const char* const uma_name;
  // Should the UMA use large memory range (1MB - 64GB) or small memory range
  // (10KB - 500MB). Only relevant if the |metric| is a size metric.
  const bool is_large_metric;
  // The type of metric that is measured, usually size in bytes or object count.
  const char* const metric;
  // Indicates where to emit the metric.
  const EmitTo target;
  // The setter method for the metric in UKM recorder.
  Memory_Experimental& (Memory_Experimental::*ukm_setter)(int64_t);
};

const Metric kAllocatorDumpNamesForMetrics[] = {
    {"blink_gc", "BlinkGC", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetBlinkGC},
    {"blink_gc", "BlinkGC.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetBlinkGC_AllocatedObjects},
    {"blink_objects/Document", "NumberOfDocuments", !kLargeMetric,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfDocuments},
    {"blink_objects/AdSubframe", "NumberOfAdSubframes", !kLargeMetric,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfAdSubframes},
    {"blink_objects/DetachedScriptState", "NumberOfDetachedScriptStates",
     !kLargeMetric, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfDetachedScriptStates},
    {"blink_objects/Frame", "NumberOfFrames", !kLargeMetric,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfFrames},
    {"blink_objects/LayoutObject", "NumberOfLayoutObjects", !kLargeMetric,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfLayoutObjects},
    {"blink_objects/Node", "NumberOfNodes", !kLargeMetric,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfNodes},
    {"components/download", "DownloadService", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetDownloadService},
    {"discardable", "Discardable", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetDiscardable},
    {"extensions/value_store", "Extensions.ValueStore", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetExtensions_ValueStore},
    {"font_caches", "FontCaches", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetFontCaches},
    {"gpu/discardable_cache", "ServiceDiscardableManager", !kLargeMetric, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/discardable_cache", "ServiceDiscardableManager.AvgImageSize",
     !kLargeMetric, "average_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/gl", "CommandBuffer", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetCommandBuffer},
    {"gpu/gr_shader_cache", "Gpu.GrShaderCache", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/shared_images", "gpu::SharedImageStub", !kLargeMetric, kEffectiveSize,
     EmitTo::kIgnored, nullptr},
    {"gpu/transfer_cache", "ServiceTransferCache", !kLargeMetric, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/transfer_cache", "ServiceTransferCache.AvgImageSize", !kLargeMetric,
     "average_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"history", "History", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetHistory},
    {"java_heap", "JavaHeap", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetJavaHeap},
    {"leveldatabase", "LevelDatabase", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetLevelDatabase},
    {"malloc", "Malloc", kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetMalloc},
    {"malloc/allocated_objects", "Malloc.AllocatedObjects", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetMalloc_AllocatedObjects},
    {"mojo", "NumberOfMojoHandles", !kLargeMetric,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfMojoHandles},
    {"net", "Net", !kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetNet},
    {"net/url_request_context", "Net.UrlRequestContext", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetNet_UrlRequestContext},
    {"omnibox", "OmniboxSuggestions", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetOmniboxSuggestions},
    {"partition_alloc", "PartitionAlloc", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetPartitionAlloc},
    {"partition_alloc/allocated_objects", "PartitionAlloc.AllocatedObjects",
     kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_AllocatedObjects},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.Partitions.ArrayBuffer", kLargeMetric, kSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_ArrayBuffer},
    {"partition_alloc/partitions/buffer", "PartitionAlloc.Partitions.Buffer",
     kLargeMetric, kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_Buffer},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Partitions.FastMalloc", kLargeMetric, kSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_FastMalloc},
    {"partition_alloc/partitions/layout", "PartitionAlloc.Partitions.Layout",
     kLargeMetric, kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_Layout},
    {"site_storage", "SiteStorage", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSiteStorage},
    {"site_storage/blob_storage", "SiteStorage.BlobStorage", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_BlobStorage},
    {"site_storage/index_db", "SiteStorage.IndexDB", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_IndexDB},
    {"site_storage/localstorage", "SiteStorage.LocalStorage", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_LocalStorage},
    {"site_storage/session_storage", "SiteStorage.SessionStorage",
     !kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_SessionStorage},
    {"skia", "Skia", kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSkia},
    {"skia/gpu_resources", "SharedContextState", kLargeMetric, kEffectiveSize,
     EmitTo::kIgnored, nullptr},
    {"skia/gpu_resources", "VizProcessContextProvider", kLargeMetric,
     kEffectiveSize, EmitTo::kIgnored, nullptr},
    {"skia/sk_glyph_cache", "Skia.SkGlyphCache", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSkia_SkGlyphCache},
    {"skia/sk_resource_cache", "Skia.SkResourceCache", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSkia_SkResourceCache},
    {"sqlite", "Sqlite", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSqlite},
    {"sync", "Sync", kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSync},
    {"tab_restore", "TabRestore", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetTabRestore},
    {"ui", "UI", !kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetUI},
    {"v8", "V8", kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8},
    {"v8", "V8.AllocatedObjects", kLargeMetric, kAllocatedObjectsSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_AllocatedObjects},
    {"v8/main", "V8.Main", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Main},
    {"v8/main", "V8.Main.AllocatedObjects", kLargeMetric, kAllocatedObjectsSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_AllocatedObjects},
    {"v8/main/heap", "V8.Main.Heap", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Main_Heap},
    {"v8/main/heap", "V8.Main.Heap.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_AllocatedObjects},
    {"v8/main/heap/code_large_object_space",
     "V8.Main.Heap.CodeLargeObjectSpace", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_CodeLargeObjectSpace},
    {"v8/main/heap/code_large_object_space",
     "V8.Main.Heap.CodeLargeObjectSpace.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::
         SetV8_Main_Heap_CodeLargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/code_space", "V8.Main.Heap.CodeSpace", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_CodeSpace},
    {"v8/main/heap/code_space", "V8.Main.Heap.CodeSpace.AllocatedObjects",
     kLargeMetric, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_CodeSpace_AllocatedObjects},
    {"v8/main/heap/large_object_space", "V8.Main.Heap.LargeObjectSpace",
     kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_LargeObjectSpace},
    {"v8/main/heap/large_object_space",
     "V8.Main.Heap.LargeObjectSpace.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_LargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/map_space", "V8.Main.Heap.MapSpace", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_MapSpace},
    {"v8/main/heap/map_space", "V8.Main.Heap.MapSpace.AllocatedObjects",
     kLargeMetric, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_MapSpace_AllocatedObjects},
    {"v8/main/heap/new_large_object_space", "V8.Main.Heap.NewLargeObjectSpace",
     kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_NewLargeObjectSpace},
    {"v8/main/heap/new_large_object_space",
     "V8.Main.Heap.NewLargeObjectSpace.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::
         SetV8_Main_Heap_NewLargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/new_space", "V8.Main.Heap.NewSpace", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_NewSpace},
    {"v8/main/heap/new_space", "V8.Main.Heap.NewSpace.AllocatedObjects",
     kLargeMetric, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_NewSpace_AllocatedObjects},
    {"v8/main/heap/old_space", "V8.Main.Heap.OldSpace", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_OldSpace},
    {"v8/main/heap/old_space", "V8.Main.Heap.OldSpace.AllocatedObjects",
     kLargeMetric, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_OldSpace_AllocatedObjects},
    {"v8/main/heap/read_only_space", "V8.Main.Heap.ReadOnlySpace", kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_ReadOnlySpace},
    {"v8/main/heap/read_only_space",
     "V8.Main.Heap.ReadOnlySpace.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_ReadOnlySpace_AllocatedObjects},
    {"v8/main/malloc", "V8.Main.Malloc", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Main_Malloc},
    {"v8/workers", "V8.Workers", kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Workers},
    {"v8/workers", "V8.Workers.AllocatedObjects", kLargeMetric,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Workers_AllocatedObjects},
    {"web_cache", "WebCache", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetWebCache},
    {"web_cache/Image_resources", "WebCache.ImageResources", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_ImageResources},
    {"web_cache/CSS stylesheet_resources", "WebCache.CSSStylesheetResources",
     !kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_CSSStylesheetResources},
    {"web_cache/Script_resources", "WebCache.ScriptResources", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_ScriptResources},
    {"web_cache/XSL stylesheet_resources", "WebCache.XSLStylesheetResources",
     !kLargeMetric, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_XSLStylesheetResources},
    {"web_cache/Font_resources", "WebCache.FontResources", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_FontResources},
    {"web_cache/Code_cache", "WebCache.V8CodeCache", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_V8CodeCache},
    {"web_cache/Encoded_size_duplicated_in_data_urls",
     "WebCache.EncodedSizeDuplicatedInDataUrls", !kLargeMetric, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_EncodedSizeDuplicatedInDataUrls},
    {"web_cache/Other_resources", "WebCache.OtherResources", !kLargeMetric,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_OtherResources},
};

#define EXPERIMENTAL_UMA_PREFIX "Memory.Experimental."
#define VERSION_SUFFIX_NORMAL "2."
#define VERSION_SUFFIX_SMALL "2.Small."

// Used to measure KB-granularity memory stats. Range is from 1KB to 500,000KB
// (500MB).
#define MEMORY_METRICS_HISTOGRAM_KB(name, value) \
  base::UmaHistogramCustomCounts(name, value, 10, 500000, 100)

void EmitProcessUkm(const Metric& item,
                    uint64_t value,
                    Memory_Experimental* builder) {
  DCHECK(item.ukm_setter) << "UKM metrics must provide a setter";
  (builder->*(item.ukm_setter))(value);
}

void EmitProcessUma(HistogramProcessType process_type,
                    const Metric& item,
                    uint64_t value) {
  std::string uma_name;

  // Always use "Gpu" in process name for command buffers to be
  // consistent even in single process mode.
  if (base::StringPiece(item.uma_name) == "CommandBuffer") {
    uma_name =
        EXPERIMENTAL_UMA_PREFIX "Gpu" VERSION_SUFFIX_NORMAL "CommandBuffer";
    DCHECK(item.is_large_metric);
  } else {
    const char* version_suffix =
        item.is_large_metric ? VERSION_SUFFIX_NORMAL : VERSION_SUFFIX_SMALL;
    uma_name = std::string(EXPERIMENTAL_UMA_PREFIX) +
               HistogramProcessTypeToString(process_type) + version_suffix +
               item.uma_name;
  }

  if (item.is_large_metric) {
    MEMORY_METRICS_HISTOGRAM_MB(uma_name, value / 1024 / 1024);
  } else {
    MEMORY_METRICS_HISTOGRAM_KB(uma_name, value / 1024);
  }
}

void EmitProcessUmaAndUkm(const GlobalMemoryDump::ProcessDump& pmd,
                          HistogramProcessType process_type,
                          const base::Optional<base::TimeDelta>& uptime,
                          bool record_uma,
                          Memory_Experimental* builder) {
  for (const auto& item : kAllocatorDumpNamesForMetrics) {
    base::Optional<uint64_t> value = pmd.GetMetric(item.dump_name, item.metric);
    if (!value)
      continue;

    switch (item.target) {
      case EmitTo::kCountsInUkmOnly:
        EmitProcessUkm(item, value.value(), builder);
        break;
      case EmitTo::kSizeInUmaOnly:
        if (record_uma)
          EmitProcessUma(process_type, item, value.value());
        break;
      case EmitTo::kSizeInUkmAndUma:
        // For each 'size' metric, emit size as MB.
        EmitProcessUkm(item, value.value() / 1024 / 1024, builder);
        if (record_uma)
          EmitProcessUma(process_type, item, value.value());
        break;
      case EmitTo::kIgnored:
        break;
      default:
        NOTREACHED();
    }
  }

#if !defined(OS_MACOSX)
  // Resident set is not populated on Mac.
  builder->SetResident(pmd.os_dump().resident_set_kb / 1024);
#endif

  builder->SetPrivateMemoryFootprint(pmd.os_dump().private_footprint_kb / 1024);
  builder->SetSharedMemoryFootprint(pmd.os_dump().shared_footprint_kb / 1024);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  builder->SetPrivateSwapFootprint(pmd.os_dump().private_footprint_swap_kb /
                                   1024);
#endif
  if (uptime)
    builder->SetUptime(uptime.value().InSeconds());
  if (!record_uma)
    return;

  const char* process_name = HistogramProcessTypeToString(process_type);
#if defined(OS_MACOSX)
  // Resident set is not populated on Mac.
  DCHECK_EQ(pmd.os_dump().resident_set_kb, 0U);
#else
  MEMORY_METRICS_HISTOGRAM_MB(
      std::string(kMemoryHistogramPrefix) + process_name + ".ResidentSet",
      pmd.os_dump().resident_set_kb / 1024);
#endif
  MEMORY_METRICS_HISTOGRAM_MB(GetPrivateFootprintHistogramName(process_type),
                              pmd.os_dump().private_footprint_kb / 1024);
  MEMORY_METRICS_HISTOGRAM_MB(std::string(kMemoryHistogramPrefix) +
                                  process_name + ".SharedMemoryFootprint",
                              pmd.os_dump().shared_footprint_kb / 1024);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  MEMORY_METRICS_HISTOGRAM_MB(std::string(kMemoryHistogramPrefix) +
                                  process_name + ".PrivateSwapFootprint",
                              pmd.os_dump().private_footprint_swap_kb / 1024);
#endif
}

void EmitSummedGpuMemory(const GlobalMemoryDump::ProcessDump& pmd,
                         Memory_Experimental* builder,
                         bool record_uma) {
  // Combine several categories together to sum up Chrome-reported gpu memory.
  static const char* gpu_categories[] = {
      "gpu/gl",
      "gpu/shared_images",
      "skia/gpu_resources",
  };
  Metric synthetic_metric = {nullptr,
                             "GpuMemory",
                             kLargeMetric,
                             kEffectiveSize,
                             EmitTo::kSizeInUkmAndUma,
                             &Memory_Experimental::SetGpuMemory};

  uint64_t total = 0;
  for (size_t i = 0; i < base::size(gpu_categories); ++i) {
    total +=
        pmd.GetMetric(gpu_categories[i], synthetic_metric.metric).value_or(0);
  }

  // We log this metric for both the browser and GPU process, and only one will
  // have entries for |gpu_categories|, so only log if |total| > 0. There should
  // be almost no meaningful cases where |total| is actually zero.
  if (total == 0)
    return;

  // Always use kGpu as the process name for this even for the in process
  // command buffer case.
  EmitProcessUkm(synthetic_metric, total, builder);
  if (record_uma)
    EmitProcessUma(HistogramProcessType::kGpu, synthetic_metric, total);
}

void EmitBrowserMemoryMetrics(const GlobalMemoryDump::ProcessDump& pmd,
                              ukm::SourceId ukm_source_id,
                              ukm::UkmRecorder* ukm_recorder,
                              const base::Optional<base::TimeDelta>& uptime,
                              bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::BROWSER));
  EmitProcessUmaAndUkm(pmd, HistogramProcessType::kBrowser, uptime, record_uma,
                       &builder);
  EmitSummedGpuMemory(pmd, &builder, record_uma);

  builder.Record(ukm_recorder);
}

void EmitRendererMemoryMetrics(
    const GlobalMemoryDump::ProcessDump& pmd,
    const ProcessMemoryMetricsEmitter::PageInfo* page_info,
    ukm::UkmRecorder* ukm_recorder,
    int number_of_extensions,
    const base::Optional<base::TimeDelta>& uptime,
    bool record_uma) {
  ukm::SourceId ukm_source_id =
      page_info ? page_info->ukm_source_id : ukm::UkmRecorder::GetNewSourceID();
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::RENDERER));
  builder.SetNumberOfExtensions(number_of_extensions);

  const HistogramProcessType process_type =
      (number_of_extensions == 0) ? HistogramProcessType::kRenderer
                                  : HistogramProcessType::kExtension;
  EmitProcessUmaAndUkm(pmd, process_type, uptime, record_uma, &builder);

  if (page_info) {
    builder.SetIsVisible(page_info->is_visible);
    builder.SetTimeSinceLastVisibilityChange(
        page_info->time_since_last_visibility_change.InSeconds());
    builder.SetTimeSinceLastNavigation(
        page_info->time_since_last_navigation.InSeconds());
  }

  builder.Record(ukm_recorder);
}

void EmitGpuMemoryMetrics(const GlobalMemoryDump::ProcessDump& pmd,
                          ukm::SourceId ukm_source_id,
                          ukm::UkmRecorder* ukm_recorder,
                          const base::Optional<base::TimeDelta>& uptime,
                          bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(
      static_cast<int64_t>(memory_instrumentation::mojom::ProcessType::GPU));
  EmitProcessUmaAndUkm(pmd, HistogramProcessType::kGpu, uptime, record_uma,
                       &builder);
  EmitSummedGpuMemory(pmd, &builder, record_uma);
  builder.Record(ukm_recorder);
}

void EmitUtilityMemoryMetrics(const GlobalMemoryDump::ProcessDump& pmd,
                              ukm::SourceId ukm_source_id,
                              ukm::UkmRecorder* ukm_recorder,
                              const base::Optional<base::TimeDelta>& uptime,
                              bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::UTILITY));
  EmitProcessUmaAndUkm(pmd, HistogramProcessType::kUtility, uptime, record_uma,
                       &builder);

  builder.Record(ukm_recorder);
}

void EmitAudioServiceMemoryMetrics(
    const GlobalMemoryDump::ProcessDump& pmd,
    ukm::SourceId ukm_source_id,
    ukm::UkmRecorder* ukm_recorder,
    const base::Optional<base::TimeDelta>& uptime,
    bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::UTILITY));
  EmitProcessUmaAndUkm(pmd, HistogramProcessType::kAudioService, uptime,
                       record_uma, &builder);

  builder.Record(ukm_recorder);
}

void EmitNetworkServiceMemoryMetrics(
    const GlobalMemoryDump::ProcessDump& pmd,
    ukm::SourceId ukm_source_id,
    ukm::UkmRecorder* ukm_recorder,
    const base::Optional<base::TimeDelta>& uptime,
    bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::UTILITY));
  EmitProcessUmaAndUkm(pmd, HistogramProcessType::kNetworkService, uptime,
                       record_uma, &builder);

  builder.Record(ukm_recorder);
}

}  // namespace

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter()
    : pid_scope_(base::kNullProcessId) {}

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter(
    base::ProcessId pid_scope)
    : pid_scope_(pid_scope) {}

void ProcessMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MarkServiceRequestsInProgress();

  // The callback keeps this object alive until the callback is invoked.
  auto callback =
      base::Bind(&ProcessMemoryMetricsEmitter::ReceivedMemoryDump, this);
  std::vector<std::string> mad_list;
  for (const auto& metric : kAllocatorDumpNamesForMetrics)
    mad_list.push_back(metric.dump_name);
  if (pid_scope_ != base::kNullProcessId) {
    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->RequestGlobalDumpForPid(pid_scope_, mad_list, callback);
  } else {
    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->RequestGlobalDump(mad_list, callback);
  }

  // Use a lambda adapter to post the results back to this sequence.
  GetProcessToPageInfoMapCallback callback2 = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         scoped_refptr<ProcessMemoryMetricsEmitter> pmme,
         std::vector<ProcessInfo> process_infos) -> void {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ProcessMemoryMetricsEmitter::ReceivedProcessInfos,
                           pmme, std::move(process_infos)));
      },
      base::SequencedTaskRunnerHandle::Get(),
      scoped_refptr<ProcessMemoryMetricsEmitter>(this));

  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&ProcessMemoryMetricsEmitter::GetProcessToPageInfoMap,
                     std::move(callback2)));
}

void ProcessMemoryMetricsEmitter::MarkServiceRequestsInProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memory_dump_in_progress_ = true;
  get_process_urls_in_progress_ = true;
}

ProcessMemoryMetricsEmitter::~ProcessMemoryMetricsEmitter() {}

void ProcessMemoryMetricsEmitter::ReceivedMemoryDump(
    bool success,
    std::unique_ptr<GlobalMemoryDump> dump) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memory_dump_in_progress_ = false;
  if (!success)
    return;
  global_dump_ = std::move(dump);
  CollateResults();
}

void ProcessMemoryMetricsEmitter::ReceivedProcessInfos(
    std::vector<ProcessInfo> process_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  get_process_urls_in_progress_ = false;
  process_infos_.clear();
  process_infos_.reserve(process_infos.size());

  // If there are duplicate pids, keep the latest ProcessInfoPtr.
  for (ProcessInfo& process_info : process_infos) {
    base::ProcessId pid = process_info.pid;
    process_infos_[pid] = std::move(process_info);
  }
  CollateResults();
}

ukm::UkmRecorder* ProcessMemoryMetricsEmitter::GetUkmRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return ukm::UkmRecorder::Get();
}

int ProcessMemoryMetricsEmitter::GetNumberOfExtensions(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int number_of_extensions = 0;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Retrieve the renderer process host for the given pid.
  int rph_id = -1;
  bool found = false;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (!iter.GetCurrentValue()->GetProcess().IsValid())
      continue;

    if (iter.GetCurrentValue()->GetProcess().Pid() == pid) {
      found = true;
      rph_id = iter.GetCurrentValue()->GetID();
      break;
    }
  }
  if (!found)
    return 0;

  // Count the number of extensions associated with that renderer process host
  // in all profiles.
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    std::set<std::string> extension_ids =
        process_map->GetExtensionsInProcess(rph_id);
    for (const std::string& extension_id : extension_ids) {
      // Only count non hosted apps extensions.
      const extensions::Extension* extension =
          registry->enabled_extensions().GetByID(extension_id);
      if (extension && !extension->is_hosted_app())
        number_of_extensions++;
    }
  }
#endif
  return number_of_extensions;
}

base::Optional<base::TimeDelta> ProcessMemoryMetricsEmitter::GetProcessUptime(
    const base::Time& now,
    base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto process_info = process_infos_.find(pid);
  if (process_info != process_infos_.end()) {
    if (!process_info->second.launch_time.is_null())
      return now - process_info->second.launch_time;
  }
  return base::Optional<base::TimeDelta>();
}

void ProcessMemoryMetricsEmitter::CollateResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (memory_dump_in_progress_ || get_process_urls_in_progress_)
    return;
  if (!global_dump_)
    return;

  uint32_t private_footprint_total_kb = 0;
  uint32_t renderer_private_footprint_total_kb = 0;
  uint32_t shared_footprint_total_kb = 0;
  uint32_t resident_set_total_kb = 0;
  bool emit_metrics_for_all_processes = pid_scope_ == base::kNullProcessId;

  TabFootprintAggregator per_tab_metrics;

  base::Time now = base::Time::Now();
  for (const auto& pmd : global_dump_->process_dumps()) {
    uint32_t process_pmf_kb = pmd.os_dump().private_footprint_kb;
    private_footprint_total_kb += process_pmf_kb;
    shared_footprint_total_kb += pmd.os_dump().shared_footprint_kb;
    resident_set_total_kb += pmd.os_dump().resident_set_kb;

    if (!emit_metrics_for_all_processes && pid_scope_ != pmd.pid())
      continue;

    switch (pmd.process_type()) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        EmitBrowserMemoryMetrics(
            pmd, ukm::UkmRecorder::GetNewSourceID(), GetUkmRecorder(),
            GetProcessUptime(now, pmd.pid()), emit_metrics_for_all_processes);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        renderer_private_footprint_total_kb += process_pmf_kb;
        const PageInfo* single_page_info = nullptr;
        auto iter = process_infos_.find(pmd.pid());
        if (iter != process_infos_.end()) {
          const ProcessInfo& process_info = iter->second;

          if (emit_metrics_for_all_processes) {
            // Renderer metrics-by-tab only make sense if we're visiting all
            // render processes.
            for (const PageInfo& page_info : process_info.page_infos) {
              if (page_info.hosts_main_frame) {
                per_tab_metrics.AssociateMainFrame(page_info.ukm_source_id,
                                                   pmd.pid(), page_info.tab_id,
                                                   process_pmf_kb);
              } else {
                per_tab_metrics.AssociateSubFrame(page_info.ukm_source_id,
                                                  pmd.pid(), page_info.tab_id,
                                                  process_pmf_kb);
              }
            }
          }

          // If there is more than one frame being hosted in a renderer, don't
          // emit any per-renderer URLs. This is not ideal, but UKM does not
          // support multiple-URLs per entry, and we must have one entry per
          // process.
          if (process_info.page_infos.size() == 1) {
            single_page_info = &process_info.page_infos[0];
          }
        }

        int number_of_extensions = GetNumberOfExtensions(pmd.pid());
        EmitRendererMemoryMetrics(
            pmd, single_page_info, GetUkmRecorder(), number_of_extensions,
            GetProcessUptime(now, pmd.pid()), emit_metrics_for_all_processes);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::GPU: {
        EmitGpuMemoryMetrics(pmd, ukm::UkmRecorder::GetNewSourceID(),
                             GetUkmRecorder(), GetProcessUptime(now, pmd.pid()),
                             emit_metrics_for_all_processes);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::UTILITY: {
        if (pmd.pid() == content::GetProcessIdForAudioService()) {
          EmitAudioServiceMemoryMetrics(
              pmd, ukm::UkmRecorder::GetNewSourceID(), GetUkmRecorder(),
              GetProcessUptime(now, pmd.pid()), emit_metrics_for_all_processes);
        } else if (pmd.service_name() ==
                   network::mojom::NetworkService::Name_) {
          EmitNetworkServiceMemoryMetrics(
              pmd, ukm::UkmRecorder::GetNewSourceID(), GetUkmRecorder(),
              GetProcessUptime(now, pmd.pid()), emit_metrics_for_all_processes);
        } else {
          EmitUtilityMemoryMetrics(
              pmd, ukm::UkmRecorder::GetNewSourceID(), GetUkmRecorder(),
              GetProcessUptime(now, pmd.pid()), emit_metrics_for_all_processes);
        }
        break;
      }
      case memory_instrumentation::mojom::ProcessType::PLUGIN:
        FALLTHROUGH;
      case memory_instrumentation::mojom::ProcessType::ARC:
        FALLTHROUGH;
      case memory_instrumentation::mojom::ProcessType::OTHER:
        break;
    }
  }

  if (emit_metrics_for_all_processes) {
    const auto& metrics = global_dump_->aggregated_metrics();
    int32_t native_resident_kb = metrics.native_library_resident_kb();
    int32_t native_library_resident_not_ordered_kb =
        metrics.native_library_resident_not_ordered_kb();
    int32_t native_library_not_resident_ordered_kb =
        metrics.native_library_not_resident_ordered_kb();

    // |native_resident_kb| is only calculated for android devices that support
    // code ordering.
    if (native_resident_kb != metrics.kInvalid) {
      // Size of the native library on android is ~40MB.
      // More precision is needed in the middle buckets, hence the range.
      base::UmaHistogramCustomCounts(
          "Memory.NativeLibrary.MappedAndResidentMemoryFootprint2",
          native_resident_kb, 1000, 100000, 100);
      if (native_library_not_resident_ordered_kb != metrics.kInvalid) {
        base::UmaHistogramCustomCounts(
            "Memory.NativeLibrary.NotResidentOrderedCodeMemoryFootprint",
            native_library_not_resident_ordered_kb, 1000, 100000, 100);
      }
      if (native_library_resident_not_ordered_kb != metrics.kInvalid) {
        base::UmaHistogramCustomCounts(
            "Memory.NativeLibrary.ResidentNotOrderedCodeMemoryFootprint",
            native_library_resident_not_ordered_kb, 1000, 100000, 100);
      }
    }

    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Experimental.Total2.PrivateMemoryFootprint",
        private_footprint_total_kb / 1024);
#if defined(OS_MACOSX)
    // Resident set is not populated on Mac.
    DCHECK_EQ(resident_set_total_kb, 0U);
#else
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.ResidentSet",
                                  resident_set_total_kb / 1024);

#endif
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.PrivateMemoryFootprint",
                                  private_footprint_total_kb / 1024);
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.RendererPrivateMemoryFootprint",
                                  renderer_private_footprint_total_kb / 1024);
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.SharedMemoryFootprint",
                                  shared_footprint_total_kb / 1024);

    Memory_Experimental(ukm::UkmRecorder::GetNewSourceID())
        .SetTotal2_PrivateMemoryFootprint(private_footprint_total_kb / 1024)
        .SetTotal2_SharedMemoryFootprint(shared_footprint_total_kb / 1024)
        .Record(GetUkmRecorder());

    // Renderer metrics-by-tab only make sense if we're visiting all render
    // processes.
    per_tab_metrics.RecordPmfs(GetUkmRecorder());
  }
}

namespace {

// Returns true iff the given |process| is responsible for hosting the
// main-frame of the given |page|.
bool HostsMainFrame(const performance_manager::ProcessNode* process,
                    const performance_manager::PageNode* page) {
  const performance_manager::FrameNode* main_frame = page->GetMainFrameNode();
  if (main_frame == nullptr) {
    // |process| can't host a frame that doesn't exist.
    return false;
  }

  return main_frame->GetProcessNode() == process;
}

}  // namespace

void ProcessMemoryMetricsEmitter::GetProcessToPageInfoMap(
    GetProcessToPageInfoMapCallback callback,
    performance_manager::Graph* graph) {
  std::vector<ProcessInfo> process_infos;
  std::vector<const performance_manager::ProcessNode*> process_nodes =
      graph->GetAllProcessNodes();
  for (auto* process_node : process_nodes) {
    if (process_node->GetProcessId() == base::kNullProcessId)
      continue;

    ProcessInfo process_info;
    process_info.pid = process_node->GetProcessId();
    process_info.launch_time = process_node->GetLaunchTime();

    base::flat_set<const performance_manager::PageNode*> page_nodes =
        performance_manager::GraphOperations::GetAssociatedPageNodes(
            process_node);
    for (const performance_manager::PageNode* page_node : page_nodes) {
      if (page_node->GetUkmSourceID() == ukm::kInvalidSourceId)
        continue;

      PageInfo page_info;
      page_info.ukm_source_id = page_node->GetUkmSourceID();
      page_info.tab_id =
          performance_manager::Node::GetSerializationId(page_node);
      page_info.hosts_main_frame = HostsMainFrame(process_node, page_node);
      page_info.is_visible = page_node->IsVisible();
      page_info.time_since_last_visibility_change =
          page_node->GetTimeSinceLastVisibilityChange();
      page_info.time_since_last_navigation =
          page_node->GetTimeSinceLastNavigation();
      process_info.page_infos.push_back(std::move(page_info));
    }
    process_infos.push_back(std::move(process_info));
  }
  std::move(callback).Run(std::move(process_infos));
}

ProcessMemoryMetricsEmitter::ProcessInfo::ProcessInfo() = default;

ProcessMemoryMetricsEmitter::ProcessInfo::ProcessInfo(ProcessInfo&& other) =
    default;

ProcessMemoryMetricsEmitter::ProcessInfo::~ProcessInfo() = default;

ProcessMemoryMetricsEmitter::ProcessInfo&
ProcessMemoryMetricsEmitter::ProcessInfo::operator=(const ProcessInfo& other) =
    default;
