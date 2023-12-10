# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for ChromeOS builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

def __filegroups(ctx):
    fg = {}
    if not "args.gn" in ctx.metadata:
        print("no args.gn")
        return fg
    gn_args = gn.args(ctx)
    if not "cros_target_cxx" in gn_args:
        print("no cros_target_cxx")
        return fg
    toolchain = gn_args.get("cros_target_cxx")
    toolchain = toolchain.strip('"')
    toolchain = ctx.fs.canonpath(toolchain)
    print("toolchain = %s" % toolchain)
    if toolchain:
        toolchain = path.dir(path.dir(toolchain))
        fg[toolchain + ":headers"] = {
            "type": "glob",
            "includes": ["*"],
        }
    if not "target_sysroot" in gn_args:
        print("no target_sysroot")
        return fg
    sysroot = gn_args.get("target_sysroot")
    sysroot = sysroot.strip('"')
    sysroot = ctx.fs.canonpath(sysroot)
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

def __cros_compiler(ctx, cmd):
    tool_inputs = cmd.tool_inputs
    for i, arg in enumerate(cmd.args):
        if arg.startswith("-fprofile-sample-use="):
            # profile data is in ninja input (direct or indirect),
            # but siso doesn't include ninja inputs for deps=gcc
            # (it would include lots of unnecessary inputs)
            # so just add profdata by checking command line flag.
            profdata = ctx.fs.canonpath(arg.removeprefix("-fprofile-sample-use="))
            tool_inputs.append(profdata)
    ctx.actions.fix(tool_inputs = tool_inputs)

__handlers = {
    "cros_compiler": __cros_compiler,
}

def __step_config(ctx, step_config):
    if not "args.gn" in ctx.metadata:
        return step_config
    gn_args = gn.args(ctx)
    if "cros_target_cxx" in gn_args:
        toolchain = gn_args.get("cros_target_cxx")
        if toolchain:
            step_config["rules"].extend([
                {
                    "name": "clang-cros/cxx",
                    "action": "(.*_)?cxx",
                    "command_prefix": "../../build/cros_cache/chrome-sdk/",
                    "remote": True,
                    "handler": "cros_compiler",
                    "canonicalize_dir": True,
                    "timeout": "5m",
                },
                {
                    "name": "clang-cros/cc",
                    "action": "(.*_)?cc",
                    "command_prefix": "../../build/cros_cache/chrome-sdk/",
                    "remote": True,
                    "handler": "cros_compiler",
                    "canonicalize_dir": True,
                    "timeout": "5m",
                },
            ])
    if "target_sysroot" in gn_args:
        sysroot = gn_args.get("target_sysroot")
        if sysroot:
            sysroot = sysroot.strip('"')
            sysroot = ctx.fs.canonpath(sysroot)
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
    filegroups = __filegroups,
    handlers = __handlers,
    step_config = __step_config,
)
