// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Definition of a single metric entry to be extracted from a ProcessMemoryDump.
 */
export interface MetricDef {
  /** User-visible label for the metric. */
  label: string;
  /**
   * Name of the allocator dump in chromeAllocatorDumps (or empty for OS dump).
   */
  dumpName: string;
  /**
   * Name of the numeric metric within the allocator dump (or OS dump field).
   */
  metricName: string;
  /** Display unit and formatting type. */
  unit: 'bytes'|'kib'|'count'|'percent';
}

/**
 * Definition of a collapsible section grouping related metrics.
 */
export interface SectionDef {
  /** Unique section identifier. */
  id: string;
  /** User-visible label for the section header. */
  label: string;
  /** List of metric definitions contained in this section. */
  metrics: MetricDef[];
}

export const SECTION_DEFS: SectionDef[] = [
  {
    id: 'os',
    label: 'OS Metrics',
    metrics: [
      {
        label: 'Private Memory Footprint',
        dumpName: '',
        metricName: 'private_footprint_kb',
        unit: 'kib',
      },
      {
        label: 'Resident Set Size (RSS)',
        dumpName: '',
        metricName: 'resident_set_kb',
        unit: 'kib',
      },
      {
        label: 'Shared Footprint',
        dumpName: '',
        metricName: 'shared_footprint_kb',
        unit: 'kib',
      },
    ],
  },
  {
    id: 'malloc',
    label: 'Malloc',
    metrics: [
      {
        label: 'Malloc Total',
        dumpName: 'malloc',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Allocated Objects',
        dumpName: 'malloc/allocated_objects',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Wasted Memory',
        dumpName: 'malloc/partitions/allocator',
        metricName: 'wasted',
        unit: 'bytes',
      },
      {
        label: 'Committed Virtual Memory',
        dumpName: 'malloc/partitions/allocator',
        metricName: 'virtual_committed_size',
        unit: 'bytes',
      },
      {
        label: 'Thread Cache',
        dumpName: 'malloc/partitions/allocator/thread_cache',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Scheduler Loop Quarantine',
        dumpName: 'malloc/partitions/allocator/scheduler_loop_quarantine',
        metricName: 'size_in_bytes',
        unit: 'bytes',
      },
      {
        label: 'Original Partition Objects',
        dumpName: 'malloc/partitions/original',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'Aligned Partition Objects',
        dumpName: 'malloc/partitions/aligned',
        metricName: 'object_count',
        unit: 'count',
      },
    ],
  },
  {
    id: 'partition_alloc',
    label: 'PartitionAlloc',
    metrics: [
      {
        label: 'PartitionAlloc Total',
        dumpName: 'partition_alloc',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Allocated Objects',
        dumpName: 'partition_alloc/allocated_objects',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Partition: ArrayBuffer',
        dumpName: 'partition_alloc/partitions/array_buffer',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Partition: ArrayBuffer - Wasted',
        dumpName: 'partition_alloc/partitions/array_buffer',
        metricName: 'wasted',
        unit: 'bytes',
      },
      {
        label: 'Partition: Buffer',
        dumpName: 'partition_alloc/partitions/buffer',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Partition: Buffer - Wasted',
        dumpName: 'partition_alloc/partitions/buffer',
        metricName: 'wasted',
        unit: 'bytes',
      },
      {
        label: 'Partition: FastMalloc',
        dumpName: 'partition_alloc/partitions/fast_malloc',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Partition: FastMalloc - Wasted',
        dumpName: 'partition_alloc/partitions/fast_malloc',
        metricName: 'wasted',
        unit: 'bytes',
      },
      {
        label: 'Partition: Layout',
        dumpName: 'partition_alloc/partitions/layout',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Partition: Layout - Wasted',
        dumpName: 'partition_alloc/partitions/layout',
        metricName: 'wasted',
        unit: 'bytes',
      },
    ],
  },
  {
    id: 'blink_gc',
    label: 'BlinkGC',
    metrics: [
      {
        label: 'Blink GC Total',
        dumpName: 'blink_gc',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Allocated Objects',
        dumpName: 'blink_gc',
        metricName: 'allocated_objects_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap',
        dumpName: 'blink_gc/main',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap Allocated Objects',
        dumpName: 'blink_gc/main',
        metricName: 'allocated_objects_size',
        unit: 'bytes',
      },
      {
        label: 'DOM Documents',
        dumpName: 'blink_objects/Document',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM Nodes',
        dumpName: 'blink_objects/Node',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM Frames',
        dumpName: 'blink_objects/Frame',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM Layout Objects',
        dumpName: 'blink_objects/LayoutObject',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM ArrayBufferContents',
        dumpName: 'blink_objects/ArrayBufferContents',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM Resources',
        dumpName: 'blink_objects/Resource',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM Audio Handlers',
        dumpName: 'blink_objects/AudioHandler',
        metricName: 'object_count',
        unit: 'count',
      },
      {
        label: 'DOM JS Event Listeners',
        dumpName: 'blink_objects/JSEventListener',
        metricName: 'object_count',
        unit: 'count',
      },
    ],
  },
  {
    id: 'v8',
    label: 'V8',
    metrics: [
      {
        label: 'V8 Total',
        dumpName: 'v8',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Allocated Objects',
        dumpName: 'v8',
        metricName: 'allocated_objects_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap',
        dumpName: 'v8/main/heap',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap: Old Space',
        dumpName: 'v8/main/heap/old_space',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap: New Space',
        dumpName: 'v8/main/heap/new_space',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap: Code Space',
        dumpName: 'v8/main/heap/code_space',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Main Heap: Map Space',
        dumpName: 'v8/main/heap/map_space',
        metricName: 'effective_size',
        unit: 'bytes',
      },
    ],
  },
  {
    id: 'components',
    label: 'Breakdown per component',
    metrics: [
      {
        label: 'Skia',
        dumpName: 'skia',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'GPU: Shared Images',
        dumpName: 'gpu/shared_images',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Dawn: Shared Context',
        dumpName: 'gpu/dawn',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Dawn: Buffers',
        dumpName: 'gpu/dawn/buffers',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Dawn: Textures',
        dumpName: 'gpu/dawn/textures',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Compositor: Tile Memory',
        dumpName: 'cc/tile_memory',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Discardable',
        dumpName: 'discardable',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'Hibernated Canvas',
        dumpName: 'canvas/hibernated',
        metricName: 'size',
        unit: 'bytes',
      },
      {
        label: 'Font Caches',
        dumpName: 'font_caches',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'LevelDB',
        dumpName: 'leveldatabase',
        metricName: 'effective_size',
        unit: 'bytes',
      },
      {
        label: 'SQLite',
        dumpName: 'sqlite',
        metricName: 'effective_size',
        unit: 'bytes',
      },
    ],
  },
];
