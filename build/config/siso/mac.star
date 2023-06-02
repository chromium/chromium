# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for macOS."""

load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./remote_exec_wrapper.star", "remote_exec_wrapper")

__filegroups = {}
__handlers = {}

def __step_config(ctx, step_config):
    # TODO(b/273407069): Handle reproxy mode.
    config.check(ctx)
    if remote_exec_wrapper.enabled(ctx):
        step_config = remote_exec_wrapper.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
