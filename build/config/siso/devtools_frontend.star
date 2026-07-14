# -*- bazel-starlark -*-
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for devtools-frontend."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./platform.star", "platform")
load("./tsc.star", "tsc")

# TODO: crbug.com/1478909 - Specify typescript inputs in GN config.
def __filegroups(ctx):
    return {
        "third_party/devtools-frontend/src/node_modules/typescript:typescript": {
            "type": "glob",
            "includes": ["*"],
        },
        "third_party/devtools-frontend/src/node_modules:node_modules": {
            "type": "glob",
            "includes": ["*.cjs", "*.js", "*.json", "*.mjs", "*.ts"],
        },
    }

def __step_config(ctx, step_config):
    step_config["input_deps"].update({
        "third_party/devtools-frontend/src/scripts/build/typescript/ts_library.py": [
            "third_party/devtools-frontend/src/node_modules/typescript:typescript",
            "third_party/devtools-frontend/src/node_modules:node_modules",
        ],
        "third_party/devtools-frontend/src/scripts/build/esbuild.js": [
            "third_party/devtools-frontend/src/node_modules:node_modules",
        ],
        "third_party/devtools-frontend/src/scripts/build/generate_css_js_files.js": [
            "third_party/devtools-frontend/src/node_modules:node_modules",
        ],
    })

    step_config["rules"].extend([
        {
            "name": "devtools-frontend/typescript/ts_library",
            "command_prefix": "python3 ../../third_party/devtools-frontend/src/scripts/build/typescript/ts_library.py",
            "remote": config.get(ctx, "default-remote"),
            "output_local": True,
            "timeout": "2m",
            "platform_ref": "large",
        },
        {
            "name": "devtools-frontend/esbuild",
            "command_prefix": platform.python_bin + " ../../third_party/node/node.py ../../third_party/devtools-frontend/src/scripts/build/esbuild.js",
            "remote": config.get(ctx, "default-remote"),
            "timeout": "2m",
            "platform_ref": "large",
        },
        {
            "name": "devtools-frontend/build/generate_css_js_files",
            "command_prefix": "python3 ../../third_party/node/node.py ../../third_party/devtools-frontend/src/scripts/build/generate_css_js_files.js",
            "remote": config.get(ctx, "default-remote"),
            "output_local": True,
            "timeout": "2m",
        },
    ])
    return step_config

devtools_frontend = module(
    "devtools_frontend",
    step_config = __step_config,
    handlers = {},
    filegroups = __filegroups,
)
