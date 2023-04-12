# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for linux."""

load("@builtin//struct.star", "module")
load("./clang_linux.star", "clang")
load("./nacl_linux.star", "nacl")

__filegroups = {}
__filegroups.update(clang.filegroups)
__filegroups.update(nacl.filegroups)

__handlers = {}
__handlers.update(clang.handlers)
__handlers.update(nacl.handlers)

def __step_config(ctx, step_config):
    step_config["platforms"] = {
        "default": {
            "OSFamily": "Linux",
            "container-image": "docker://gcr.io/chops-private-images-prod/rbe/siso-chromium/linux@sha256:d4fcda628ebcdb3dd79b166619c56da08d5d7bd43d1a7b1f69734904cc7a1bb2",
        },
    }
    step_config = clang.step_config(ctx, step_config)
    step_config = nacl.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
