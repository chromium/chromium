# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities that fill gap between platforms."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")

# Python binary name. python3 or python3.exe.
__python_bin = {
    # is_windows => python bin
    True: "python3.exe",
    False: "python3",
}[runtime.os == "windows"]

platform = module(
    "platform",
    python_bin = __python_bin,
)
