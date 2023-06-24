# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

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
    "buildtools/third_party/libc++/trunk/include:headers": {
        "type": "glob",
        "includes": ["*"],
        # can't use "*.h", because c++ headers have no extension.
    },
    "buildtools/third_party/libc++abi/trunk/include:headers": {
        "type": "glob",
        "includes": ["*.h"],
    },
    "third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include:include": {
        "type": "glob",
        "includes": ["*"],
        # can't use "*.h", because c++ headers have no extension.
    },
    "third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include:include": {
        "type": "glob",
        "includes": ["*"],
    },

    # toolchain root
    # :headers for compiling
    "third_party/llvm-build/Release+Asserts:headers": {
        "type": "glob",
        "includes": [
            "*.h",
            "bin/clang",
            "bin/clang++",
        ],
    },
}

def __clang_compile_coverage(ctx, cmd):
    # TODO(b/278225415): add better support for coverage build.
    # The instrument file contains the list of files affected by a patch.
    # Including this file to remote action input prevents cache hits.
    inputs = []
    deps_args = []
    for i, arg in enumerate(cmd.args):
        if i == 0:
            continue
        if arg == "../../build/toolchain/clang_code_coverage_wrapper.py":
            continue
        if arg.startswith("--files-to-instrument="):
            inputs.append(ctx.fs.canonpath(arg.removeprefix("--files-to-instrument=")))
            continue
        if len(deps_args) == 0 and path.base(arg).find("clang") >= 0:
            deps_args.append(arg)
            continue
        if deps_args:
            if arg in ["-MD", "-MMD", "-c"]:
                continue
            if arg.startswith("-MF") or arg.startswith("-o"):
                continue
            if i > 1 and cmd.args[i - 1] in ["-MF", "-o"]:
                continue
            deps_args.append(arg)
    if deps_args:
        deps_args.append("-M")
    ctx.actions.fix(
        tool_inputs = cmd.tool_inputs + inputs,
        deps_args = deps_args,
    )

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
        "third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/sysroot:headers": [
            "third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include:include",
            "third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include:include",
        ],

        # need this because we use
        # buildtools/third_party/libc++/trunk/include:headers,
        # but scandeps doesn't scan `__config` file, which uses
        # `#include <__config_site>`
        "buildtools/third_party/libc++": [
            "buildtools/third_party/libc++/__config_site",
        ],
    })
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
                "build/toolchain/clang_code_coverage_wrapper.py",
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
                "build/toolchain/clang_code_coverage_wrapper.py",
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
