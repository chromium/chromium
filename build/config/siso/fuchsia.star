# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Fuchsia builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")

# TODO: crbug.com/323091468 - Propagate the target fuchsia arch and API version
# from GN, and remove the hardcoded filegroups.
fuchsia_archs = [
    "arm64",
    "riscv64",
    "x64",
]

fuchsia_versions = ["16", "20", "22", "23", "24", "NEXT"]

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") == '"fuchsia"':
            return True
    return False

def __filegroups(ctx):
    fg = {}
    for arch in fuchsia_archs:
        libpath = "third_party/fuchsia-sdk/sdk/arch/%s/lib" % arch
        fg[libpath + ":libs"] = {
            "type": "glob",
            "includes": ["*.o", "*.a", "*.so"],
        }
        for ver in fuchsia_versions:
            sysroot = "third_party/fuchsia-sdk/sdk/obj/%s-api-%s/sysroot" % (arch, ver)
            libpath = "third_party/fuchsia-sdk/sdk/obj/%s-api-%s/lib" % (arch, ver)
            fg[sysroot + ":headers"] = {
                "type": "glob",
                "includes": ["*.h", "*.inc", "*.o"],
            }
            fg[sysroot + ":link"] = {
                "type": "glob",
                "includes": ["*.o", "*.a", "*.so"],
            }
            fg[libpath + ":link"] = {
                "type": "glob",
                "includes": ["*.o", "*.a", "*.so"],
            }
    return fg

fuchsia = module(
    "fuchsia",
    enabled = __enabled,
    filegroups = __filegroups,
)
