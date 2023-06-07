# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote exec wrapper calls into reproxy config."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./config.star", "config")

__filegroups = {}

def __rewrite_rewrapper(ctx, cmd):
    # Slice from the first non-arg passed to rewrapper.
    # (Whilst rewrapper usage is `rewrapper [-flags] -- command ...` we don't pass -- to rewrapper.)
    non_flag_start = -1
    cfg_file = None
    for i, arg in enumerate(cmd.args):
        if i == 0:
            continue

        # NOTE: Only handle -cfg= as that's how we call rewrapper in our .gn files.
        if arg.startswith("-cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("-cfg="))
            continue
        if not arg.startswith("-"):
            non_flag_start = i
            break
    if non_flag_start < 1:
        fail("couldn't find first non-arg passed to rewrapper for %s" % str(cmd.args))

    if not cfg_file:
        fail("cfg file expected but none found")

    if not ctx.fs.exists(cfg_file):
        fail("cmd specifies rewrapper cfg %s but not found, is download_remoteexec_cfg set in gclient custom_vars?" % cfg_file)

    reproxy_config = {}
    for line in str(ctx.fs.read(cfg_file)).splitlines():
        if line.startswith("canonicalize_working_dir="):
            reproxy_config["canonicalize_working_dir"] = line.removeprefix("canonicalize_working_dir=").lower() == "true"

        reproxy_config["download_outputs"] = True
        if line.startswith("download_outputs="):
            reproxy_config["download_outputs"] = line.removeprefix("download_outputs=").lower() == "true"

        if line.startswith("exec_strategy="):
            reproxy_config["exec_strategy"] = line.removeprefix("exec_strategy=")

        if line.startswith("inputs="):
            reproxy_config["inputs"] = line.removeprefix("inputs").split(",")

        if line.startswith("labels="):
            if "labels" not in reproxy_config:
                reproxy_config["labels"] = dict()
            for label in line.removeprefix("labels=").split(","):
                label_parts = label.split("=")
                if len(label_parts) != 2:
                    fail("not k,v %s" % label)
                reproxy_config["labels"][label_parts[0]] = label_parts[1]

        if line.startswith("platform="):
            if "platform" not in reproxy_config:
                reproxy_config["platform"] = dict()
            for label in line.removeprefix("platform=").split(","):
                label_parts = label.split("=")
                if len(label_parts) != 2:
                    fail("not k,v %s" % label)
                reproxy_config["platform"][label_parts[0]] = label_parts[1]

        if line.startswith("server_address="):
            reproxy_config["server_address"] = line.removeprefix("server_address=")

    ctx.actions.fix(
        args = cmd.args[non_flag_start:],
        reproxy_config = json.encode(reproxy_config),
    )

__handlers = {
    "rewrite_rewrapper": __rewrite_rewrapper,
}

def __enabled(ctx):
    if config.get(ctx, "rewrapper_to_reproxy") and "args.gn" in ctx.metadata:
        gn_args = gn.parse_args(ctx.metadata["args.gn"])
        if gn_args.get("use_remoteexec") == "true":
            return True
    return False

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "clang/cxx",
            "action": "(.*_)?cxx",
            "handler": "rewrite_rewrapper",
        },
        {
            "name": "clang/cc",
            "action": "(.*_)?cc",
            "handler": "rewrite_rewrapper",
        },
        {
            "name": "clang/objcxx",
            "action": "(.*_)?objcxx",
            "handler": "rewrite_rewrapper",
        },
        {
            "name": "clang/objc",
            "action": "(.*_)?objc",
            "handler": "rewrite_rewrapper",
        },
        {
            "name": "clang/asm",
            "action": "(.*_)?asm",
            "handler": "rewrite_rewrapper",
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
