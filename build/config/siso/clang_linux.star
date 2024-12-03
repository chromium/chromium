# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//struct.star", "module")
load("./android.star", "android")
load("./clang_all.star", "clang_all")
load("./clang_unix.star", "clang_unix")
load("./fuchsia.star", "fuchsia")
load("./win_sdk.star", "win_sdk")

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
        "build/linux/debian_bullseye_arm64-sysroot/usr/include:include": {
            "type": "glob",
            "includes": ["*"],
            # need bits/stab.def, c++/*
        },
        "build/linux/debian_bullseye_arm64-sysroot/usr/lib:headers": {
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
        "build/linux/debian_bullseye_armhf-sysroot/usr/include:include": {
            "type": "glob",
            "includes": ["*"],
            # need bits/stab.def, c++/*
        },
        "build/linux/debian_bullseye_armhf-sysroot/usr/lib:headers": {
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
        "third_party/llvm-build/Release+Asserts/bin:llddeps": {
            "type": "glob",
            "includes": [
                "clang*",
                "ld.lld",
                "ld64.lld",
                "lld",
                "llvm-nm",
                "llvm-objcopy",
                "llvm-objdump",
                "llvm-otool",
                "llvm-readelf",
                "llvm-readobj",
                "llvm-strip",
            ],
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
        "build/linux/debian_bullseye_i386-sysroot/lib:libso": {
            "type": "glob",
            "includes": ["*.so*"],
        },
        "build/linux/debian_bullseye_i386-sysroot/usr/lib/i386-linux-gnu:libs": {
            "type": "glob",
            "includes": ["*.o", "*.so*", "lib*.a"],
        },
        "build/linux/debian_bullseye_i386-sysroot/usr/lib/gcc/i686-linux-gnu:libgcc": {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        },
        "build/linux/debian_bullseye_armhf-sysroot/lib:libso": {
            "type": "glob",
            "includes": ["*.so*"],
        },
        "build/linux/debian_bullseye_armhf-sysroot/usr/lib/arm-linux-gnueabihf:libs": {
            "type": "glob",
            "includes": ["*.o", "*.so*", "lib*.a"],
        },
        "build/linux/debian_bullseye_armhf-sysroot/usr/lib/gcc/arm-linux-gnueabihf:libgcc": {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        },
    }
    if android.enabled(ctx):
        fg.update(android.filegroups(ctx))
    if fuchsia.enabled(ctx):
        fg.update(fuchsia.filegroups(ctx))
    fg.update(clang_all.filegroups(ctx))
    return fg

__handlers = {}
__handlers.update(clang_unix.handlers)

def __step_config(ctx, step_config):
    step_config["input_deps"].update({
        # sysroot headers for precomputed subtrees
        "build/linux/debian_bullseye_amd64-sysroot:headers": [
            "build/linux/debian_bullseye_amd64-sysroot/usr/include:include",
            "build/linux/debian_bullseye_amd64-sysroot/usr/lib:headers",
        ],
        "build/linux/debian_bullseye_arm64-sysroot:headers": [
            "build/linux/debian_bullseye_arm64-sysroot/usr/include:include",
            "build/linux/debian_bullseye_arm64-sysroot/usr/lib:headers",
        ],
        "build/linux/debian_bullseye_i386-sysroot:headers": [
            "build/linux/debian_bullseye_i386-sysroot/usr/include:include",
            "build/linux/debian_bullseye_i386-sysroot/usr/lib:headers",
        ],
        "build/linux/debian_bullseye_armhf-sysroot:headers": [
            "build/linux/debian_bullseye_armhf-sysroot/usr/include:include",
            "build/linux/debian_bullseye_armhf-sysroot/usr/lib:headers",
        ],
        "build/linux/debian_bullseye_amd64-sysroot:link": [
            "build/linux/debian_bullseye_amd64-sysroot/lib/x86_64-linux-gnu:libso",
            "build/linux/debian_bullseye_amd64-sysroot/lib64/ld-linux-x86-64.so.2",
            "build/linux/debian_bullseye_amd64-sysroot/usr/lib/gcc/x86_64-linux-gnu:libgcc",
            "build/linux/debian_bullseye_amd64-sysroot/usr/lib/x86_64-linux-gnu:libs",
            "third_party/llvm-build/Release+Asserts/bin:llddeps",
            # The following inputs are used for sanitizer builds.
            # It might be better to add them only for sanitizer builds if there is a performance issue.
            "third_party/llvm-build/Release+Asserts/lib/clang:libs",
        ],
        "build/linux/debian_bullseye_i386-sysroot:link": [
            "build/linux/debian_bullseye_i386-sysroot/lib:libso",
            "build/linux/debian_bullseye_i386-sysroot/usr/lib/gcc/i686-linux-gnu:libgcc",
            "build/linux/debian_bullseye_i386-sysroot/usr/lib/i386-linux-gnu:libs",
            "third_party/llvm-build/Release+Asserts/bin:llddeps",
        ],
        "build/linux/debian_bullseye_armhf-sysroot:link": [
            "build/linux/debian_bullseye_armhf-sysroot/lib:libso",
            "build/linux/debian_bullseye_armhf-sysroot/usr/lib/gcc/arm-linux-gnueabihf:libgcc",
            "build/linux/debian_bullseye_armhf-sysroot/usr/lib/arm-linux-gnueabihf:libs",
            "third_party/llvm-build/Release+Asserts/bin:llddeps",
        ],
        "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot:headers": [
            "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include:include",
            "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include:include",
        ],
        "third_party/llvm-build/Release+Asserts/bin/clang++:link": [
            "third_party/llvm-build/Release+Asserts/bin:llddeps",
        ],
        "third_party/llvm-build/Release+Asserts:link": [
            "third_party/llvm-build/Release+Asserts/bin:llddeps",
            "third_party/llvm-build/Release+Asserts/lib/clang:libs",
        ],
    })
    step_config["input_deps"].update(clang_all.input_deps)

    step_config["rules"].extend(clang_unix.rules(ctx))
    if win_sdk.enabled(ctx):
        win_sdk.step_config(ctx, step_config)
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
