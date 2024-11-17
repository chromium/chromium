# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang-cl/windows."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_all.star", "clang_all")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")
load("./config.star", "config")
load("./gn_logs.star", "gn_logs")
load("./rewrapper_cfg.star", "rewrapper_cfg")
load("./win_sdk.star", "win_sdk")

def __filegroups(ctx):
    fg = {}
    fg.update(win_sdk.filegroups(ctx))
    fg.update(clang_all.filegroups(ctx))
    return fg

def __clang_compile_coverage(ctx, cmd):
    clang_command = clang_code_coverage_wrapper.run(ctx, list(cmd.args))
    ctx.actions.fix(args = clang_command)

__handlers = {
    "clang_compile_coverage": __clang_compile_coverage,
}

def __step_config(ctx, step_config):
    cfg = "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_windows.cfg"
    if ctx.fs.exists(cfg):
        reproxy_config = rewrapper_cfg.parse(ctx, cfg)
        largePlatform = {}
        for k, v in reproxy_config["platform"].items():
            if k.startswith("label:action"):
                continue
            largePlatform[k] = v

        # no "action_large" Windows worker pool
        windowsWorker = True
        if reproxy_config["platform"]["OSFamily"] != "Windows":
            largePlatform["label:action_large"] = "1"
            windowsWorker = False
        step_config["platforms"].update({
            "clang-cl": reproxy_config["platform"],
            "clang-cl_large": largePlatform,
        })
        step_config["input_deps"].update(clang_all.input_deps)

        # when win_toolchain_dir is unknown (e.g.
        # missing build/win_toolchain.json), we can't run
        # clang-cl remotely as we can find sysroot files
        # under exec_root, so just run locally.
        # When building with ToT Clang, we can't run clang-cl
        # remotely, too.
        remote = False
        win_toolchain_dir = win_sdk.toolchain_dir(ctx)
        if win_toolchain_dir:
            remote = True
            if reproxy_config["platform"]["OSFamily"] == "Windows":
                step_config["input_deps"].update({
                    win_toolchain_dir + ":headers": [
                        win_toolchain_dir + ":headers-ci",
                    ],
                })
            else:
                win_sdk.step_config(ctx, step_config)
        remote_wrapper = reproxy_config.get("remote_wrapper")
        input_root_absolute_path = gn_logs.read(ctx).get("clang_need_input_root_absolute_path") == "true"
        canonicalize_dir = not input_root_absolute_path

        timeout = "2m"
        if gn.args(ctx).get("use_reclient") == "false" and windowsWorker:
            # use longer timeout for siso native
            # it takes long time for input fetch (many files in sysroot etc)
            timeout = "4m"

        step_config["rules"].extend([
            {
                "name": "clang-cl/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "..\\..\\third_party\\llvm-build\\Release+Asserts\\bin\\clang-cl.exe",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang-cl.exe",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "platform_ref": "clang-cl",
                "remote": remote,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": remote_wrapper,
                "timeout": timeout,
            },
            {
                "name": "clang-cl/cc",
                "action": "(.*_)?cc",
                "command_prefix": "..\\..\\third_party\\llvm-build\\Release+Asserts\\bin\\clang-cl.exe",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang-cl.exe",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "platform_ref": "clang-cl",
                "remote": remote,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": remote_wrapper,
                "timeout": timeout,
            },
            {
                "name": "clang-coverage/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "python3.exe ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang-cl",
                "remote": remote,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": remote_wrapper,
                "timeout": timeout,
            },
            {
                "name": "clang-coverage/cc",
                "action": "(.*_)?cc",
                "command_prefix": "python3.exe ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "exclude_input_patterns": ["*.stamp"],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang-cl",
                "remote": remote,
                "input_root_absolute_path": input_root_absolute_path,
                "canonicalize_dir": canonicalize_dir,
                "remote_wrapper": remote_wrapper,
                "timeout": timeout,
            },
        ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
