# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./android.star", "android")
load("./clang_all.star", "clang_all")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")
load("./config.star", "config")
load("./cros.star", "cros")

# TODO: b/323091468 - Propagate target android ABI and android SDK version
# from GN, and remove the hardcoded filegroups.
android_archs = [
    "aarch64-linux-android",
    "arm-linux-androideabi",
    "i686-linux-android",
    "riscv64-linux-android",
    "x86_64-linux-android",
]

android_versions = list(range(21, 35))

def __filegroups(ctx):
    fg = {
        # for precomputed subtrees
        "build/linux/debian_bullseye_amd64-sysroot/usr/include:include": {
            "type": "glob",
            "includes": ["*"],
            # need bits/stab.def, c++/*
        },
        "build/linux/debian_bullseye_amd64-sysroot/usr/lib:headers": {
            "type": "glob",
            "includes": ["*.h", "crtbegin.o"],
        },
        "build/linux/debian_bullseye_i386-sysroot/usr/include:include": {
            "type": "glob",
            "includes": ["*"],
            # need bits/stab.def, c++/*
        },
        "build/linux/debian_bullseye_i386-sysroot/usr/lib:headers": {
            "type": "glob",
            "includes": ["*.h", "crtbegin.o"],
        },
        "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include:include": {
            "type": "glob",
            "includes": ["*"],
            # can't use "*.h", because c++ headers have no extension.
        },
        "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include:include": {
            "type": "glob",
            "includes": ["*"],
        },
        "third_party/llvm-build/Release+Asserts/lib/clang:libs": {
            "type": "glob",
            "includes": ["*/lib/*/*", "*/lib/*", "*/share/*"],
        },
        "build/linux/debian_bullseye_amd64-sysroot/lib/x86_64-linux-gnu:libso": {
            "type": "glob",
            "includes": ["*.so*"],
        },
        "build/linux/debian_bullseye_amd64-sysroot/usr/lib/x86_64-linux-gnu:libs": {
            "type": "glob",
            "includes": ["*.o", "*.so*", "lib*.a"],
        },
        "build/linux/debian_bullseye_amd64-sysroot/usr/lib/gcc/x86_64-linux-gnu:libgcc": {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        },
    }
    if android.enabled(ctx):
        for arch in android_archs:
            for ver in android_versions:
                group = "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/%s/%d:link" % (arch, ver)
                fg[group] = {
                    "type": "glob",
                    "includes": ["*"],
                }

    fg.update(clang_all.filegroups(ctx))
    return fg

def __clang_compile_coverage(ctx, cmd):
    clang_command = clang_code_coverage_wrapper.run(ctx, list(cmd.args))
    ctx.actions.fix(args = clang_command)

__handlers = {
    "clang_compile_coverage": __clang_compile_coverage,
}

