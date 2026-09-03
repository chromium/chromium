# -*- bazel-starlark -*-
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Linux sysroot."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./gn_logs.star", "gn_logs")

target_cpus = ["amd64", "i386", "arm64", "armhf"]

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") == '"linux"':
            return True
    return runtime.os == "linux"

def __filegroups(ctx):
    gn_logs_data = gn_logs.read(ctx)
    root = gn_logs_data.get("source_root", "")
    fg = {}

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
    return fg

def __input_deps(ctx):
    gn_logs_data = gn_logs.read(ctx)
    root = gn_logs_data.get("source_root", "")
    inputs = {}

    def __add_sysroot_for_target_cpu(inputs, cpu):
        inputs.update({
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
        return inputs

    for cpu in target_cpus:
        inputs = __add_sysroot_for_target_cpu(inputs, cpu)
    return inputs

linux_sysroot = module(
    "linux_sysroot",
    enabled = __enabled,
    filegroups = __filegroups,
    input_deps = __input_deps,
)
