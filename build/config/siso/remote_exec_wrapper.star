# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for remote exec wrapper."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")

__filegroups = {}
__handlers = {}

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.parse_args(ctx.metadata["args.gn"])
        if gn_args.get("use_goma") == "true":
            return True
        if gn_args.get("use_remoteexec") == "true":
            return True
    return False

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "clang/cxx",
            "action": "(.*_)?cxx",
            "use_remote_exec_wrapper": True,
        },
        {
            "name": "clang/cc",
            "action": "(.*_)?cc",
            "use_remote_exec_wrapper": True,
        },
        {
            "name": "clang/objcxx",
            "action": "(.*_)?objcxx",
            "use_remote_exec_wrapper": True,
        },
        {
            "name": "clang/objc",
            "action": "(.*_)?objc",
            "use_remote_exec_wrapper": True,
        },
        {
            "name": "action_remote",
            "command_prefix": "python3 ../../build/util/action_remote.py",
            "use_remote_exec_wrapper": True,
        },
    ])
    return step_config

remote_exec_wrapper = module(
    "remote_exec_wrapper",
    enabled = __enabled,
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
