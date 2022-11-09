# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# PartitionAlloc is planned to be extracted into a standalone library, and
# therefore dependencies need to be strictly controlled and minimized.

gclient_gn_args_file = 'partition_allocator/build/config/gclient_args.gni'

# Only these hosts are allowed for dependencies in this DEPS file.
# This is a subset of chromium/src/DEPS's allowed_hosts.
allowed_hosts = [
  'chromium.googlesource.com',
]

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {
  'partition_allocator/build':
      Var('chromium_git') + '/chromium/src/build.git',
  'partition_allocator/buildtools':
      Var('chromium_git') + '/chromium/src/buildtools.git',
  'partition_allocator/buildtools/clang_format/script':
      Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git',
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
  'partition_allocator/buildtools/third_party/libc++/trunk':
      Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxx.git',
  'partition_allocator/buildtools/third_party/libc++abi/trunk':
      Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libcxxabi.git',
  'partition_allocator/tools/clang':
      Var('chromium_git') + '/chromium/src/tools/clang.git',
}

hooks = [
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': [
        'python3',
        'partition_allocator/build/linux/sysroot_scripts/install-sysroot.py',
        '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': [
        'python3',
        'partition_allocator/build/linux/sysroot_scripts/install-sysroot.py',
        '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': [
        'python3',
        'partition_allocator/build/linux/sysroot_scripts/install-sysroot.py',
        '--arch=x86'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    'action': [
        'python3',
        'partition_allocator/build/linux/sysroot_scripts/install-sysroot.py',
        '--arch=mips'],
  },
  {
    'name': 'sysroot_mips64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips64',
    'action': [
        'python3',
        'partition_allocator/build/linux/sysroot_scripts/install-sysroot.py',
        '--arch=mips64el'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': [
        'python3',
        'partition_allocator/build/linux/sysroot_scripts/install-sysroot.py',
        '--arch=x64'],
  },
  {
    # Update the prebuilt clang toolchain.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python3', 'partition_allocator/tools/clang/scripts/update.py'],
  },
]

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
