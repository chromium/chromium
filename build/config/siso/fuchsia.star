# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Fuchsia builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") == '"fuchsia"':
            return True
    return False

fuchsia = module(
    "fuchsia",
    enabled = __enabled,
)
