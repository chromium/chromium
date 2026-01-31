# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration main entry."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./backend_config/backend.star", "backend")
load("./blink_all.star", "blink_all")
load("./config.star", "config")
load("./gn_logs.star", "gn_logs")
load("./grit.star", "grit")
load("./linux.star", chromium_linux = "chromium")
load("./mac.star", chromium_mac = "chromium")
load("./mojo.star", "mojo")
load("./platform.star", "platform")
load("./reproxy.star", "reproxy")
load("./rust.star", "rust")
load("./simple.star", "simple")
load("./windows.star", chromium_windows = "chromium")

def __disable_remote(ctx, step_config):
    gn_logs_data = gn_logs.read(ctx)
    if gn_logs_data.get("use_remoteexec") == "true":
        return step_config
    for rule in step_config["rules"]:
        rule["remote"] = False
    return step_config

def __unset_timeout(ctx, step_config):
    if not config.get(ctx, "no-remote-timeout"):
        return step_config
    for rule in step_config["rules"]:
        rule.pop("timeout", None)
    return step_config

def init(ctx):
    print("runtime: os:%s arch:%s run:%d" % (
        runtime.os,
        runtime.arch,
        runtime.num_cpu,
    ))
    host = {
        "linux": chromium_linux,
        "darwin": chromium_mac,
        "windows": chromium_windows,
    }[runtime.os]
    properties = {}
    for k, v in gn.args(ctx).items():
        properties["gn_args:" + k] = v
    for k, v in gn_logs.read(ctx).items():
        properties["gn_logs:" + k] = v

    step_config = {
        "properties": properties,
        "platforms": backend.platform_properties(ctx),
        "input_deps": {},
        "rules": [],
    }
    step_config = blink_all.step_config(ctx, step_config)
    step_config = grit.step_config(ctx, step_config)
    step_config = host.step_config(ctx, step_config)
    step_config = mojo.step_config(ctx, step_config)
    step_config = rust.step_config(ctx, step_config)
    step_config = simple.step_config(ctx, step_config)
    if reproxy.enabled(ctx):
        step_config = reproxy.step_config(ctx, step_config)

    step_config = __disable_remote(ctx, step_config)
    step_config = __unset_timeout(ctx, step_config)

    filegroups = {}
    filegroups.update(blink_all.filegroups(ctx))
    filegroups.update(host.filegroups(ctx))
    filegroups.update(rust.filegroups(ctx))
    filegroups.update(simple.filegroups(ctx))

    handlers = {}
    handlers.update(blink_all.handlers)
    handlers.update(host.handlers)
    handlers.update(rust.handlers)
    handlers.update(simple.handlers)
    handlers.update(reproxy.handlers)

    return module(
        "config",
        step_config = json.encode(step_config),
        filegroups = filegroups,
        handlers = handlers,
    )
