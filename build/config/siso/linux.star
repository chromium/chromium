# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for linux."""

load("@builtin//struct.star", "module")
load("./clang_linux.star", "clang")
load("./config.star", "config")
load("./mojo.star", "mojo")
load("./nacl_linux.star", "nacl")
load("./nasm_linux.star", "nasm")
load("./remote_exec_wrapper.star", "remote_exec_wrapper")
load("./rewrapper_to_reproxy.star", "rewrapper_to_reproxy")
load("./android.star", "android")

__filegroups = {}
__filegroups.update(android.filegroups)
__filegroups.update(clang.filegroups)
__filegroups.update(mojo.filegroups)
__filegroups.update(nacl.filegroups)
__filegroups.update(nasm.filegroups)

__handlers = {}
__handlers.update(android.handlers)
__handlers.update(clang.handlers)
__handlers.update(mojo.handlers)
__handlers.update(nacl.handlers)
__handlers.update(nasm.handlers)
__handlers.update(remote_exec_wrapper.handlers)
__handlers.update(rewrapper_to_reproxy.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config["platforms"] = {
        "default": {
            "OSFamily": "Linux",
            "container-image": "docker://gcr.io/chops-private-images-prod/rbe/siso-chromium/linux@sha256:d4fcda628ebcdb3dd79b166619c56da08d5d7bd43d1a7b1f69734904cc7a1bb2",
        },
    }

    # rewrapper_to_reproxy takes precedence over remote exec wrapper handler if enabled.
    if rewrapper_to_reproxy.enabled(ctx):
        step_config = rewrapper_to_reproxy.step_config(ctx, step_config)
    elif remote_exec_wrapper.enabled(ctx):
        step_config = remote_exec_wrapper.step_config(ctx, step_config)
    else:
        if android.enabled(ctx):
            step_config = android.step_config(ctx, step_config)
        step_config = clang.step_config(ctx, step_config)
        step_config = mojo.step_config(ctx, step_config)
        step_config = nacl.step_config(ctx, step_config)
        step_config = nasm.step_config(ctx, step_config)

    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
