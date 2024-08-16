# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for mojo."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./platform.star", "platform")

def __step_config(ctx, step_config):
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
            "indirect_inputs": {
                "includes": [
                    "*.js",
                    "*.mojom",
                    "*.mojom-module",
                    "*.test-mojom",
                    "*.test-mojom-module",
                    "*.zip",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            "restat": True,
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
        },
        {
            "name": "mojo/mojom_parser",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/mojom/mojom_parser.py",
            "indirect_inputs": {
                "includes": [
                    "*.build_metadata",
                    "*.mojom",
                    "*.mojom-module",
                    "*.test-mojom",
                    "*.test-mojom-module",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            # TODO: b/285078792 - Win cross compile on Linux worker doesn't work with input_root_absolute_path=true.
            "remote": runtime.os != "windows",
            "canonicalize_dir": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
        },
        {
            "name": "mojo/validate_typemap_config",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/bindings/validate_typemap_config.py",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
        },
        {
            "name": "mojo/generate_type_mappings",
            "command_prefix": platform.python_bin + " ../../mojo/public/tools/bindings/generate_type_mappings.py",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
        },
    ])
    return step_config

mojo = module(
    "mojo",
    step_config = __step_config,
)
