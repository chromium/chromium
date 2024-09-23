// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/tab_footprint_aggregator.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_data_validation.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "content/public/browser/audio_service_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "partition_alloc/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/child_process_binding_types.h"
#include "base/android/meminfo_dump_provider.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#endif

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
#if BUILDFLAG(IS_CHROMEOS)
const char kNonExoSize[] = "non_exo_size";
#endif

constexpr int kKiB = 1024;
constexpr int kMiB = 1024 * 1024;

struct MetricRange {
  const int min;
  const int max;
};

const MetricRange ImageSizeMetricRange = {1, 500 * kMiB /*500 MiB*/};

// Prefer predefined ranges kLarge, kSmall and kTiny over custom ranges.
enum class MetricSize {
  kPercentage,  // percentages, 0% - 100%
  kLarge,       // 1MiB - 64,000MiB
  kSmall,       // 10 - 500,000KiB
  kTiny,        // 1 - 500,000B
  kCustom,      // custom range, in bytes
};

enum class EmitTo {
  kCountsInUkmOnly,
  kCountsInUkmAndSizeInUma,
  kSizeInUkmAndUma,
  kSizeInUmaOnly,
  kIgnored
};

struct Metric {
  // The root dump name that represents the required metric.
  const char* const dump_name;
  // The name of the metric to be recorded in UMA.
  const char* const uma_name;
  // Indicates the size range of the metric. Only relevant if the |metric| is a
  // size metric.
  const MetricSize metric_size;
  // The type of metric that is measured, usually size in bytes or object count.
  const char* const metric;
  // Indicates where to emit the metric.
  const EmitTo target;
  // The setter method for the metric in UKM recorder.
  Memory_Experimental& (Memory_Experimental::*ukm_setter)(int64_t);
  // Size range for the kCustom |metric_size|. Represents the min and max of the
  // size range, in bytes.
  const MetricRange range;
};

