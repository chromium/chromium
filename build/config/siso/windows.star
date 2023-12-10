# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Windows."""

load("@builtin//struct.star", "module")
load("./clang_windows.star", "clang")
load("./config.star", "config")
load("./reproxy.star", "reproxy")

def __filegroups(ctx):
    fg = {}
    fg.update(clang.filegroups(ctx))
    return fg

__handlers = {}
__handlers.update(clang.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = clang.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
