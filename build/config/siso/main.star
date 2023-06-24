# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration main entry."""

load("@builtin//encoding.star", "json")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./linux.star", chromium_linux = "chromium")
load("./mac.star", chromium_mac = "chromium")
load("./simple.star", "simple")
load("./windows.star", chromium_windows = "chromium")

def init(ctx):
    print("runtime: os:%s arch:%s run:%d" % (
        runtime.os,
        runtime.arch,
        runtime.num_cpu,
    ))
    host = {
        "linux": chromium_linux,
        "darwin": chromium_mac,
        "windows": chromium_windows,
    }[runtime.os]
    step_config = {
        "platforms": {},
        "input_deps": {},
        "rules": [],
    }
    step_config = host.step_config(ctx, step_config)
    step_config = simple.step_config(ctx, step_config)

    filegroups = {}
    filegroups.update(host.filegroups)
    filegroups.update(simple.filegroups)

    handlers = {}
    handlers.update(host.handlers)
    handlers.update(simple.handlers)

    return module(
        "config",
        step_config = json.encode(step_config),
        filegroups = filegroups,
        handlers = handlers,
    )