const Metric kAllocatorDumpNamesForMetrics[] = {
    {"accessibility/ax_platform_node",
     "AXPlatformNodeCount",
     MetricSize::kCustom,
     MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly,
     /*ukm_setter=*/nullptr,
     {1, 1000000}},
    {"blink_gc", "BlinkGC", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetBlinkGC},
    {"blink_gc", "BlinkGC.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetBlinkGC_AllocatedObjects},
    {"blink_gc", "BlinkGC.Fragmentation", MetricSize::kPercentage,
     "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_gc/main", "BlinkGC.Main.Heap", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_gc/main", "BlinkGC.Main.Heap.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_gc/main", "BlinkGC.Main.Heap.Fragmentation",
     MetricSize::kPercentage, "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/Document", "NumberOfDocuments", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfDocuments},
    {"blink_objects/ArrayBufferContents", "NumberOfArrayBufferContents",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfArrayBufferContents},
    {"blink_objects/AdSubframe", "NumberOfAdSubframes", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfAdSubframes},
    {"blink_objects/DetachedScriptState", "NumberOfDetachedScriptStates",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfDetachedScriptStates},
    {"blink_objects/Frame", "NumberOfFrames", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfFrames},
    {"blink_objects/LayoutObject", "NumberOfLayoutObjects", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfLayoutObjects},
    {"blink_objects/Node", "NumberOfNodes", MetricSize::kSmall,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmAndSizeInUma,
     &Memory_Experimental::SetNumberOfNodes},
    {"blink_objects/AudioHandler", "NumberOfAudioHandler", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/JSEventListener", "NumberOfJSEventListener",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/MediaKeySession", "NumberOfMediaKeySession",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/MediaKeys", "NumberOfMediaKeys", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/Resource", "NumberOfResources", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/ContextLifecycleStateObserver",
     "NumberOfContextLifecycleStateObserver", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/V8PerContextData", "NumberOfV8PerContextData",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/WorkerGlobalScope", "NumberOfWorkerGlobalScope",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/UACSSResource", "NumberOfUACSSResource", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/RTCPeerConnection", "NumberOfRTCPeerConnection",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"blink_objects/ResourceFetcher", "NumberOfResourceFetcher",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"canvas/hibernated", "HibernatedCanvas.Size", MetricSize::kSmall, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"canvas/hibernated", "HibernatedCanvas.OriginalSize", MetricSize::kSmall,
     "original_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"cc/tile_memory", "TileMemory", MetricSize::kSmall, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"components/download", "DownloadService", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetDownloadService},
    {"discardable", "Discardable", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetDiscardable},
    {"discardable", "Discardable.FreelistSize", MetricSize::kSmall,
     "freelist_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"discardable", "Discardable.FreelistSize.Dirty", MetricSize::kSmall,
     "freelist_size_dirty", EmitTo::kSizeInUmaOnly, nullptr},
    {"discardable", "Discardable.ResidentSize", MetricSize::kSmall,
     "resident_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"discardable", "Discardable.VirtualSize", MetricSize::kSmall,
     "virtual_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"extensions/functions", "ExtensionFunctions", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUmaOnly, nullptr},
    {"extensions/value_store", "Extensions.ValueStore", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetExtensions_ValueStore},
    {"font_caches", "FontCaches", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetFontCaches},
    {"gpu/dawn", "DawnSharedContext", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/discardable_cache", "ServiceDiscardableManager", MetricSize::kCustom,
     kSize, EmitTo::kSizeInUmaOnly, nullptr, ImageSizeMetricRange},
    {"gpu/discardable_cache", "ServiceDiscardableManager.AvgImageSize",
     MetricSize::kCustom, "average_size", EmitTo::kSizeInUmaOnly, nullptr,
     ImageSizeMetricRange},
    {"gpu/gl", "CommandBuffer", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetCommandBuffer},
    {"gpu/gr_shader_cache", "Gpu.GrShaderCache", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/mapped_memory", "GpuMappedMemory", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    // Not effective size, to account for the total footprint, a large fraction
    // of it being claimed by renderers.
    {"gpu/shared_images", "SharedImages", MetricSize::kLarge, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/shared_images", "SharedImages.Purgeable", MetricSize::kLarge,
     "purgeable_size", EmitTo::kSizeInUmaOnly, nullptr},
#if BUILDFLAG(IS_CHROMEOS)
    {"gpu/shared_images", "SharedImages.NonExo", MetricSize::kLarge,
     kNonExoSize, EmitTo::kSizeInUmaOnly, nullptr},
#endif  // BUILDFLAG(IS_CHROMEOS)
    {"gpu/transfer_cache", "ServiceTransferCache", MetricSize::kCustom, kSize,
     EmitTo::kSizeInUmaOnly, nullptr, ImageSizeMetricRange},
    {"gpu/transfer_cache", "ServiceTransferCache.AvgImageSize",
     MetricSize::kCustom, "average_size", EmitTo::kSizeInUmaOnly, nullptr,
     ImageSizeMetricRange},
    // For the Vulkan Memory Allocator, "allocated_size" is the amount of GPU
    // memory used by the allocator, not the amount allocated by clients, which
    // is "used_size".
    {"gpu/vulkan", "Vulkan", MetricSize::kLarge, "allocated_size",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/vulkan", "Vulkan.AllocatedObjects", MetricSize::kLarge, "used_size",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"gpu/vulkan", "Vulkan.Fragmentation", MetricSize::kLarge,
     "fragmentation_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"history", "History", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetHistory},
#if BUILDFLAG(IS_MAC)
    {"iosurface", "IOSurface", MetricSize::kLarge, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"iosurface", "IOSurface.DirtyMemory", MetricSize::kLarge,
     "resident_swapped", EmitTo::kSizeInUmaOnly, nullptr},
    {"iosurface", "IOSurface.NonPurgeable", MetricSize::kLarge,
     "nonpurgeable_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"iosurface", "IOSurface.Purgeable", MetricSize::kLarge, "purgeable_size",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"ioaccelerator", "IOAccelerator", MetricSize::kLarge, kSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"ioaccelerator", "IOAccelerator.DirtyMemory", MetricSize::kLarge,
     "resident_swapped", EmitTo::kSizeInUmaOnly, nullptr},
    {"ioaccelerator", "IOAccelerator.NonPurgeable", MetricSize::kLarge,
     "nonpurgeable_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"ioaccelerator", "IOAccelerator.Purgeable", MetricSize::kLarge,
     "purgeable_size", EmitTo::kSizeInUmaOnly, nullptr},
#endif
    {"java_heap", "JavaHeap", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetJavaHeap},
    {"leveldatabase", "LevelDatabase", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetLevelDatabase},
    {"malloc", "Malloc", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetMalloc},
    {"malloc/allocated_objects", "Malloc.AllocatedObjects", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetMalloc_AllocatedObjects},
    {"malloc/allocated_objects", "Malloc.AllocatedObjects.ObjectCount",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/original", "Malloc.Original.ObjectCount",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/aligned", "Malloc.Aligned.ObjectCount",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.Allocator.ObjectCount",
     MetricSize::kTiny, MemoryAllocatorDump::kNameObjectCount,
     EmitTo::kSizeInUmaOnly, nullptr},
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    {"malloc/partitions/allocator", "Malloc.BRPQuarantined", MetricSize::kSmall,
     "brp_quarantined_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.BRPQuarantinedCount",
     MetricSize::kTiny, "brp_quarantined_count", EmitTo::kSizeInUmaOnly,
     nullptr},
    {"partition_alloc/partitions", "PartitionAlloc.BRPQuarantined",
     MetricSize::kSmall, "brp_quarantined_size", EmitTo::kSizeInUmaOnly,
     nullptr},
    {"partition_alloc/partitions", "PartitionAlloc.BRPQuarantinedCount",
     MetricSize::kTiny, "brp_quarantined_count", EmitTo::kSizeInUmaOnly,
     nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.BRPQuarantined.FastMalloc", MetricSize::kSmall,
     "brp_quarantined_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.BRPQuarantinedCount.FastMalloc", MetricSize::kTiny,
     "brp_quarantined_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.BRPQuarantined.Buffer", MetricSize::kSmall,
     "brp_quarantined_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.BRPQuarantinedCount.Buffer", MetricSize::kTiny,
     "brp_quarantined_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.BRPQuarantined.ArrayBuffer", MetricSize::kSmall,
     "brp_quarantined_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.BRPQuarantinedCount.ArrayBuffer", MetricSize::kTiny,
     "brp_quarantined_count", EmitTo::kSizeInUmaOnly, nullptr},
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    {"malloc/partitions", "Malloc.BRPQuarantinedBytesPerMinute",
     MetricSize::kSmall, "brp_quarantined_bytes_per_minute",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions", "Malloc.BRPQuarantinedCountPerMinute",
     MetricSize::kTiny, "brp_quarantined_count_per_minute",
     EmitTo::kSizeInUmaOnly, nullptr},
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    {"malloc/extreme_lud/large_objects", "Malloc.ExtremeLUD.LargeObjects.Count",
     MetricSize::kTiny, "count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.SizeInBytes", MetricSize::kSmall,
     "size_in_bytes", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.CumulativeCount", MetricSize::kSmall,
     "cumulative_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.CumulativeSizeInBytes", MetricSize::kLarge,
     "cumulative_size_in_bytes", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.QuarantineMissCount", MetricSize::kTiny,
     "quarantine_miss_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.BytesPerMinute", MetricSize::kSmall,
     "bytes_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.CountPerMinute", MetricSize::kTiny,
     "count_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.MissCountPerMinute", MetricSize::kTiny,
     "miss_count_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/large_objects",
     "Malloc.ExtremeLUD.LargeObjects.QuarantinedTime", MetricSize::kSmall,
     "quarantined_time", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects", "Malloc.ExtremeLUD.SmallObjects.Count",
     MetricSize::kTiny, "count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.SizeInBytes", MetricSize::kSmall,
     "size_in_bytes", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.CumulativeCount", MetricSize::kSmall,
     "cumulative_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.CumulativeSizeInBytes", MetricSize::kLarge,
     "cumulative_size_in_bytes", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.QuarantineMissCount", MetricSize::kTiny,
     "quarantine_miss_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.BytesPerMinute", MetricSize::kSmall,
     "bytes_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.CountPerMinute", MetricSize::kTiny,
     "count_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.MissCountPerMinute", MetricSize::kTiny,
     "miss_count_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/extreme_lud/small_objects",
     "Malloc.ExtremeLUD.SmallObjects.QuarantinedTime", MetricSize::kSmall,
     "quarantined_time", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator/scheduler_loop_quarantine",
     "Malloc.SchedulerLoopQuarantine.Count", MetricSize::kTiny, "count",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator/scheduler_loop_quarantine",
     "Malloc.SchedulerLoopQuarantine.SizeInBytes", MetricSize::kSmall,
     "size_in_bytes", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator/scheduler_loop_quarantine",
     "Malloc.SchedulerLoopQuarantine.CumulativeCount", MetricSize::kTiny,
     "cumulative_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator/scheduler_loop_quarantine",
     "Malloc.SchedulerLoopQuarantine.CumulativeSizeInBytes", MetricSize::kSmall,
     "cumulative_size_in_bytes", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator/scheduler_loop_quarantine/",
     "Malloc.SchedulerLoopQuarantine.QuarantineMissCount", MetricSize::kTiny,
     "quarantine_miss_count", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator/thread_cache", "Malloc.ThreadCache",
     MetricSize::kSmall, kSize, EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.MaxAllocatedSize",
     MetricSize::kLarge, "max_allocated_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.MaxCommittedSize",
     MetricSize::kLarge, "max_committed_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.CommittedSize", MetricSize::kLarge,
     "virtual_committed_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.Wasted", MetricSize::kLarge,
     "wasted", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc/partitions/allocator", "Malloc.Fragmentation",
     MetricSize::kPercentage, "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"malloc", "Malloc.SyscallsPerMinute", MetricSize::kTiny,
     "syscalls_per_minute", EmitTo::kSizeInUmaOnly, nullptr},
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    {"mojo", "NumberOfMojoHandles", MetricSize::kSmall,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfMojoHandles},
    {"media/webmediaplayer/audio", "WebMediaPlayer.Audio", MetricSize::kSmall,
     kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebMediaPlayer_Audio},
    {"media/webmediaplayer/video", "WebMediaPlayer.Video", MetricSize::kLarge,
     kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebMediaPlayer_Video},
    {"media/webmediaplayer/data_source", "WebMediaPlayer.DataSource",
     MetricSize::kLarge, kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebMediaPlayer_DataSource},
    {"media/webmediaplayer/demuxer", "WebMediaPlayer.Demuxer",
     MetricSize::kLarge, kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebMediaPlayer_Demuxer},
    {"media/webmediaplayer", "WebMediaPlayer.Instances", MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount, EmitTo::kCountsInUkmOnly,
     &Memory_Experimental::SetNumberOfWebMediaPlayers},
    {"omnibox", "OmniboxSuggestions", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetOmniboxSuggestions},
    {"parkable_images", "ParkableImage.OnDiskSize", MetricSize::kSmall,
     "on_disk_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"parkable_images", "ParkableImage.UnparkedSize", MetricSize::kSmall,
     "unparked_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"parkable_images", "ParkableImage.TotalSize", MetricSize::kSmall,
     "total_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc", "PartitionAlloc", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetPartitionAlloc},
    {"partition_alloc/allocated_objects", "PartitionAlloc.AllocatedObjects",
     MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_AllocatedObjects},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.Partitions.ArrayBuffer", MetricSize::kLarge, kSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_ArrayBuffer},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.Fragmentation.ArrayBuffer", MetricSize::kPercentage,
     "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.MaxCommittedSize.ArrayBuffer", MetricSize::kLarge,
     "max_committed", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.CommittedSize.ArrayBuffer", MetricSize::kLarge,
     "virtual_committed_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.MaxAllocatedSize.ArrayBuffer", MetricSize::kLarge,
     "max_allocated_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.Wasted.ArrayBuffer", MetricSize::kLarge, "wasted",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/buffer", "PartitionAlloc.Partitions.Buffer",
     MetricSize::kLarge, kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_Buffer},
    {"partition_alloc/partitions/buffer", "PartitionAlloc.Fragmentation.Buffer",
     MetricSize::kPercentage, "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.MaxCommittedSize.Buffer", MetricSize::kLarge,
     "max_committed", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/buffer", "PartitionAlloc.CommittedSize.Buffer",
     MetricSize::kLarge, "virtual_committed_size", EmitTo::kSizeInUmaOnly,
     nullptr},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.MaxAllocatedSize.Buffer", MetricSize::kLarge,
     "max_allocated_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/buffer", "PartitionAlloc.Wasted.Buffer",
     MetricSize::kLarge, "wasted", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Partitions.FastMalloc", MetricSize::kLarge, kSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_FastMalloc},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Fragmentation.FastMalloc", MetricSize::kPercentage,
     "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.MaxCommittedSize.FastMalloc", MetricSize::kLarge,
     "max_committed", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.CommittedSize.FastMalloc", MetricSize::kLarge,
     "virtual_committed_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.MaxAllocatedSize.FastMalloc", MetricSize::kLarge,
     "max_allocated_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Wasted.FastMalloc", MetricSize::kLarge, "wasted",
     EmitTo::kSizeInUmaOnly, nullptr},
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    {"partition_alloc/partitions/fast_malloc/thread_cache",
     "PartitionAlloc.Partitions.FastMalloc.ThreadCache", MetricSize::kSmall,
     kSize, EmitTo::kSizeInUmaOnly, nullptr},
#endif
    {"partition_alloc/partitions/layout", "PartitionAlloc.Partitions.Layout",
     MetricSize::kLarge, kSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetPartitionAlloc_Partitions_Layout},
    {"partition_alloc/partitions/layout", "PartitionAlloc.Fragmentation.Layout",
     MetricSize::kPercentage, "fragmentation", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/layout",
     "PartitionAlloc.MaxCommittedSize.Layout", MetricSize::kLarge,
     "max_committed", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/layout", "PartitionAlloc.CommittedSize.Layout",
     MetricSize::kLarge, "virtual_committed_size", EmitTo::kSizeInUmaOnly,
     nullptr},
    {"partition_alloc/partitions/layout",
     "PartitionAlloc.MaxAllocatedSize.Layout", MetricSize::kLarge,
     "max_allocated_size", EmitTo::kSizeInUmaOnly, nullptr},
    {"partition_alloc/partitions/layout", "PartitionAlloc.Wasted.Layout",
     MetricSize::kLarge, "wasted", EmitTo::kSizeInUmaOnly, nullptr},
    {"passwords", "ManualFillingCache", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUmaOnly, nullptr},
    {"site_storage", "SiteStorage", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSiteStorage},
    {"site_storage/blob_storage", "SiteStorage.BlobStorage", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_BlobStorage},
    {"site_storage/index_db", "SiteStorage.IndexDB", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_IndexDB},
    {"site_storage/localstorage", "SiteStorage.LocalStorage",
     MetricSize::kSmall, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_LocalStorage},
    {"site_storage/session_storage", "SiteStorage.SessionStorage",
     MetricSize::kSmall, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSiteStorage_SessionStorage},
    {"skia", "Skia", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSkia},
    {"skia", "Skia.PurgeableSize", MetricSize::kLarge, "purgeable_size",
     EmitTo::kSizeInUmaOnly, nullptr},
    {"skia/gpu_resources", "SharedContextState", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kIgnored, nullptr},
    {"skia/sk_glyph_cache", "Skia.SkGlyphCache", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSkia_SkGlyphCache},
    {"skia/sk_resource_cache", "Skia.SkResourceCache", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetSkia_SkResourceCache},
    {"sqlite", "Sqlite", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSqlite},
    {"sync", "Sync", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetSync},
    {"tab_restore", "TabRestore", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetTabRestore},
    {"ui", "UI", MetricSize::kSmall, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetUI},
    {"v8", "V8", MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8},
    {"v8", "V8.AllocatedObjects", MetricSize::kLarge, kAllocatedObjectsSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_AllocatedObjects},
    {"v8/main", "V8.Main", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Main},
    {"v8/main", "V8.Main.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_AllocatedObjects},
    {"v8/main/global_handles", "V8.Main.GlobalHandles", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_GlobalHandles},
    {"v8/main/global_handles", "V8.Main.GlobalHandles.AllocatedObjects",
     MetricSize::kLarge, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_GlobalHandles_AllocatedObjects},
    {"v8/main/heap", "V8.Main.Heap", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Main_Heap},
    {"v8/main/heap", "V8.Main.Heap.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_AllocatedObjects},
    {"v8/main/heap/code_large_object_space",
     "V8.Main.Heap.CodeLargeObjectSpace", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_CodeLargeObjectSpace},
    {"v8/main/heap/code_large_object_space",
     "V8.Main.Heap.CodeLargeObjectSpace.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::
         SetV8_Main_Heap_CodeLargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/code_space", "V8.Main.Heap.CodeSpace", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_CodeSpace},
    {"v8/main/heap/code_space", "V8.Main.Heap.CodeSpace.AllocatedObjects",
     MetricSize::kLarge, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_CodeSpace_AllocatedObjects},
    {"v8/main/heap/large_object_space", "V8.Main.Heap.LargeObjectSpace",
     MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_LargeObjectSpace},
    {"v8/main/heap/large_object_space",
     "V8.Main.Heap.LargeObjectSpace.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_LargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/map_space", "V8.Main.Heap.MapSpace", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_MapSpace},
    {"v8/main/heap/map_space", "V8.Main.Heap.MapSpace.AllocatedObjects",
     MetricSize::kLarge, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_MapSpace_AllocatedObjects},
    {"v8/main/heap/new_large_object_space", "V8.Main.Heap.NewLargeObjectSpace",
     MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_NewLargeObjectSpace},
    {"v8/main/heap/new_large_object_space",
     "V8.Main.Heap.NewLargeObjectSpace.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::
         SetV8_Main_Heap_NewLargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/new_space", "V8.Main.Heap.NewSpace", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_NewSpace},
    {"v8/main/heap/new_space", "V8.Main.Heap.NewSpace.AllocatedObjects",
     MetricSize::kLarge, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_NewSpace_AllocatedObjects},
    {"v8/main/heap/old_space", "V8.Main.Heap.OldSpace", MetricSize::kLarge,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_OldSpace},
    {"v8/main/heap/old_space", "V8.Main.Heap.OldSpace.AllocatedObjects",
     MetricSize::kLarge, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_OldSpace_AllocatedObjects},
    {"v8/main/heap/read_only_space", "V8.Main.Heap.ReadOnlySpace",
     MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_ReadOnlySpace},
    {"v8/main/heap/read_only_space",
     "V8.Main.Heap.ReadOnlySpace.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_ReadOnlySpace_AllocatedObjects},
    {"v8/main/heap/large_object_space", "V8.Main.Heap.SharedLargeObjectSpace",
     MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_SharedLargeObjectSpace},
    {"v8/main/heap/large_object_space",
     "V8.Main.Heap.SharedLargeObjectSpace.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::
         SetV8_Main_Heap_SharedLargeObjectSpace_AllocatedObjects},
    {"v8/main/heap/shared_space", "V8.Main.Heap.SharedSpace",
     MetricSize::kLarge, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_SharedSpace},
    {"v8/main/heap/shared_space", "V8.Main.Heap.SharedSpace.AllocatedObjects",
     MetricSize::kLarge, kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Main_Heap_SharedSpace_AllocatedObjects},
    {"v8/main/malloc", "V8.Main.Malloc", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Main_Malloc},
    {"v8/workers", "V8.Workers", MetricSize::kLarge, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetV8_Workers},
    {"v8/workers", "V8.Workers.AllocatedObjects", MetricSize::kLarge,
     kAllocatedObjectsSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetV8_Workers_AllocatedObjects},
    {"web_cache", "WebCache", MetricSize::kSmall, kEffectiveSize,
     EmitTo::kSizeInUkmAndUma, &Memory_Experimental::SetWebCache},
    {"web_cache/Image_resources", "WebCache.ImageResources", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_ImageResources},
    {"web_cache/CSS stylesheet_resources", "WebCache.CSSStylesheetResources",
     MetricSize::kSmall, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_CSSStylesheetResources},
    {"web_cache/Script_resources", "WebCache.ScriptResources",
     MetricSize::kSmall, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_ScriptResources},
    {"web_cache/XSL stylesheet_resources", "WebCache.XSLStylesheetResources",
     MetricSize::kSmall, kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_XSLStylesheetResources},
    {"web_cache/Font_resources", "WebCache.FontResources", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_FontResources},
    {"web_cache/Code_cache", "WebCache.V8CodeCache", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_V8CodeCache},
    {"web_cache/Encoded_size_duplicated_in_data_urls",
     "WebCache.EncodedSizeDuplicatedInDataUrls", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_EncodedSizeDuplicatedInDataUrls},
    {"web_cache/Other_resources", "WebCache.OtherResources", MetricSize::kSmall,
     kEffectiveSize, EmitTo::kSizeInUkmAndUma,
     &Memory_Experimental::SetWebCache_OtherResources},
#if BUILDFLAG(IS_ANDROID)
    {base::android::MeminfoDumpProvider::kDumpName, "AndroidOtherPss",
     MetricSize::kLarge, base::android::MeminfoDumpProvider::kPssMetricName,
     EmitTo::kSizeInUmaOnly, nullptr},
    {base::android::MeminfoDumpProvider::kDumpName, "AndroidOtherPrivateDirty",
     MetricSize::kLarge,
     base::android::MeminfoDumpProvider::kPrivateDirtyMetricName,
     EmitTo::kSizeInUmaOnly, nullptr},
#endif
};

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Metrics specific to PartitionAlloc's address space stats (cf.
// kAllocatorDumpNamesForMetrics above). All of these metrics come in
// three variants: bare, after 1 hour, and after 24 hours. These metrics
// are only recorded in UMA.
const Metric kPartitionAllocAddressSpaceMetrics[] = {
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace.BlocklistSize",
        .metric_size = MetricSize::kTiny,
        .metric = "blocklist_size",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace.BlocklistHitCount",
        .metric_size = MetricSize::kTiny,
        .metric = "blocklist_hit_count",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace."
                    "RegularPoolLargestAvailableReservation",
        .metric_size = MetricSize::kLarge,
        .metric = "regular_pool_largest_reservation",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace.RegularPoolUsage",
        .metric_size = MetricSize::kLarge,
        .metric = "regular_pool_usage",
    },
    Metric{
        .uma_name =
            "PartitionAlloc.AddressSpace.BRPPoolLargestAvailableReservation",
        .metric_size = MetricSize::kLarge,
        .metric = "brp_pool_largest_reservation",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace.BRPPoolUsage",
        .metric_size = MetricSize::kLarge,
        .metric = "brp_pool_usage",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace."
                    "ConfigurablePoolLargestAvailableReservation",
        .metric_size = MetricSize::kLarge,
        .metric = "configurable_pool_largest_reservation",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace.ConfigurablePoolUsage",
        .metric_size = MetricSize::kLarge,
        .metric = "configurable_pool_usage",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace."
                    "ThreadIsolatedPoolLargestAvailableReservation",
        .metric_size = MetricSize::kLarge,
        .metric = "thread_isolated_pool_largest_reservation",
    },
    Metric{
        .uma_name = "PartitionAlloc.AddressSpace.ThreadIsolatedPoolUsage",
        .metric_size = MetricSize::kLarge,
        .metric = "thread_isolated_pool_usage",
    },
};
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Record a memory size in megabytes, over a potential interval up to 32 GB.
#define UMA_HISTOGRAM_LARGE_MEMORY_MB(name, sample) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 1, 32768, 50)

#define EXPERIMENTAL_UMA_PREFIX "Memory.Experimental."
#define VERSION_SUFFIX_PERCENT "2."
#define VERSION_SUFFIX_NORMAL "2."
#define VERSION_SUFFIX_SMALL "2.Small."
#define VERSION_SUFFIX_TINY "2.Tiny."
#define VERSION_SUFFIX_CUSTOM "2.Custom."

void EmitProcessUkm(const Metric& item,
                    uint64_t value,
                    Memory_Experimental* builder) {
  DCHECK(item.ukm_setter) << "UKM metrics must provide a setter";
  (builder->*(item.ukm_setter))(value);
}

const char* MetricSizeToVersionSuffix(MetricSize size) {
  switch (size) {
    case MetricSize::kPercentage:
      return VERSION_SUFFIX_PERCENT;
    case MetricSize::kLarge:
      return VERSION_SUFFIX_NORMAL;
    case MetricSize::kSmall:
      return VERSION_SUFFIX_SMALL;
    case MetricSize::kTiny:
      return VERSION_SUFFIX_TINY;
    case MetricSize::kCustom:
      return VERSION_SUFFIX_CUSTOM;
  }
}

void EmitProcessUma(HistogramProcessType process_type,
                    const Metric& item,
                    uint64_t value) {
  std::string uma_name;

  // Always use "Gpu" in process name for command buffers to be
  // consistent even in single process mode.
  if (std::string_view(item.uma_name) == "CommandBuffer") {
    uma_name =
        EXPERIMENTAL_UMA_PREFIX "Gpu" VERSION_SUFFIX_NORMAL "CommandBuffer";
    DCHECK(item.metric_size == MetricSize::kLarge);
  } else {
    uma_name = std::string(EXPERIMENTAL_UMA_PREFIX) +
               HistogramProcessTypeToString(process_type) +
               MetricSizeToVersionSuffix(item.metric_size) + item.uma_name;
  }

  switch (item.metric_size) {
    case MetricSize::kPercentage:
      base::UmaHistogramPercentage(uma_name, value);
      break;
    case MetricSize::kLarge:  // 1 - 64,000 MiB
      MEMORY_METRICS_HISTOGRAM_MB(uma_name, value / kMiB);
      break;
    case MetricSize::kSmall:  // 10 - 500,000 KiB
      base::UmaHistogramCustomCounts(uma_name, value / kKiB, 10, 500000, 100);
      break;
    case MetricSize::kTiny:  // 1 - 500,000 bytes
      base::UmaHistogramCustomCounts(uma_name, value, 1, 500000, 100);
      break;
    case MetricSize::kCustom:
      base::UmaHistogramCustomCounts(uma_name, value, item.range.min,
                                     item.range.max, 100);
      break;
  }
}

void EmitPartitionAllocFragmentationStat(
    const GlobalMemoryDump::ProcessDump& pmd,
    HistogramProcessType process_type,
    const char* dump_name,
    const char* uma_name) {
  std::optional<uint64_t> value = pmd.GetMetric(dump_name, "fragmentation");
  if (value.has_value()) {
    Metric fragmentation_metric = {dump_name,
                                   uma_name,
                                   MetricSize::kPercentage,
                                   "fragmentation",
                                   EmitTo::kSizeInUmaOnly,
                                   nullptr};
    EmitProcessUma(process_type, fragmentation_metric, value.value());
  }
}

void EmitPartitionAllocWastedStat(const GlobalMemoryDump::ProcessDump& pmd,
                                  HistogramProcessType process_type,
                                  const char* dump_name,
                                  const char* uma_name) {
  std::optional<uint64_t> value = pmd.GetMetric(dump_name, "wasted");
  if (value.has_value()) {
    Metric wasted_metric = {dump_name,
                            uma_name,
                            MetricSize::kLarge,
                            "wasted",
                            EmitTo::kSizeInUmaOnly,
                            nullptr};
    EmitProcessUma(process_type, wasted_metric, value.value());
  }
}

void EmitMallocStats(const GlobalMemoryDump::ProcessDump& pmd,
                     HistogramProcessType process_type,
                     const std::optional<base::TimeDelta>& uptime) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  const char* const kMallocDumpName = "malloc/partitions/allocator";
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  static constexpr int kRecordHours[] = {1, 24};
  // First element of the pair is the name as found in memory dumps, second
  // element is the corresponding name to emit in UMA.
  static constexpr std::pair<const char*, const char*> kPartitionNames[] = {
      {"array_buffer", "ArrayBuffer"},
      {"buffer", "Buffer"},
      {"fast_malloc", "FastMalloc"},
      {"layout", "Layout"}};

  for (int hours : kRecordHours) {
    if (uptime <= base::Hours(hours))
      continue;

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    EmitPartitionAllocFragmentationStat(
        pmd, process_type, kMallocDumpName,
        base::StringPrintf("Malloc.Fragmentation.After%dH", hours).c_str());
    EmitPartitionAllocWastedStat(
        pmd, process_type, kMallocDumpName,
        base::StringPrintf("Malloc.Wasted.After%dH", hours).c_str());
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

    for (const auto& partition_name : kPartitionNames) {
      const auto* dump_name = partition_name.first;
      const auto* uma_name = partition_name.second;
      EmitPartitionAllocFragmentationStat(
          pmd, process_type, dump_name,
          base::StringPrintf("PartitionAlloc.Fragmentation.%s.After%dH",
                             uma_name, hours)
              .c_str());
      EmitPartitionAllocWastedStat(
          pmd, process_type, dump_name,
          base::StringPrintf("PartitionAlloc.Wasted.%s.After%dH", uma_name,
                             hours)
              .c_str());
    }
  }
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void EmitPartitionAllocAddressSpaceStatVariants(
    const Metric& metric,
    const uint64_t metric_value,
    HistogramProcessType process_type,
    const std::optional<base::TimeDelta>& uptime) {
  // Emit the bare metric.
  EmitProcessUma(process_type, metric, metric_value);

  // These address space stats also come in variants for "after 1H"
  // and "after 24H." If the time is right, we emit those too.
  if (!uptime.has_value()) {
    return;
  }
  static constexpr int kRecordHours[] = {1, 24};
  for (int hours : kRecordHours) {
    if (uptime.value() <= base::Hours(hours)) {
      continue;
    }
    const std::string uma_name_with_time =
        base::StringPrintf("%s.After%dH", metric.uma_name, hours);
    EmitProcessUma(process_type,
                   // Lazily populated only with applicable members.
                   Metric{
                       .uma_name = uma_name_with_time.c_str(),
                       .metric_size = metric.metric_size,
                   },
                   metric_value);
  }
}

void EmitPartitionAllocAddressSpaceStats(
    const GlobalMemoryDump::ProcessDump& pmd,
    HistogramProcessType process_type,
    const std::optional<base::TimeDelta>& uptime) {
  for (const auto& metric : kPartitionAllocAddressSpaceMetrics) {
    std::optional<uint64_t> metric_value =
        pmd.GetMetric("partition_alloc/address_space", metric.metric);
    if (!metric_value.has_value()) {
      continue;
    }
    EmitPartitionAllocAddressSpaceStatVariants(metric, metric_value.value(),
                                               process_type, uptime);
  }
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

void EmitProcessUmaAndUkm(const GlobalMemoryDump::ProcessDump& pmd,
                          HistogramProcessType process_type,
                          const std::optional<base::TimeDelta>& uptime,
                          bool record_uma,
                          Memory_Experimental* builder) {
  for (const auto& item : kAllocatorDumpNamesForMetrics) {
    std::optional<uint64_t> value = pmd.GetMetric(item.dump_name, item.metric);
    if (!value)
      continue;

    switch (item.target) {
      case EmitTo::kCountsInUkmOnly:
        EmitProcessUkm(item, value.value(), builder);
        break;
      case EmitTo::kCountsInUkmAndSizeInUma:
        EmitProcessUkm(item, value.value(), builder);
        if (record_uma)
          EmitProcessUma(process_type, item, value.value());
        break;
      case EmitTo::kSizeInUmaOnly:
        if (record_uma)
          EmitProcessUma(process_type, item, value.value());
        break;
      case EmitTo::kSizeInUkmAndUma:
        // For each 'size' metric, emit size as MB.
        EmitProcessUkm(item, value.value() / kMiB, builder);
        if (record_uma)
          EmitProcessUma(process_type, item, value.value());
        break;
      case EmitTo::kIgnored:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Resident set is not populated on Mac.
  builder->SetResident(pmd.os_dump().resident_set_kb / kKiB);

  builder->SetPrivateMemoryFootprint(pmd.os_dump().private_footprint_kb / kKiB);
  builder->SetSharedMemoryFootprint(pmd.os_dump().shared_footprint_kb / kKiB);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  builder->SetPrivateSwapFootprint(pmd.os_dump().private_footprint_swap_kb /
                                   kKiB);
#endif
  if (uptime)
    builder->SetUptime(uptime.value().InSeconds());
  if (!record_uma)
    return;

  const char* process_name = HistogramProcessTypeToString(process_type);

  MEMORY_METRICS_HISTOGRAM_MB(
      std::string(kMemoryHistogramPrefix) + process_name + ".ResidentSet",
      pmd.os_dump().resident_set_kb / kKiB);
  MEMORY_METRICS_HISTOGRAM_MB(GetPrivateFootprintHistogramName(process_type),
                              pmd.os_dump().private_footprint_kb / kKiB);
  MEMORY_METRICS_HISTOGRAM_MB(std::string(kMemoryHistogramPrefix) +
                                  process_name + ".SharedMemoryFootprint",
                              pmd.os_dump().shared_footprint_kb / kKiB);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  MEMORY_METRICS_HISTOGRAM_MB(std::string(kMemoryHistogramPrefix) +
                                  process_name + ".PrivateSwapFootprint",
                              pmd.os_dump().private_footprint_swap_kb / kKiB);
#endif

  if (record_uma) {
    EmitMallocStats(pmd, process_type, uptime);
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    EmitPartitionAllocAddressSpaceStats(pmd, process_type, uptime);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  }
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
                             MetricSize::kLarge,
                             kEffectiveSize,
                             EmitTo::kSizeInUkmAndUma,
                             &Memory_Experimental::SetGpuMemory};

  uint64_t total = 0;
  for (size_t i = 0; i < std::size(gpu_categories); ++i) {
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

#if BUILDFLAG(IS_CHROMEOS)
void EmitGpuMemoryNonExo(const GlobalMemoryDump::ProcessDump& pmd,
                         bool record_uma) {
  if (!record_uma) {
    return;
  }
  Metric synthetic_metric = {
      nullptr, "GpuMemoryNonExo",      MetricSize::kLarge,
      kSize,   EmitTo::kSizeInUmaOnly, nullptr};

  // Combine several categories together to sum up Chrome-reported gpu memory.
  uint64_t total = 0;
  total += pmd.GetMetric("gpu/shared_images", kNonExoSize).value_or(0);
  total += pmd.GetMetric("skia/gpu_resources", kSize).value_or(0);

  // We only report this metric for the GPU process, so we always use kGpu.
  EmitProcessUma(HistogramProcessType::kGpu, synthetic_metric, total);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void EmitBrowserMemoryMetrics(const GlobalMemoryDump::ProcessDump& pmd,
                              ukm::SourceId ukm_source_id,
                              ukm::UkmRecorder* ukm_recorder,
                              const std::optional<base::TimeDelta>& uptime,
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
    const std::optional<base::TimeDelta>& uptime,
    bool record_uma) {
  // If the renderer doesn't host a single page, no page_info will be passed in,
  // and there's no single URL to associate its memory with.
  ukm::SourceId ukm_source_id =
      page_info ? page_info->ukm_source_id : ukm::NoURLSourceId();

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
                          const std::optional<base::TimeDelta>& uptime,
                          bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(
      static_cast<int64_t>(memory_instrumentation::mojom::ProcessType::GPU));
  EmitProcessUmaAndUkm(pmd, HistogramProcessType::kGpu, uptime, record_uma,
                       &builder);
  EmitSummedGpuMemory(pmd, &builder, record_uma);
#if BUILDFLAG(IS_CHROMEOS)
  EmitGpuMemoryNonExo(pmd, record_uma);
#endif
  builder.Record(ukm_recorder);
}

void EmitUtilityMemoryMetrics(HistogramProcessType ptype,
                              const GlobalMemoryDump::ProcessDump& pmd,
                              ukm::SourceId ukm_source_id,
                              ukm::UkmRecorder* ukm_recorder,
                              const std::optional<base::TimeDelta>& uptime,
                              bool record_uma) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::UTILITY));
  EmitProcessUmaAndUkm(pmd, ptype, uptime, record_uma, &builder);

  builder.Record(ukm_recorder);
}

#if BUILDFLAG(IS_ANDROID)
// Return the base::android::ChildBindingState if the process with `pid` is a
// renderer. If the `pid` is not in the list of live renderers it is assumed to
// be unbound. If the `process_type` is not for a renderer return nullopt.
std::optional<base::android::ChildBindingState>
GetAndroidRendererProcessBindingState(
    memory_instrumentation::mojom::ProcessType process_type,
    base::ProcessId pid) {
  if (process_type != memory_instrumentation::mojom::ProcessType::RENDERER) {
    return std::nullopt;
  }
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (!iter.GetCurrentValue()->GetProcess().IsValid())
      continue;

    if (iter.GetCurrentValue()->GetProcess().Pid() == pid) {
      return iter.GetCurrentValue()->GetEffectiveChildBindingState();
    }
  }
  // This can occur if the process no longer exists. Specifically, it is
  // possible a memory dump was requested, but the process was killed before
  // reaching this point so we cannot check its status. Treat as UNBOUND.
  return base::android::ChildBindingState::UNBOUND;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter()
    : pid_scope_(base::kNullProcessId) {}

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter(
    base::ProcessId pid_scope)
    : pid_scope_(pid_scope) {}

void ProcessMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

// https://crbug.com/330751658 (hopefully temporary).
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86_64)
  // Do not skip when command-line is used to specifically test it.
  if (base::GetPageSize() == 16 * 1024) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    int test_delay_in_minutes = 0;
    if (command_line->HasSwitch(switches::kTestMemoryLogDelayInMinutes)) {
      base::StringToInt(command_line->GetSwitchValueASCII(
                            switches::kTestMemoryLogDelayInMinutes),
                        &test_delay_in_minutes);
    }
    // Allow a negative value to test the crashing scenario.
    if (test_delay_in_minutes >= 0) {
      LOG(WARNING) << "Ignoring dump request to avoid emulator crash. "
                   << "https://crbug.com/330751658";
      return;
    }
  }
#endif

  MarkServiceRequestsInProgress();

  auto* instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  // nullptr means content layer is not initialized yet (there's no memory
  // metrics to log in this case)
  if (instrumentation) {
    // The callback keeps this object alive until the callback is invoked.
    auto callback =
        base::BindOnce(&ProcessMemoryMetricsEmitter::ReceivedMemoryDump, this);
    std::vector<std::string> mad_list;
    for (const auto& metric : kAllocatorDumpNamesForMetrics)
      mad_list.push_back(metric.dump_name);
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    mad_list.push_back("partition_alloc/address_space");
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    if (pid_scope_ != base::kNullProcessId) {
      instrumentation->RequestGlobalDumpForPid(pid_scope_, mad_list,
                                               std::move(callback));
    } else {
      instrumentation->RequestGlobalDump(mad_list, std::move(callback));
    }
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
      base::SequencedTaskRunner::GetCurrentDefault(),
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Retrieve the renderer process host for the given pid.

  content::RenderProcessHost* rph = nullptr;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (!iter.GetCurrentValue()->GetProcess().IsValid())
      continue;

    if (iter.GetCurrentValue()->GetProcess().Pid() == pid) {
      rph = iter.GetCurrentValue();
      break;
    }
  }
  if (!rph) {
    return 0;
  }

  // Count the number of extensions associated with this `rph`'s profile.
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(rph->GetBrowserContext());
  if (!process_map) {
    return 0;
  }

  const extensions::Extension* extension =
      process_map->GetEnabledExtensionByProcessID(rph->GetID());
  // Only include this extension if it's not a hosted app.
  return (extension && !extension->is_hosted_app()) ? 1 : 0;
#else
  return 0;
#endif
}

std::optional<base::TimeDelta> ProcessMemoryMetricsEmitter::GetProcessUptime(
    base::TimeTicks now,
    base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto process_info = process_infos_.find(pid);
  if (process_info != process_infos_.end()) {
    if (!process_info->second.launch_time.is_null())
      return now - process_info->second.launch_time;
  }
  return std::optional<base::TimeDelta>();
}

void ProcessMemoryMetricsEmitter::CollateResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (memory_dump_in_progress_ || get_process_urls_in_progress_)
    return;
  // The memory dump can be done, yet |global_dump_| not set if:
  // - Process metrics collection fails first.
  // - Process Infos arrive later.
  if (!global_dump_)
    return;

  uint32_t private_footprint_total_kb = 0;
#if BUILDFLAG(IS_ANDROID)
  uint32_t private_footprint_excluding_waived_total_kb = 0;
  uint32_t renderer_private_footprint_excluding_waived_total_kb = 0;
  uint32_t private_footprint_visible_or_higher_total_kb = 0;
  uint32_t renderer_private_footprint_visible_or_higher_total_kb = 0;
#endif  // BUILDFLAG(IS_ANDROID)
  uint32_t renderer_private_footprint_total_kb = 0;
  uint32_t renderer_malloc_total_kb = 0;
  uint32_t renderer_blink_gc_total_kb = 0;
  uint32_t renderer_blink_gc_fragmentation_total_kb = 0;
  uint32_t shared_footprint_total_kb = 0;
  uint32_t resident_set_total_kb = 0;
  uint64_t tiles_total_memory = 0;
  uint64_t hibernated_canvas_total_memory = 0;
  uint64_t hibernated_canvas_total_original_memory = 0;
  uint64_t gpu_mapped_memory_total = 0;
  bool emit_metrics_for_all_processes = pid_scope_ == base::kNullProcessId;

  TabFootprintAggregator per_tab_metrics;

  base::TimeTicks now = base::TimeTicks::Now();
  for (const auto& pmd : global_dump_->process_dumps()) {
    uint32_t process_pmf_kb = pmd.os_dump().private_footprint_kb;
    private_footprint_total_kb += process_pmf_kb;
#if BUILDFLAG(IS_ANDROID)
    bool is_waived_renderer = false;
    bool is_less_than_visible_renderer = false;
    auto renderer_binding_state_android =
        GetAndroidRendererProcessBindingState(pmd.process_type(), pmd.pid());
    if (renderer_binding_state_android) {
      // Also exclude base::android::ChildBindingState::UNBOUND which can occur
      // as the state change can be racy.
      is_waived_renderer = *renderer_binding_state_android <=
                           base::android::ChildBindingState::WAIVED;
      is_less_than_visible_renderer = *renderer_binding_state_android <
                                      base::android::ChildBindingState::VISIBLE;
    }
    private_footprint_excluding_waived_total_kb +=
        is_waived_renderer ? 0 : process_pmf_kb;
    private_footprint_visible_or_higher_total_kb +=
        is_less_than_visible_renderer ? 0 : process_pmf_kb;
#endif  // BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
        renderer_private_footprint_excluding_waived_total_kb +=
            is_waived_renderer ? 0 : process_pmf_kb;
        renderer_private_footprint_visible_or_higher_total_kb +=
            is_less_than_visible_renderer ? 0 : process_pmf_kb;
#endif  // BUILDFLAG(IS_ANDROID)
        hibernated_canvas_total_memory +=
            pmd.GetMetric("canvas/hibernated", kSize).value_or(0);
        hibernated_canvas_total_original_memory +=
            pmd.GetMetric("canvas/hibernated", "original_size").value_or(0);
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

          // If there is more than one tab being hosted in a renderer, don't
          // emit certain data. This is not ideal, but UKM does not support
          // multiple-URLs per entry, and we must have one entry per process.
          if (process_info.page_infos.size() == 1) {
            single_page_info = &process_info.page_infos[0];
          }
        }

        // Sum malloc memory from all renderers.
        renderer_malloc_total_kb +=
            pmd.GetMetric("malloc", "effective_size").value_or(0) / kKiB;

        // Sum Blink memory from all renderers.
        const uint64_t blink_gc_bytes =
            pmd.GetMetric("blink_gc", kEffectiveSize).value_or(0);
        const uint64_t blink_gc_allocated_objects_bytes =
            pmd.GetMetric("blink_gc", kAllocatedObjectsSize).value_or(0);
        renderer_blink_gc_total_kb += blink_gc_bytes / kKiB;
        renderer_blink_gc_fragmentation_total_kb +=
            (blink_gc_bytes - blink_gc_allocated_objects_bytes) / kKiB;

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
        HistogramProcessType ptype;
        if (pmd.pid() == content::GetProcessIdForAudioService()) {
          ptype = HistogramProcessType::kAudioService;
        } else if (pmd.service_name() ==
                   media::mojom::CdmServiceBroker::Name_) {
          ptype = HistogramProcessType::kCdmService;
#if BUILDFLAG(IS_WIN)
        } else if (pmd.service_name() ==
                   media::mojom::MediaFoundationServiceBroker::Name_) {
          ptype = HistogramProcessType::kMediaFoundationService;
#endif
        } else if (pmd.service_name() ==
                   network::mojom::NetworkService::Name_) {
          ptype = HistogramProcessType::kNetworkService;
        } else if (pmd.service_name() ==
                   paint_preview::mojom::PaintPreviewCompositorCollection::
                       Name_) {
          ptype = HistogramProcessType::kPaintPreviewCompositor;
        } else {
          ptype = HistogramProcessType::kUtility;
        }
        EmitUtilityMemoryMetrics(
            ptype, pmd, ukm::UkmRecorder::GetNewSourceID(), GetUkmRecorder(),
            GetProcessUptime(now, pmd.pid()), emit_metrics_for_all_processes);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::PLUGIN:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::ARC:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::OTHER:
        break;
    }

    if (emit_metrics_for_all_processes) {
      // cc/ has clients in all process types. Careful though about
      // double-counting: in the GPU process a lot of the memory is allocated on
      // behalf of other process types, so the size vs effective_size
      // distinction matters there.
      //
      // Not using effective size as tiles are not shared across processes, but
      // they are shared with the GPU process (under a different name), and we
      // don't want to count these partially if priority is not set right.
      tiles_total_memory += pmd.GetMetric("cc/tile_memory", kSize).value_or(0);
      gpu_mapped_memory_total +=
          pmd.GetMetric("gpu/mapped_memory", kSize).value_or(0);
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
          "Memory.NativeLibrary.MappedAndResidentMemoryFootprint3",
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

    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.ResidentSet",
                                  resident_set_total_kb / kKiB);

    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.PrivateMemoryFootprint",
                                  private_footprint_total_kb / kKiB);
    // The pseudo metric of Memory.Total.PrivateMemoryFootprint. Only used to
    // assess field trial data quality.
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "UMA.Pseudo.Memory.Total.PrivateMemoryFootprint",
        metrics::GetPseudoMetricsSample(
            static_cast<double>(private_footprint_total_kb) / kKiB));
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.RendererPrivateMemoryFootprint",
                                  renderer_private_footprint_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.RendererMalloc",
                                  renderer_malloc_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.RendererBlinkGC",
                                  renderer_blink_gc_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Total.RendererBlinkGC.Fragmentation",
        renderer_blink_gc_fragmentation_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.SharedMemoryFootprint",
                                  shared_footprint_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_MEDIUM_MB("Memory.Total.TileMemory",
                                   tiles_total_memory / kMiB);
    UMA_HISTOGRAM_MEMORY_MEDIUM_MB("Memory.Total.GpuMappedMemory",
                                   gpu_mapped_memory_total / kMiB);
#if BUILDFLAG(IS_ANDROID)
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Total.PrivateMemoryFootprintExcludingWaivedRenderers",
        private_footprint_excluding_waived_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Total.RendererPrivateMemoryFootprintExcludingWaived",
        renderer_private_footprint_excluding_waived_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Total.PrivateMemoryFootprintVisibleOrHigherPriorityRenderers",
        private_footprint_visible_or_higher_total_kb / kKiB);
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Total.RendererPrivateMemoryFootprintVisibleOrHigherPriority",
        renderer_private_footprint_visible_or_higher_total_kb / kKiB);
#endif

    UMA_HISTOGRAM_MEMORY_MEDIUM_MB("Memory.Total.HibernatedCanvas.Size",
                                   hibernated_canvas_total_memory / kMiB);
    UMA_HISTOGRAM_MEMORY_MEDIUM_MB(
        "Memory.Total.HibernatedCanvas.OriginalSize",
        hibernated_canvas_total_original_memory / kMiB);

    Memory_Experimental(ukm::UkmRecorder::GetNewSourceID())
        .SetTotal2_PrivateMemoryFootprint(private_footprint_total_kb / kKiB)
        .SetTotal2_SharedMemoryFootprint(shared_footprint_total_kb / kKiB)
        .Record(GetUkmRecorder());

    // Renderer metrics-by-tab only make sense if we're visiting all render
    // processes.
    per_tab_metrics.RecordPmfs(GetUkmRecorder());

#if BUILDFLAG(IS_CHROMEOS)
    base::SystemMemoryInfoKB system_meminfo;
    if (base::GetSystemMemoryInfo(&system_meminfo)) {
      int mem_used_mb =
          (system_meminfo.total - system_meminfo.available) / 1024;
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.System.MemAvailableMB",
                                    system_meminfo.available / 1024);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.System.MemUsedMB", mem_used_mb);
    }
