# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote exec wrapper calls into reproxy config."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./mojo.star", "mojo")

__filegroups = {}

def __parse_rewrapper_cfg(ctx, cfg_file):
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

        if line.startswith("exec_timeout="):
            reproxy_config["exec_timeout"] = line.removeprefix("exec_timeout=")

        if line.startswith("inputs="):
            reproxy_config["inputs"] = line.removeprefix("inputs=").split(",")

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
    return reproxy_config

def __rewrite_rewrapper(ctx, cmd):
    # Example command:
    #   ../../buildtools/reclient/rewrapper
    #     -cfg=../../buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg
    #     -exec_root=/path/to/your/chromium/src/
    #     ../../third_party/llvm-build/Release+Asserts/bin/clang++
    #     [rest of clang args]
    # We don't need to care about:
    #   -exec_root: Siso already knows this.
    wrapped_command_pos = -1
    cfg_file = None
    for i, arg in enumerate(cmd.args):
        if i == 0:
            continue
        if arg.startswith("-cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("-cfg="))
            continue
        if not arg.startswith("-"):
            wrapped_command_pos = i
            break
    if wrapped_command_pos < 1:
        fail("couldn't find first non-arg passed to rewrapper for %s" % str(cmd.args))
    ctx.actions.fix(
        args = cmd.args[wrapped_command_pos:],
        reproxy_config = json.encode(__parse_rewrapper_cfg(ctx, cfg_file)),
    )

def __rewrite_action_remote_py(ctx, cmd):
    # Example command:
    #   python3
    #     ../../build/util/action_remote.py
    #     ../../buildtools/reclient/rewrapper
    #     --custom_processor=mojom_parser
    #     --cfg=../../buildtools/reclient_cfgs/python/rewrapper_linux.cfg
    #     --exec_root=/path/to/your/chromium/src/
    #     --input_list_paths=gen/gpu/ipc/common/surface_handle__parser__remote_inputs.rsp
    #     --output_list_paths=gen/gpu/ipc/common/surface_handle__parser__remote_outputs.rsp
    #     python3
    #     ../../mojo/public/tools/mojom/mojom_parser.py
    #     [rest of mojo args]
    # We don't need to care about:
    #   --exec_root: Siso already knows this.
    #   --custom_processor: Used by action_remote.py to apply mojo handling.
    #   --[input,output]_list_paths: We should always use mojo.star for Siso.
    wrapped_command_pos = -1
    cfg_file = None
    for i, arg in enumerate(cmd.args):
        if i < 3:
            continue
        if arg.startswith("--cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("--cfg="))
            continue
        if not arg.startswith("-"):
            wrapped_command_pos = i
            break
    if wrapped_command_pos < 1:
        fail("couldn't find action command in %s" % str(cmd.args))
    ctx.actions.fix(
        args = cmd.args[wrapped_command_pos:],
        reproxy_config = json.encode(__parse_rewrapper_cfg(ctx, cfg_file)),
    )

__handlers = {
    "rewrite_rewrapper": __rewrite_rewrapper,
    "rewrite_action_remote_py": __rewrite_action_remote_py,
}

def __enabled(ctx):
    if config.get(ctx, "rewrapper_to_reproxy") and "args.gn" in ctx.metadata:
        gn_args = gn.parse_args(ctx.metadata["args.gn"])
        if gn_args.get("use_remoteexec") == "true":
            return True
    return False

def __step_config(ctx, step_config):
    mojom_parser_rule = mojo.step_rule()
    mojom_parser_rule.update({
        "command_prefix": "python3 ../../build/util/action_remote.py ../../buildtools/reclient/rewrapper --custom_processor=mojom_parser",
        "handler": "rewrite_action_remote_py",
    })
    step_config["rules"].extend([
        mojom_parser_rule,
        {
            "name": "action_remote",
            "command_prefix": "python3 ../../build/util/action_remote.py ../../buildtools/reclient/rewrapper",
            "handler": "rewrite_action_remote_py",
        },
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
