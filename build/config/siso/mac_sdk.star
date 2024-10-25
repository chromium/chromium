# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for mac sdk."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./gn_logs.star", "gn_logs")

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") in ('"mac"', '"ios"'):
            return True
    return runtime.os == "darwin"

def __filegroups(ctx):
    sdk_includes = [
        "*.framework",
        "*.h",
        "*.json",
        "*.modulemap",
        "Current",
        "Frameworks",
        "Headers",
        "Modules",
        "crt*.o",
        "usr/include/c++/v1/*",
        "usr/include/c++/v1/*/*",
    ]
    fg = {
        "build/mac_files/xcode_binaries/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk:headers": {
            "type": "glob",
            "includes": sdk_includes,
        },
    }
    if gn.args(ctx).get("use_remoteexec") == "true":
        # precompute subtree for sysroot/frameworks for siso scandeps,
        # which is not complex enough to handle C preprocessor tricks
        # and need system include dirs when using deps log of -MMD.
        # need to add new entries when new version is used.
        #
        # if use_remoteexec is not true, these dirs are not under exec root
        # and failed to create filegroup for such dirs. crbug.com/352216756
        gn_logs_data = gn_logs.read(ctx)
        if gn_logs_data.get("mac_sdk_path"):
            fg[ctx.fs.canonpath("./" + gn_logs_data.get("mac_sdk_path")) + ":headers"] = {
                "type": "glob",
                "includes": sdk_includes,
            }
        if gn_logs_data.get("ios_sdk_path"):
            fg[ctx.fs.canonpath("./" + gn_logs_data.get("ios_sdk_path")) + ":headers"] = {
                "type": "glob",
                "includes": sdk_includes,
            }

    fg[ctx.fs.canonpath("./sdk/xcode_links/iPhoneSimulator.platform/Developer/Library/Frameworks") + ":headers"] = {
        "type": "glob",
        "includes": sdk_includes,
    }
    return fg

mac_sdk = module(
    "mac_sdk",
    enabled = __enabled,
    filegroups = __filegroups,
)
