# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for blink scripts."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./platform.star", "platform")

def __filegroups(ctx):
    return {}

__handlers = {
}

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "blink/generate_bindings",
            "command_prefix": platform.python_bin + " ../../third_party/blink/renderer/bindings/scripts/generate_bindings.py",
            "remote": True,
            "timeout": "2m",
            "platform_ref": "large",
        },
    ])

    # TODO: Enable remote actions for Mac and Windows.
    if runtime.os == "linux":
        step_config["rules"].extend([
            {
                "name": "blink/run_with_pythonpath",
                "command_prefix": platform.python_bin + " ../../third_party/blink/renderer/build/scripts/run_with_pythonpath.py -I ../../third_party/blink/renderer/build/scripts -I ../../third_party -I ../../third_party/pyjson5/src -I ../../tools ../../third_party/blink/renderer/build/scripts/",
                "remote": True,
                "timeout": "2m",
            },
        ])
    return step_config

blink_all = module(
    "blink_all",
    filegroups = __filegroups,
    handlers = __handlers,
    step_config = __step_config,
)
