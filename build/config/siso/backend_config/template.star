# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Template for Siso backend config.

Copy this to backend.star and edit property values for your backend.
"""

load("@builtin//struct.star", "module")

def __platform_properties(ctx):
    # You can use ctx to access file by ctx.fs etc.
    # See https://chromium.googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/build/siso/docs/starlark_config.md#initialization
    return {
        "default": {
            "OSFamily": "Linux",
            "container-image": "<container image used by RBE>",
            "label:<label_key>": "<label_value>",
        },
        # Large workers are usually used for Python actions like generate bindings, mojo generators etc
        # They can run on Linux workers.
        "large": {
            "OSFamily": "Linux",
            "container-image": "<container image used by RBE>",
            "label:<label_key>": "<label_value>",
        },
    }

backend = module(
    "backend",
    platform_properties = __platform_properties,
)
