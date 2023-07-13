# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for proto/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./protoc_wrapper.star", "protoc_wrapper")

__filegroups = {
}

def __protoc_wrapper(ctx, cmd):
    inputs = protoc_wrapper.scandeps(ctx, cmd.args)
    ctx.actions.fix(inputs = cmd.inputs + inputs)

__handlers = {
    "protoc_wrapper": __protoc_wrapper,
}

def __step_config(ctx, step_config):
    remote_run = config.get(ctx, "remote_proto") or config.get(ctx, "remote_all")
    step_config["rules"].extend([
        {
            "name": "proto_linux/protoc_wrapper",
            "command_prefix": "python3 ../../tools/protoc_wrapper/protoc_wrapper.py",
            "indirect_inputs": {
                "includes": ["*.proto"],
            },
            "exclude_input_patterns": [
                "*.o",
                "*.a",
                "*.h",
                "*.cc",
                # "*_pb2.py",
            ],
            "handler": "protoc_wrapper",
            "remote": True,
            # chromeos generates default.profraw?
            "ignore_extra_output_pattern": ".*default.profraw",
            # "deps": "depfile",
            "output_local": True,
        },
    ])
    return step_config

proto = module(
    "proto",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
