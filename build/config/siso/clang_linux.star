# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_all.star", "clang_all")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")

__filegroups = {
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
}
__filegroups.update(clang_all.filegroups)

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
            "handler": "clang_compile_coverage",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
    ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
