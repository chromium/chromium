# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Fuchsia builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./gn_logs.star", "gn_logs")

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") == '"fuchsia"':
            return True
    return False

def __filegroups(ctx):
    gn_logs_data = gn_logs.read(ctx)
    fuchsia_arch_root = gn_logs_data.get("fuchsia_arch_root")
    fuchsia_legacy_arch_root = gn_logs_data.get("fuchsia_legacy_arch_root")
    if not fuchsia_arch_root or not fuchsia_legacy_arch_root:
        print("could not find fuchsia_arch_root or fuchsia_legacy_arch_root from gn_logs.txt")
        return {}
    fg = {
        # The legacy directory is still used. But, will be removed soon.
        fuchsia_legacy_arch_root + "/lib:libs": {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        },
        fuchsia_arch_root + "/sysroot:headers": {
            "type": "glob",
            "includes": ["*.h", "*.inc", "*.o"],
        },
        fuchsia_arch_root + "/sysroot:link": {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        },
        fuchsia_arch_root + "/lib:link": {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        },
    }
    return fg

fuchsia = module(
    "fuchsia",
    enabled = __enabled,
    filegroups = __filegroups,
)
