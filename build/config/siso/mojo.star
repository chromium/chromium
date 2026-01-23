# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for mojo."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./platform.star", "platform")

def __step_config(ctx, step_config):
    remote_run = config.get(ctx, "googlechrome")

    # mojom_bindings_generator.py will run faster on n2-highmem-8 than
    # n2-custom-2-3840
    # e.g.
    #  n2-highmem-8: exec: 880.202978ms
    #  n2-custom-2-3840: exec: 2.42808488s
    platform_ref = "large"
    step_config["rules"].extend([
        {
            "name": "mojo/mojom_bindings_generator",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/bindings/mojom_bindings_generator.py",
            "restat": True,
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
            "remote_command": "python3",  # only run on Linux worker even for CI Windows.
        },
        {
            "name": "mojo/mojom_parser",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/mojom/mojom_parser.py",
            "remote": remote_run and runtime.os != "windows",
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
            "remote_command": "python3",  # only run on Linux worker even for CI Windows.
        },
        {
            "name": "mojo/validate_typemap_config",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/bindings/validate_typemap_config.py",
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
            "remote_command": "python3",  # only run on Linux worker even for CI Windows.
        },
        {
            "name": "mojo/generate_type_mappings",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/bindings/generate_type_mappings.py",
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
            "remote_command": "python3",  # only run on Linux worker even for CI Windows.
        },
    ])
    return step_config

mojo = module(
    "mojo",
    step_config = __step_config,
)
