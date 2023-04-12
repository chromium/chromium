# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/linux."""

load("@builtin//struct.star", "module")

__filegroups = {}

__handlers = {}

def __step_config(ctx, step_config):
    step_config["input_deps"].update({
        # clang++ is a symlink to clang
        # but siso doesn't add symlink target automatically.
        "third_party/llvm-build/Release+Asserts/bin/clang++": [
            "third_party/llvm-build/Release+Asserts/bin/clang",
        ],
    })
    step_config["rules"].extend([
        {
            "name": "clang/cxx",
            "action": "cxx",
            "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang++ ",
            "inputs": [
                "third_party/llvm-build/Release+Asserts/bin/clang++",
            ],
            "remote": True,
        },
        {
            "name": "clang/cc",
            "action": "cc",
            "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang ",
            "inputs": [
                "third_party/llvm-build/Release+Asserts/bin/clang",
            ],
            "remote": True,
        },
    ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
