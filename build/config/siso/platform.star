# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities that fill gap between platforms."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")

# Python binary name for host actions.
__python_bin = "../../third_party/cpython3/host/bin/python3"

# Python binary name for remote Linux worker actions (cross-compilation from Windows/macOS).
__remote_python_bin = "../../third_party/cpython3/linux-amd64/bin/python3"

def __filegroups(ctx):
    return {
        "third_party/cpython3/linux-amd64:cpython3": {
            "type": "glob",
            "includes": [
                "*",
            ],
            "excludes": [
                "bin/python3.11",
                "*.h",
            ],
        },
    }

platform = module(
    "platform",
    filegroups = __filegroups,
    python_bin = __python_bin,
    remote_python_bin = __remote_python_bin,
)
