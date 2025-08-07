# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./android.star", "android")
load("./clang_all.star", "clang_all")
load("./clang_exception.star", "clang_exception")
load("./clang_unix.star", "clang_unix")
load("./gn_logs.star", "gn_logs")
load("./fuchsia.star", "fuchsia")
load("./win_sdk.star", "win_sdk")

target_cpus = ["amd64", "i386", "arm64", "armhf"]

def __filegroups(ctx):
    gn_logs_data = gn_logs.read(ctx)

    # source_root is set only for CrOS's chroot builds that use
    # rbe_exec_root="/".
    root = gn_logs_data.get("source_root", "")
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
    }

    def __add_sysroot_for_target_cpu(fg, cpu):
        fg.update({
            # for precomputed subtrees
            path.join(root, "build/linux/debian_bullseye_%s-sysroot/usr/include" % cpu) + ":include": {
                "type": "glob",
                "includes": ["*"],
                # need bits/stab.def, c++/*
            },
            path.join(root, "build/linux/debian_bullseye_%s-sysroot/usr/lib" % cpu) + ":headers": {
                "type": "glob",
                "includes": ["*.h", "crtbegin.o"],
            },
            path.join(root, "build/linux/debian_bullseye_%s-sysroot" % cpu) + ":libs": {
                "type": "glob",
                "includes": ["*.so*", "*.o", "*.a"],
                "excludes": [
                    "usr/lib/python*/*/*",
                    "systemd/*/*",
                    "usr/libexec/*/*",
                ],
            },
        })
        return fg

    for cpu in target_cpus:
        fg = __add_sysroot_for_target_cpu(fg, cpu)
    if android.enabled(ctx):
        fg.update(android.filegroups(ctx))
    if fuchsia.enabled(ctx):
        fg.update(fuchsia.filegroups(ctx))
    fg.update(clang_all.filegroups(ctx))
    return fg

__handlers = {}
__handlers.update(clang_unix.handlers)
__handlers.update(clang_all.handlers)

def __step_config(ctx, step_config):
    gn_logs_data = gn_logs.read(ctx)

    # source_root is set only for CrOS's chroot builds that use
    # rbe_exec_root="/".
    root = gn_logs_data.get("source_root", "")

    step_config["input_deps"].update({
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

    def __add_sysroot_for_target_cpu(step_config, cpu):
        step_config["input_deps"].update({
            # sysroot headers for precomputed subtrees
            path.join(root, "build/linux/debian_bullseye_%s-sysroot" % cpu) + ":headers": [
                path.join(root, "build/linux/debian_bullseye_%s-sysroot/usr/include" % cpu) + ":include",
                path.join(root, "build/linux/debian_bullseye_%s-sysroot/usr/lib" % cpu) + ":headers",
            ],
            path.join(root, "build/linux/debian_bullseye_%s-sysroot" % cpu) + ":link": [
                path.join(root, "build/linux/debian_bullseye_%s-sysroot" % cpu) + ":libs",
                path.join(root, "third_party/llvm-build/Release+Asserts/bin") + ":llddeps",
                # The following inputs are used for sanitizer builds.
                # It might be better to add them only for sanitizer builds if there is a performance issue.
                path.join(root, "third_party/llvm-build/Release+Asserts/lib/clang") + ":libs",
            ],
        })

    for cpu in target_cpus:
        __add_sysroot_for_target_cpu(step_config, cpu)
    step_config["input_deps"].update(clang_all.input_deps(ctx))

    step_config["rules"].extend(clang_unix.rules(ctx))
    if win_sdk.enabled(ctx):
        win_sdk.step_config(ctx, step_config)
    step_config = clang_exception.step_config(ctx, step_config)
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
