# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""gn_logs module to access gn_logs data."""

load("@builtin//struct.star", "module")

def __read(ctx):
    fname = ctx.fs.canonpath("./gn_logs.txt")
    if not ctx.fs.exists(fname):
        return {}
    gn_logs = ctx.fs.read(fname)
    vars = {}
    for line in str(gn_logs).splitlines():
        if line.startswith("#"):
            continue
        if not "=" in line:
            continue
        kv = line.split("=", 1)
        vars[kv[0].strip()] = kv[1].strip()
    return vars

gn_logs = module(
    "gn_logs",
    read = __read,
)
