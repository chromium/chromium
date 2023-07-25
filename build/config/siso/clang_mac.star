# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang/mac."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_all.star", "clang_all")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")
load("./rewrapper_cfg.star", "rewrapper_cfg")

__filegroups = {
    "build/mac_files/xcode_binaries/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk:headers": {
        "type": "glob",
        "includes": ["*"],
    },
}
__filegroups.update(clang_all.filegroups)

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
        step_config["platforms"].update({
            "clang": reproxy_config["platform"],
        })
        step_config["input_deps"].update(clang_all.input_deps)
        step_config["rules"].extend([
            {
                "name": "clang/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang++",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang/cc",
                "action": "(.*_)?cc",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang/objcxx",
                "action": "(.*_)?objcxx",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang++",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang/objc",
                "action": "(.*_)?objc",
                "command_prefix": "../../third_party/llvm-build/Release+Asserts/bin/clang",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang-coverage/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang-coverage/cc",
                "action": "(.*_)?cc",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang-coverage/objcxx",
                "action": "(.*_)?objcxx",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
            {
                "name": "clang-coverage/objc",
                "action": "(.*_)?objc",
                "command_prefix": "python3 ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
            },
        ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
