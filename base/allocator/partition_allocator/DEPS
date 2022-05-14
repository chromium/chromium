# PartitionAlloc is planned to be extracted into a standalone library, and
# therefore dependencies need to be strictly controlled and minimized.

noparent = True

include_rules = [
    "+base/allocator/buildflags.h",
    "+base/base_export.h",
    "+base/check.h",
    "+base/check_op.h",
    "+base/compiler_specific.h",
    "+base/dcheck_is_on.h",
    "+base/debug/proc_maps_linux.h",
    "+base/immediate_crash.h",
    "+base/lazy_instance.h",
    "+base/logging_buildflags.h",
    "+base/mac/foundation_util.h",
    "+base/mac/mac_util.h",
    "+base/mac/scoped_cftyperef.h",
    "+base/process/memory.h",
    "+base/thread_annotations.h",
    "+base/threading/platform_thread.h",
    "+base/time/time.h",
    "+base/win/windows_types.h",
    "+build/build_config.h",
    "+build/buildflag.h",
    "+build/chromecast_buildflags.h",
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
    "+base/strings/stringprintf.h",
    "+base/system/sys_info.h",
    "+base/test/gtest_util.h",
    "+base/time/time_override.h",
    "+base/timer/lap_timer.h",
    "+base/win/windows_version.h",
    "+testing/gmock/include/gmock/gmock.h",
    "+testing/gtest/include/gtest/gtest.h",
    "+testing/gtest/include/gtest/gtest_prod.h",
    "+testing/perf/perf_result_reporter.h",
]
