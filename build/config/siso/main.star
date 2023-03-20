# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@builtin//encoding.star", "json")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./linux.star", chromium_linux = "chromium")

def init(ctx):
    print("runtime: os:%s arch:%s run:%d" % (
        runtime.os,
        runtime.arch,
        runtime.num_cpu,
    ))
    host = {
        "linux": chromium_linux,
        # add mac, windows
    }[runtime.os]
    return module(
        "config",
        step_config = json.encode(host.step_config(ctx)),
        filegroups = host.filegroups,
        handlers = host.handlers,
    )