def __step_config(ctx, step_config):
    step_config["input_deps"].update({
        # sysroot headers for precomputed subtrees
        "build/linux/debian_bullseye_amd64-sysroot:headers": [
            "build/linux/debian_bullseye_amd64-sysroot/usr/include:include",
            "build/linux/debian_bullseye_amd64-sysroot/usr/lib:headers",
        ],
        "build/linux/debian_bullseye_i386-sysroot:headers": [
            "build/linux/debian_bullseye_i386-sysroot/usr/include:include",
            "build/linux/debian_bullseye_i386-sysroot/usr/lib:headers",
        ],
        "build/linux/debian_bullseye_amd64-sysroot:link": [
            "build/linux/debian_bullseye_amd64-sysroot/lib/x86_64-linux-gnu:libso",
            "build/linux/debian_bullseye_amd64-sysroot/lib64/ld-linux-x86-64.so.2",
            "build/linux/debian_bullseye_amd64-sysroot/usr/lib/gcc/x86_64-linux-gnu:libgcc",
            "build/linux/debian_bullseye_amd64-sysroot/usr/lib/x86_64-linux-gnu:libs",
            "third_party/llvm-build/Release+Asserts/bin/clang",
            "third_party/llvm-build/Release+Asserts/bin/clang++",
            "third_party/llvm-build/Release+Asserts/bin/ld.lld",
            "third_party/llvm-build/Release+Asserts/bin/lld",
            "third_party/llvm-build/Release+Asserts/bin/llvm-nm",
            "third_party/llvm-build/Release+Asserts/bin/llvm-readelf",
            "third_party/llvm-build/Release+Asserts/bin/llvm-readobj",
            # The following inputs are used for sanitizer builds.
            # It might be better to add them only for sanitizer builds if there is a performance issue.
            "third_party/llvm-build/Release+Asserts/lib/clang:libs",
        ],
        "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot:headers": [
            "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include:include",
            "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include:include",
        ],
    })
    step_config["input_deps"].update(clang_all.input_deps)
    step_config["rules"].extend([
        {
            "name": "clang/cxx",
            "action": "(.*_)?cxx",
            "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang++ ",
            "inputs": [
                "third_party/llvm-build/Release+Asserts/bin/clang++",
            ],
            "exclude_input_patterns": ["*.stamp"],
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "clang/cc",
            "action": "(.*_)?cc",
            "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang ",
            "inputs": [
                "third_party/llvm-build/Release+Asserts/bin/clang",
            ],
            "exclude_input_patterns": ["*.stamp"],
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "clang-coverage/cxx",
            "action": "(.*_)?cxx",
            "command_prefix": "\"python3\" ../../build/toolchain/clang_code_coverage_wrapper.py",
            "inputs": [
                "third_party/llvm-build/Release+Asserts/bin/clang++",
            ],
            "exclude_input_patterns": ["*.stamp"],
            "handler": "clang_compile_coverage",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "clang-coverage/cc",
            "action": "(.*_)?cc",
            "command_prefix": "\"python3\" ../../build/toolchain/clang_code_coverage_wrapper.py",
            "inputs": [
                "third_party/llvm-build/Release+Asserts/bin/clang",
            ],
            "exclude_input_patterns": ["*.stamp"],
            "handler": "clang_compile_coverage",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
    ])

    # TODO: b/316267242 - Enable remote links for Android and CrOS toolchain builds.
    if not android.enabled(ctx) and not (cros.custom_toolchain(ctx) or cros.custom_sysroot(ctx)):
        step_config["rules"].extend([
            {
                "name": "clang/alink/llvm-ar",
                "action": "(.*_)?alink",
                "inputs": [
                    # TODO: b/316267242 - Add inputs to GN config.
                    "third_party/llvm-build/Release+Asserts/bin/llvm-ar",
                ],
                "exclude_input_patterns": [
                    "*.cc",
                    "*.h",
                    "*.js",
                    "*.pak",
                    "*.py",
                    "*.stamp",
                ],
                "remote": config.get(ctx, "remote-library-link"),
                "platform_ref": "large",
                "accumulate": True,
            },
            {
                "name": "clang/solink/gcc_solink_wrapper",
                "action": "(.*_)?solink",
                "command_prefix": "\"python3\" \"../../build/toolchain/gcc_solink_wrapper.py\"",
                "inputs": [
                    # TODO: b/316267242 - Add inputs to GN config.
                    "build/toolchain/gcc_solink_wrapper.py",
                    "build/toolchain/whole_archive.py",
                    "build/toolchain/wrapper_utils.py",
                    "build/linux/debian_bullseye_amd64-sysroot:link",
                ],
                "exclude_input_patterns": [
                    "*.cc",
                    "*.h",
                    "*.js",
                    "*.pak",
                    "*.py",
                    "*.stamp",
                ],
                "remote": config.get(ctx, "remote-library-link"),
                "platform_ref": "large",
            },
            {
                "name": "clang/link/gcc_link_wrapper",
                "action": "(.*_)?link",
                "command_prefix": "\"python3\" \"../../build/toolchain/gcc_link_wrapper.py\"",
                "inputs": [
                    # TODO: b/316267242 - Add inputs to GN config.
                    "build/toolchain/gcc_link_wrapper.py",
                    "build/toolchain/whole_archive.py",
                    "build/toolchain/wrapper_utils.py",
                    "build/linux/debian_bullseye_amd64-sysroot:link",
                ],
                "exclude_input_patterns": [
                    "*.cc",
                    "*.h",
                    "*.js",
                    "*.pak",
                    "*.py",
                    "*.stamp",
                ],
                "remote": config.get(ctx, "remote-exec-link"),
                "platform_ref": "large",
            },
        ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
