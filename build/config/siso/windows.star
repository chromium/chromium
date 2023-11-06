# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Windows."""

load("@builtin//struct.star", "module")
load("./clang_windows.star", "clang")
load("./config.star", "config")
load("./reproxy.star", "reproxy")

def __filegroups(ctx):
    fg = {}
    fg.update(clang.filegroups(ctx))
    return fg

__handlers = {}
__handlers.update(clang.handlers)

def __disable_remote_b289968566(ctx, step_config):
    rule = {
        # TODO(b/289968566): they often faile with exit=137 (OOM?).
        # We should migrate default machine type to n2-standard-2.
        "name": "b289968566/exit-137",
        "action_outs": [
            "./obj/chrome/test/unit_tests/chrome_browsing_data_remover_delegate_unittest.obj",
            "./obj/content/browser/browser/browser_interface_binders.obj",
        ],
        "remote": False,
    }
    if reproxy.enabled(ctx):
        rule["handler"] = "strip_rewrapper"
    step_config["rules"].insert(0, rule)
    return step_config

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = __disable_remote_b289968566(ctx, step_config)
    step_config = clang.step_config(ctx, step_config)
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
