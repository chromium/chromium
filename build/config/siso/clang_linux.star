# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//struct.star", "module")
load("./android.star", "android")
load("./clang_exception.star", "clang_exception")
load("./clang_unix.star", "clang_unix")
load("./fuchsia.star", "fuchsia")

def __filegroups(ctx):
    fg = {
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
    if android.enabled(ctx):
        fg.update(android.filegroups(ctx))
    if fuchsia.enabled(ctx):
        fg.update(fuchsia.filegroups(ctx))
    fg.update(clang_unix.filegroups(ctx))
    return fg

__handlers = clang_unix.handlers

def __step_config(ctx, step_config):
    step_config["input_deps"].update({
        "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot:headers": [
            "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include:include",
            "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include:include",
        ],
    })
    step_config["input_deps"].update(clang_unix.input_deps(ctx))

    step_config["rules"].extend(clang_unix.rules(ctx))
    step_config = clang_exception.step_config(ctx, step_config)
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
