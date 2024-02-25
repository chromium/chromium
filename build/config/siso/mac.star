# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for macOS."""

load("@builtin//struct.star", "module")
load("./clang_mac.star", "clang")
load("./config.star", "config")
load("./typescript_unix.star", "typescript")

def __filegroups(ctx):
    fg = {}
    fg.update(clang.filegroups(ctx))
    fg.update(typescript.filegroups(ctx))
    return fg

# to reduce unnecessary local process and
# unnecessary digest calculation of output file.
def __copy_bundle_data(ctx, cmd):
    input = cmd.inputs[0]
    out = cmd.outputs[0]
    ctx.actions.copy(input, out, recursive = ctx.fs.is_dir(input))
    ctx.actions.exit(exit_status = 0)

__handlers = {
    "copy_bundle_data": __copy_bundle_data,
}
__handlers.update(clang.handlers)
__handlers.update(typescript.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config["rules"].extend([
        {
            "name": "mac/copy_bundle_data",
            "action": "(.*)?copy_bundle_data",
            "handler": "copy_bundle_data",
        },
    ])
    step_config = clang.step_config(ctx, step_config)
    step_config = typescript.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
