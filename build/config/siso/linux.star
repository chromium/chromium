# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@builtin//struct.star", "module")

__filegroups = {}

__handlers = {}

def __step_config(ctx):
    step_config = {}
    step_config["platforms"] = {
        "default": {
            "OSFamily": "Linux",
            "container-image": "docker://gcr.io/chops-private-images-prod/rbe/siso-chromium/linux@sha256:d4fcda628ebcdb3dd79b166619c56da08d5d7bd43d1a7b1f69734904cc7a1bb2",
        },
    }
    step_config["input_deps"] = {
        "third_party/llvm-build/Release+Asserts/bin/clang++": [
            "third_party/llvm-build/Release+Asserts/bin/clang",
        ],
    }
    step_config["rules"] = [
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
    ]
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
