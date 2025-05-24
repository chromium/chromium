# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for V8 builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./platform.star", "platform")

def __step_config(ctx, step_config):
    remote_run = True  # Turn this to False when you do file access trace.
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_cpu", "").strip('"') == "x86":
            # RBE doesn't support ELF32
            remote_run = False
    step_config["rules"].extend([
        {
            "name": "v8/torque",
            "command_prefix": platform.python_bin + " ../../v8/tools/run.py ./torque",
            "remote": remote_run,
            "timeout": "2m",
        },
        {
            "name": "v8/mksnapshot",
            "command_prefix": platform.python_bin + " ../../v8/tools/run.py ./mksnapshot",
            "remote": remote_run,
            "timeout": "2m",
            # This action may consume a lot of memory on sanitizer builders.
            # 49s on n2-custom-3840-2 -> 32s on n2-highmem-8
            "platform_ref": "large",
            # The outputs of mksnapshot are often required for running the
            # following steps.
            "output_local": True,
        },
    ])
    return step_config

v8 = module(
    "v8",
    step_config = __step_config,
)
