# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for blink scripts."""

load("@builtin//struct.star", "module")
load("./platform.star", "platform")

def __filegroups(ctx):
    return {}

__handlers = {
}

__input_deps = {
}

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "blink/generate_bindings",
            "command_prefix": platform.python_bin + " ../../third_party/blink/renderer/bindings/scripts/generate_bindings.py",
            "inputs": [
                # build/print_python_deps.py couldn't detect this?
                # TODO(crbug.com/1475569): fix build/print_python_deps.py
                "third_party/mako/mako/mako/ext/pygmentplugin.py",
            ],
            "remote": True,
            "platform_ref": "large",
        },
    ])
    return step_config

blink_all = module(
    "blink_all",
    filegroups = __filegroups,
    handlers = __handlers,
    step_config = __step_config,
)
