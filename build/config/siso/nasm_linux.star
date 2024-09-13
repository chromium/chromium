# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for nasm/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

def __filegroups(ctx):
    return {}

__handlers = {}

def __step_config(ctx, step_config):
    remote_run = True  # Turn this to False when you do file access trace.
    rules = []
    for toolchain in ["", "clang_x64"]:
        nasm_path = path.join(toolchain, "nasm")
        rules.append({
            "name": path.join("nasm", toolchain),
            "command_prefix": "python3 ../../build/gn_run_binary.py " + nasm_path,
            "indirect_inputs": {
                "includes": ["*.asm"],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            "remote": remote_run,
            # chromeos generates default.profraw?
            "ignore_extra_output_pattern": ".*default.profraw",
            "timeout": "2m",
        })
    step_config["rules"].extend(rules)
    return step_config

nasm = module(
    "nasm",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