#endif
  }

  global_dump_ = nullptr;
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
  // Assign page nodes unique IDs within this lookup only.
  base::flat_map<const performance_manager::PageNode*, uint64_t> page_id_map;
  for (const performance_manager::ProcessNode* process_node :
       graph->GetAllProcessNodes()) {
    if (process_node->GetProcessId() == base::kNullProcessId)
      continue;

    // First add all processes and their basic information.
    ProcessInfo& process_info = process_infos.emplace_back();
    process_info.pid = process_node->GetProcessId();
    process_info.launch_time = process_node->GetLaunchTime();

    // Then add information about their associated page nodes. Only renderers
    // are associated with page nodes.
    if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
      continue;
    }

    base::flat_set<const performance_manager::PageNode*> page_nodes =
        performance_manager::GraphOperations::GetAssociatedPageNodes(
            process_node);
    for (const performance_manager::PageNode* page_node : page_nodes) {
      if (page_node->GetUkmSourceID() == ukm::kInvalidSourceId)
        continue;

      // Get or generate the tab id.
      uint64_t& tab_id = page_id_map[page_node];
      if (tab_id == 0u) {
        // 0 is an invalid id, meaning `page_node` was just inserted in
        // `page_id_map` and its tab id must be generated.
        tab_id = page_id_map.size();
      }

      PageInfo& page_info = process_info.page_infos.emplace_back();
      page_info.ukm_source_id = page_node->GetUkmSourceID();

      page_info.tab_id = tab_id;
      page_info.hosts_main_frame = HostsMainFrame(process_node, page_node);
      page_info.is_visible = page_node->IsVisible();
      page_info.time_since_last_visibility_change =
          page_node->GetTimeSinceLastVisibilityChange();
      page_info.time_since_last_navigation =
          page_node->GetTimeSinceLastNavigation();
    }
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
