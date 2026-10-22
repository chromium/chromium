# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Windows."""

load("@builtin//struct.star", "module")
load("./clang_windows.star", "clang")
load("./config.star", "config")
load("./platform.star", "platform")

def __filegroups(ctx):
    fg = {}
    fg.update(clang.filegroups(ctx))
    return fg

__handlers = {}
__handlers.update(clang.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = clang.step_config(ctx, step_config)
    step_config["rules"].extend([
        {
            "name": "perfetto/touch_file",
            "command_prefix": platform.python_bin + " ../../third_party/perfetto/tools/touch_file.py",
            "remote": False,
            "replace": True,
            # TODO(https://crbug.com/576114677): touch_file.py produces an empty
            # stamp file consumed by gen_amalgamated_sql.py and
            # gen_tp_table_headers.py without a depfile or transitive input list.
            "restat_content": False,
        },
    ])
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
