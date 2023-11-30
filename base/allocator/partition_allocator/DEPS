# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# PartitionAlloc library must not depend on Chromium
# project in order to be a standalone library.
noparent = True

include_rules = [
  # `partition_alloc` can depends on itself, via the `include_dirs` it declares.
  "+partition_alloc",

  # Build flags to infer the architecture and operating system in use.
  "+build/build_config.h",
  "+build/buildflag.h",
]

specific_include_rules = {
  ".*_(perf|unit)test\.cc$": [
    "+base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h",
    "+base/allocator/dispatcher/dispatcher.h",
    "+base/debug/allocation_trace.h",
    "+base/debug/debugging_buildflags.h",
    "+base/debug/proc_maps_linux.h",
    "+base/system/sys_info.h",
    "+base/test/gtest_util.h",
    "+base/timer/lap_timer.h",
    "+base/win/windows_version.h",
    "+testing/gmock/include/gmock/gmock.h",
    "+testing/gtest/include/gtest/gtest.h",
    "+testing/perf/perf_result_reporter.h",
  ],
  "extended_api\.cc$": [
    "+base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h",
  ],
  "raw_(ptr|ref)_unittest\.cc$": [
    "+base",
    "+third_party/abseil-cpp/absl/types/optional.h",
    "+third_party/abseil-cpp/absl/types/variant.h",
  ],
  "raw_ptr_test_support\.h$": [
    "+testing/gmock/include/gmock/gmock.h",
    "+third_party/abseil-cpp/absl/types/optional.h",
  ]
}

# In the context of a module-level DEPS, the `deps` variable must be defined.
# Some tools relies on it. For instance dawn/tools/fetch_dawn_dependencies.py
# This has no use in other contexts.
deps = {}
