# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/mac."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_all.star", "clang_all")
load("./clang_unix.star", "clang_unix")
load("./config.star", "config")
load("./gn_logs.star", "gn_logs")
load("./mac_sdk.star", "mac_sdk")
load("./rewrapper_cfg.star", "rewrapper_cfg")

def __filegroups(ctx):
    fg = {}
    fg.update(mac_sdk.filegroups(ctx))
    fg.update(clang_all.filegroups(ctx))
    return fg

__handlers = {}
__handlers.update(clang_unix.handlers)

def __step_config(ctx, step_config):
    cfg = "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_mac.cfg"
    if ctx.fs.exists(cfg):
        reproxy_config = rewrapper_cfg.parse(ctx, cfg)
        largePlatform = {}
        for k, v in reproxy_config["platform"].items():
            if k.startswith("label:action"):
                continue
            largePlatform[k] = v
        largePlatform["label:action_large"] = "1"
        step_config["platforms"].update({
            "clang": reproxy_config["platform"],
            "clang_large": largePlatform,
        })
        step_config["input_deps"].update(clang_all.input_deps)

        clang_rules = clang_unix.rules(ctx)

        for rule in clang_rules:
            if "remote" in rule and rule["remote"]:
                rule["remote_wrapper"] = reproxy_config["remote_wrapper"]
                if "platform_ref" not in rule:
                    rule["platform_ref"] = "clang"
                elif rule["platform_ref"] == "large":
                    rule["platform_ref"] = "clang_large"
            step_config["rules"].append(rule)
    elif gn.args(ctx).get("use_remoteexec") == "true":
        fail("remoteexec requires rewrapper config")
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
