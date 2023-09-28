# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for simple steps."""

load("@builtin//struct.star", "module")

def __copy(ctx, cmd):
    input = cmd.inputs[0]
    out = cmd.outputs[0]
    ctx.actions.copy(input, out, recursive = ctx.fs.is_dir(input))
    ctx.actions.exit(exit_status = 0)

def __stamp(ctx, cmd):
    if len(cmd.outputs) > 1:
        # run touch command as is?
        # iOS build stamp after swiftc.py would try to touch
        # dir and non-exist-in-hashfs file?
        # TODO(b/300385880): fix this workaround.
        return

    # don't truncate if file exists.
    out = cmd.outputs[0]
    if ctx.fs.exists(out):
        ctx.actions.write(out, ctx.fs.read(out))
    else:
        ctx.actions.write(out)
    ctx.actions.exit(exit_status = 0)

__handlers = {
    "copy": __copy,
    "stamp": __stamp,
}

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "simple/copy",
            "action": "(.*_)?copy",
            "handler": "copy",
        },
        {
            "name": "simple/stamp",
            "action": "(.*_)?stamp",
            "handler": "stamp",
            "replace": True,
        },
    ])
    return step_config

simple = module(
    "simple",
    step_config = __step_config,
    filegroups = {},
    handlers = __handlers,
)
