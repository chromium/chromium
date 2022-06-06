# PartitionAlloc is planned to be extracted into a standalone library, and
# therefore dependencies need to be strictly controlled and minimized.

noparent = True

include_rules = [
    "+base/allocator/buildflags.h",
    "+base/logging_buildflags.h",
    "+base/debug/debugging_buildflags.h",
    "+build/build_config.h",
    "+build/buildflag.h",
    "+build/chromeos_buildflags.h",
    "+build/chromecast_buildflags.h",
    "+third_party/lss/linux_syscall_support.h",
]

specific_include_rules = {
  ".*_(perf|unit)test\.cc": [
    "+base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h",
    "+base/debug/proc_maps_linux.h",
    "+base/system/sys_info.h",
    "+base/test/gtest_util.h",
    "+base/timer/lap_timer.h",
    "+base/win/windows_version.h",
    "+testing/gmock/include/gmock/gmock.h",
    "+testing/gtest/include/gtest/gtest.h",
    "+testing/gtest/include/gtest/gtest_prod.h",
    "+testing/perf/perf_result_reporter.h",
  ],
}
