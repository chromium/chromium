# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# PartitionAlloc library must not depend on Chromium
# project in order to be a standalone library.
noparent = True

# `partition_alloc` can depend only on itself, via its `include_dirs`.
include_rules = [ "+partition_alloc" ]

# TODO(crbug.com/40158212): Depending on what is tested, split the tests in
# between chromium and partition_alloc. Remove those exceptions:
specific_include_rules = {
  # Dependencies on //testing:
  ".*_(perf|unit)?test.*\.(h|cc)": [
    "+testing/gmock/include/gmock/gmock.h",
    "+testing/gtest/include/gtest/gtest.h",
    "+testing/perf/perf_result_reporter.h",
  ],
  "gtest_util.h": [
    "+testing/gtest/include/gtest/gtest.h",
  ],

  # Dependencies on //base:
  "extended_api\.cc": [
    "+base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h",
  ],
  "partition_alloc_perftest\.cc": [
    "+base/allocator/dispatcher/dispatcher.h",
    "+base/debug/allocation_trace.h",
    "+base/debug/debugging_buildflags.h",
    "+base/timer/lap_timer.h",
  ],
  "partition_lock_perftest\.cc": [
    "+base/timer/lap_timer.h",
  ],
  "raw_ptr_unittest\.cc": [
    "+base/allocator/partition_alloc_features.h",
    "+base/allocator/partition_alloc_support.h",
    "+base/cpu.h",
    "+base/debug/asan_service.h",
    "+base/metrics/histogram_base.h",
    "+base/test/bind.h",
    "+base/test/gtest_util.h",
    "+base/test/memory/dangling_ptr_instrumentation.h",
    "+base/test/scoped_feature_list.h",
    "+base/types/to_address.h",
  ],
  "raw_ref_unittest\.cc": [
    "+base/debug/asan_service.h",
    "+base/memory/raw_ptr_asan_service.h",
    "+base/test/gtest_util.h",
  ],
}

# In the context of a module-level DEPS, the `deps` variable must be defined.
# Some tools relies on it. For instance dawn/tools/fetch_dawn_dependencies.py
# This has no use in other contexts.
deps = {}
