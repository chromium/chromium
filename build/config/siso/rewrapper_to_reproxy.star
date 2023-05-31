# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote exec wrapper calls into reproxy config."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./config.star", "config")

__filegroups = {}

def __remove_rewrapper(ctx, cmd):
    # Slice from the first non-arg passed to rewrapper.
    # (Whilst rewrapper usage is `rewrapper [-flags] -- command ...` we don't pass -- to rewrapper.)
    non_flag_start = -1
    for i, arg in enumerate(cmd.args):
        if i == 0:
            continue
        if not arg.startswith('-'):
            non_flag_start = i
            break
    if non_flag_start < 1:
        fail("couldn't find first non-arg passed to rewrapper")
    ctx.actions.fix(
        args = cmd.args[non_flag_start:],
    )

__handlers = {
    "remove_rewrapper": __remove_rewrapper,
}

def __enabled(ctx):
    if config.get(ctx, "rewrapper_to_reproxy") and "args.gn" in ctx.metadata:
        gn_args = gn.parse_args(ctx.metadata["args.gn"])
        if gn_args.get("use_remoteexec") == "true":
            return True
    return False

def __step_config(ctx, step_config):
    labels = dict()

    # TODO(b/273407069): Choose the right cfg rather than hardcoding buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg.
    # TODO(b/273407069): Read cfg values other than labels.
    if not ctx.fs.exists("buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg"):
        fail("rewrapper_to_reproxy enabled but buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg not found, is download_remoteexec_cfg set in gclient custom_vars?")
    for line in str(ctx.fs.read("buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg")).splitlines():
        if line.startswith("labels="):
            for label in line.removeprefix("labels=").split(","):
                label_parts = label.split("=")
                if len(label_parts) != 2:
                    fail("invalid label %s" % label)
                labels[label_parts[0]] = label_parts[1]

    step_config["rules"].extend([
        {
            "name": "clang/cxx",
            "action": "(.*_)?cxx",
            "handler": "remove_rewrapper",
            "reproxy_config": {"labels": labels},
        },
        {
            "name": "clang/cc",
            "action": "(.*_)?cc",
            "handler": "remove_rewrapper",
            "reproxy_config": {"labels": labels},
        },
        {
            "name": "clang/objcxx",
            "action": "(.*_)?objcxx",
            "handler": "remove_rewrapper",
            "reproxy_config": {"labels": labels},
        },
        {
            "name": "clang/objc",
            "action": "(.*_)?objc",
            "handler": "remove_rewrapper",
            "reproxy_config": {"labels": labels},
        },
    ])
    return step_config

rewrapper_to_reproxy = module(
    "rewrapper_to_reproxy",
    enabled = __enabled,
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
