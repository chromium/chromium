# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for ChromeOS builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./config.star", "config")

def __cros_sysroot(ctx):
    """Returns all CrOS specific sysroot GN args."""
    if not "args.gn" in ctx.metadata:
        print("no args.gn")
        return
    gn_args = gn.args(ctx)
    if gn_args.get("target_os") != '"chromeos"':
        return
    arg = "target_sysroot"
    if arg not in gn_args:
        return
    fp = ctx.fs.canonpath(gn_args.get(arg).strip('"'))
    if "chrome-sdk" not in fp:
        return
    return fp

def __filegroups(ctx):
    fg = {}
    sysroot = __cros_sysroot(ctx)
    if sysroot:
        print("sysroot = %s" % sysroot)
        fg[path.join(sysroot, "usr/include") + ":include"] = {
            "type": "glob",
            "includes": ["*"],
            # needs bits/stab.def, c++/*
        }
        fg[path.join(sysroot, "usr/lib") + ":headers"] = {
            "type": "glob",
            "includes": ["*.h", "crtbegin.o"],
        }
        fg[path.join(sysroot, "usr/lib64") + ":headers"] = {
            "type": "glob",
            "includes": ["*.h"],
        }
        fg[sysroot + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
    print(fg)
    return fg

def __step_config(ctx, step_config):
    sysroot = __cros_sysroot(ctx)
    if not sysroot:
        return step_config

    step_config["input_deps"].update({
        sysroot + ":headers": [
            path.join(sysroot, "usr/include") + ":include",
            path.join(sysroot, "usr/lib") + ":headers",
            path.join(sysroot, "usr/lib64") + ":headers",
        ],
        sysroot + ":link": [
            sysroot + ":libs",
        ],
    })
    return step_config

cros = module(
    "cros",
    filegroups = __filegroups,
    handlers = {},
    step_config = __step_config,
)
