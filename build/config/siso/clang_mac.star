# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/mac."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_all.star", "clang_all")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")
load("./config.star", "config")
load("./gn_logs.star", "gn_logs")
load("./mac_sdk.star", "mac_sdk")
load("./rewrapper_cfg.star", "rewrapper_cfg")

def __filegroups(ctx):
    fg = {}
    fg.update(mac_sdk.filegroups(ctx))
    fg.update(clang_all.filegroups(ctx))
    return fg

def __clang_compile_coverage(ctx, cmd):
    clang_command = clang_code_coverage_wrapper.run(ctx, list(cmd.args))
    ctx.actions.fix(args = clang_command)

__handlers = {
    "clang_compile_coverage": __clang_compile_coverage,
}

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

        gn_logs_data = gn_logs.read(ctx)
        input_root_absolute_path = gn_logs_data.get("clang_need_input_root_absolute_path") == "true"
        input_root_absolute_path_for_objc = gn_logs_data.get("clang_need_input_root_absolute_path_for_objc") == "true"

        canonicalize_dir = not input_root_absolute_path
        canonicalize_dir_for_objc = not input_root_absolute_path_for_objc

        step_config["rules"].extend([
            {
                "name": "clang/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang++",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "platform_ref": "clang",
                "remote": True,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang/cc",
                "action": "(.*_)?cc",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "platform_ref": "clang",
                "remote": True,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang/objcxx",
                "action": "(.*_)?objcxx",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang++",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
                "input_root_absolute_path": input_root_absolute_path_for_objc,
                "canonicalize_dir": canonicalize_dir_for_objc,
            },
            {
                "name": "clang/objc",
                "action": "(.*_)?objc",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
                "input_root_absolute_path": input_root_absolute_path_for_objc,
                "canonicalize_dir": canonicalize_dir_for_objc,
            },
            {
                "name": "clang-coverage/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang-coverage/cc",
                "action": "(.*_)?cc",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang-coverage/objcxx",
                "action": "(.*_)?objcxx",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
                "input_root_absolute_path": input_root_absolute_path_for_objc,
                "canonicalize_dir": canonicalize_dir_for_objc,
            },
            {
                "name": "clang-coverage/objc",
                "action": "(.*_)?objc",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
                "input_root_absolute_path": input_root_absolute_path_for_objc,
                "canonicalize_dir": canonicalize_dir_for_objc,
            },
        ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
