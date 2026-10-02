# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for macOS/iOS."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./apple.star", "apple")
load("./clang_mac.star", "clang")
load("./config.star", "config")
load("./platform.star", "platform")

def __filegroups(ctx):
    fg = {}
    fg.update(clang.filegroups(ctx))
    return fg

def __download_from_google_storage(ctx, cmd):
    # download_from_google_storage will replace output dir.
    ctx.actions.fix(reconcile_outputdirs = [path.dir(cmd.outputs[0])])

__handlers = {
    "download_from_google_storage": __download_from_google_storage,
}
__handlers.update(apple.handlers)
__handlers.update(clang.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = clang.step_config(ctx, step_config)
    step_config = apple.step_config(ctx, step_config)
    step_config["rules"].extend([
        {
            "name": "download_from_google_storage",
            "command_prefix": platform.python_bin + " ../../third_party/depot_tools/download_from_google_storage.py",
            "handler": "download_from_google_storage",
        },
    ])
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
