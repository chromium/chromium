# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Windows."""

load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./remote_exec_wrapper.star", "remote_exec_wrapper")
load("./rewrapper_to_reproxy.star", "rewrapper_to_reproxy")

__filegroups = {}
__handlers = {}
__handlers.update(rewrapper_to_reproxy.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    # rewrapper_to_reproxy takes precedence over remote exec wrapper handler if enabled.
    if rewrapper_to_reproxy.enabled(ctx):
        step_config = rewrapper_to_reproxy.step_config(ctx, step_config)
    elif remote_exec_wrapper.enabled(ctx):
        step_config = remote_exec_wrapper.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
