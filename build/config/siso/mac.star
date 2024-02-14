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

__handlers = {}
__handlers.update(clang.handlers)
__handlers.update(typescript.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = clang.step_config(ctx, step_config)
    step_config = typescript.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
