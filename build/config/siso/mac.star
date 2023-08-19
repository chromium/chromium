# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for macOS."""

load("@builtin//struct.star", "module")
load("./clang_mac.star", "clang")
load("./config.star", "config")
load("./reproxy.star", "reproxy")

__filegroups = {}
__filegroups.update(clang.filegroups)
__handlers = {}
__handlers.update(clang.handlers)
__handlers.update(reproxy.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config["platforms"] = {}

    step_config = clang.step_config(ctx, step_config)
    if reproxy.enabled(ctx):
        step_config = reproxy.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
