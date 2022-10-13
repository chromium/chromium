# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# PartitionAlloc is planned to be extracted into a standalone library, and
# therefore dependencies need to be strictly controlled and minimized.

# Only these hosts are allowed for dependencies in this DEPS file.
# This is a subset of chromium/src/DEPS's allowed_hosts.
allowed_hosts = [
  'chromium.googlesource.com',
]

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {
  'partition_allocator/buildtools/clang_format/script':
      Var('chromium_git') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git',
  'partition_allocator/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'latest',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'partition_allocator/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'latest',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'partition_allocator/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'latest',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
}

noparent = True

include_rules = [
    "+build/build_config.h",
    "+build/buildflag.h",
    "+third_party/lss/linux_syscall_support.h",
]

specific_include_rules = {
  ".*_(perf|unit)test\.cc$": [
    "+base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h",
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
  "gtest_prod_util\.h$": [
    "+testing/gtest/include/gtest/gtest_prod.h",
  ],
}
