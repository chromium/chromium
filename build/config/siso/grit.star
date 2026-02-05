# -*- bazel-starlark -*-
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for grit."""

load("@builtin//struct.star", "module")
load("./config.star", "config")

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "grit/chrome_app_generated_resources",
            # TODO(crbug.com/452240479): while we now support grit_strings
            # actions we want to support all grit actions to run remotely and
            # use command_prefix instead of action
            "action": "__chrome_app_generated_resources__strings_grit.*",
            "remote": config.get(ctx, "googlechrome"),
            # Only runs on Linux workers.
            "remote_command": "python3",
            "platform_ref": "large",
            "canonicalize_dir": True,
        },
    ])
    return step_config

grit = module(
    "grit",
    step_config = __step_config,
)
