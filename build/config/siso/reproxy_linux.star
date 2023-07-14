# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for remote execution via reproxy.

This covers only actions that are not already remotely-executed via
rewrapper commands. Changes to that should be made in
reproxy_from_rewrapper.star instead."""

load("@builtin//struct.star", "module")
load("./proto_linux.star", "proto")
load("./rewrapper_cfg.star", "rewrapper_cfg")

__filegroups = {}
__filegroups.update(proto.filegroups)

__handlers = {}
__handlers.update(proto.handlers)

def __step_config(ctx, step_config):
    step_config = proto.step_config(ctx, step_config)
    for rule in step_config["rules"]:
        if rule["name"] == "proto_linux/protoc_wrapper" and rule["remote"]:
            rule["reproxy_config"] = rewrapper_cfg.parse(ctx, "buildtools/reclient_cfgs/python/rewrapper_linux.cfg")
    return step_config

reproxy = module(
    "reproxy",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
