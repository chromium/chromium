# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Windows."""

load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./remote_exec_wrapper.star", "remote_exec_wrapper")
load("./reproxy_from_rewrapper.star", "reproxy_from_rewrapper")

__filegroups = {}
__handlers = {}
__handlers.update(reproxy_from_rewrapper.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)

    # reproxy_from_rewrapper takes precedence over remote exec wrapper handler if enabled.
    if reproxy_from_rewrapper.enabled(ctx):
        step_config = reproxy_from_rewrapper.step_config(ctx, step_config)
    elif remote_exec_wrapper.enabled(ctx):
        step_config = remote_exec_wrapper.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
