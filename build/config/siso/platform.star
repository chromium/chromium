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
    # Subdirectories under lib/pythonX.Y/ unused by remote build actions.
    exclude_dirs = [
        "*/test",
        "*/tests",
        "config-*",
        "ensurepip",
        "idlelib",
        "pydoc_data",
        "site-packages",
        "tkinter",
        "turtledemo",
    ]

    # Siso glob treats "**" as single-segment "*" and does not exclude
    # directories by prefix; max depth under lib/ (site-packages) is 8.
    return {
        "third_party/cpython3/linux-amd64:cpython3": {
            "type": "glob",
            "includes": [
                "bin/python3",
            ] + [
                "lib" + "/*" * depth
                for depth in range(1, 10)
            ],
            "excludes": [
                # Keep stdlib __pycache__/*.pyc (precompiled with unchecked-hash
                # in 3pp/cpython3), except for excluded modules like turtle.py.
                "lib/python*/turtle.py",
                "lib/python*/__pycache__/turtle.*.pyc",
            ] + [
                "lib/python*/" + d + "/*" * depth
                for d in exclude_dirs
                for depth in range(1, 10)
            ],
        },
    }

platform = module(
    "platform",
    filegroups = __filegroups,
    python_bin = __python_bin,
    remote_python_bin = __remote_python_bin,
)
