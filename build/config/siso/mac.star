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

def __codesign(ctx, cmd):
    # codesign.py uses the last arguments as bundle.path
    # and it would remove existing files under bundle.path,
    # but siso could not detect such removal, so would cause failure
    # in subsequent steps. https://crbug.com/372628498
    # To capture such removal, specify the bundle path as output,
    # so hashfs.RetrieveUpdateEntriesFromLocal can detect them.
    bundle_path = ctx.fs.canonpath(cmd.args[-1])
    ctx.actions.fix(reconcile_outputdirs = [bundle_path])

__handlers = {
    "codesign": __codesign,
}
__handlers.update(clang.handlers)
__handlers.update(typescript.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = clang.step_config(ctx, step_config)
    step_config = typescript.step_config(ctx, step_config)
    step_config["rules"].extend([
        {
            "name": "codesign",
            "command_prefix": "python3 ../../build/config/apple/codesign.py ",
            "handler": "codesign",
        },
    ])
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
