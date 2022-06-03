# PartitionAlloc is planned to be extracted into a standalone library, and
# therefore dependencies need to be strictly controlled and minimized.

noparent = True

include_rules = [
    "+base/mac/foundation_util.h",
    "+build/build_config.h",
    "+build/buildflag.h",
    "+third_party/lss/linux_syscall_support.h",
]

# These are dependencies included only from tests, which means we can tackle
# them at a later point of time.
#
# CAUTION! This list can't be verified automatically and thus can easily get out
# of date.
include_rules += [
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
]
