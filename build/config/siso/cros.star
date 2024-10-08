# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for ChromeOS builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

def __custom_toolchain(ctx):
    if not "args.gn" in ctx.metadata:
        print("no args.gn")
        return None
    gn_args = gn.args(ctx)
    if gn_args.get("target_os") != '"chromeos"':
        return None
    if not "cros_target_cxx" in gn_args:
        print("no cros_target_cxx")
        return None
    cros_target_cxx = gn_args.get("cros_target_cxx")
    cros_target_cxx = cros_target_cxx.strip('"')
    cros_target_cxx = ctx.fs.canonpath(cros_target_cxx)
    toolchain = path.dir(path.dir(cros_target_cxx))
    if not toolchain:
        fail("failed to detect cros custom toolchain. cros_target_cxx = %s" % gn_args.get("cros_target_cxx"))
    return toolchain

def __custom_sysroot(ctx):
    if not "args.gn" in ctx.metadata:
        print("no args.gn")
        return None
    gn_args = gn.args(ctx)
    if gn_args.get("target_os") != '"chromeos"':
        return None
    if not "target_sysroot" in gn_args:
        print("no target_sysroot")
        return None
    sysroot = gn_args.get("target_sysroot")
    sysroot = sysroot.strip('"')
    sysroot = ctx.fs.canonpath(sysroot)
    if not sysroot:
        fail("failed to detect cros custom sysroot. target_sysroot = %s" % gn_args.get("target_sysroot"))
    return sysroot

def __filegroups(ctx):
    fg = {}
    toolchain = __custom_toolchain(ctx)
    print("toolchain = %s" % toolchain)
    if toolchain:
        fg[toolchain + ":headers"] = {
            "type": "glob",
            "includes": ["*"],
        }
    sysroot = __custom_sysroot(ctx)
    print("sysroot = %s" % sysroot)
    if sysroot:
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
    print(fg)
    return fg

def __step_config(ctx, step_config):
    if __custom_toolchain(ctx):
        step_config["rules"].extend([
            {
                "name": "clang-cros/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "../../build/cros_cache/chrome-sdk/",
                "remote": True,
                "canonicalize_dir": True,
                "timeout": "5m",
            },
            {
                "name": "clang-cros/cc",
                "action": "(.*_)?cc",
                "command_prefix": "../../build/cros_cache/chrome-sdk/",
                "remote": True,
                "canonicalize_dir": True,
                "timeout": "5m",
            },
        ])
    sysroot = __custom_sysroot(ctx)
    if sysroot:
        step_config["input_deps"].update({
            sysroot + ":headers": [
                path.join(sysroot, "usr/include") + ":include",
                path.join(sysroot, "usr/lib") + ":headers",
                path.join(sysroot, "usr/lib64") + ":headers",
            ],
        })
    return step_config

cros = module(
    "cros",
    custom_toolchain = __custom_toolchain,
    custom_sysroot = __custom_sysroot,
    filegroups = __filegroups,
    handlers = {},
    step_config = __step_config,
)
