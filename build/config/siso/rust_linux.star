# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rust/linux."""

load("@builtin//struct.star", "module")

def __filegroups(ctx):
    return {
        "third_party/rust-toolchain:toolchain": {
            "type": "glob",
            "includes": [
                "bin/rustc",
                "lib/*.so",
            ],
        },
        "build/linux/debian_bullseye_amd64-sysroot:rustlink": {
            "type": "glob",
            "includes": [
                "*.so",
                "*.so.*",
                "*.o",
                "*.a",
            ],
        },
        "third_party/llvm-build/Release+Asserts:rustlink": {
            "type": "glob",
            "includes": [
                "bin/clang",
                "bin/clang++",
                "bin/*lld",
                "libclang*.a",
            ],
        },
        "third_party/fuchsia-sdk/sdk/arch/x64/lib:rustlink": {
            "type": "glob",
            "includes": [
                "*",
            ],
        },
        "third_party/fuchsia-sdk/sdk/arch/x64/sysroot:rustlink": {
            "type": "glob",
            "includes": [
                "lib/*",
            ],
        },
    }

def __rust_bin_handler(ctx, cmd):
    inputs = []
    for i, arg in enumerate(cmd.args):
        if arg.startswith("--sysroot=../../third_party/fuchsia-sdk/sdk/arch/x64/sysroot"):
            inputs.extend([
                "third_party/fuchsia-sdk/sdk/arch/x64/lib:rustlink",
                "third_party/fuchsia-sdk/sdk/arch/x64/sysroot:rustlink",
            ])
    ctx.actions.fix(inputs = cmd.inputs + inputs)

__handlers = {
    "rust_bin_handler": __rust_bin_handler,
}

def __step_config(ctx, step_config):
    remote_run = True  # Turn this to False when you do file access trace.
    platform_ref = "large"  # Rust actions run faster on large workers.
    clang_inputs = [
        "build/linux/debian_bullseye_amd64-sysroot:rustlink",
        "third_party/llvm-build/Release+Asserts:rustlink",
    ]
    rust_inputs = [
        "build/action_helpers.py",
        "build/gn_helpers.py",
        "build/rust/rustc_wrapper.py",
        # TODO(b/285225184): use precomputed subtree
        "third_party/rust-toolchain:toolchain",
    ]
    rust_indirect_inputs = {
        "includes": [
            "*.h",
            "*.o",
            "*.rlib",
            "*.rs",
            "*.so",
        ],
    }
    step_config["rules"].extend([
        {
            "name": "rust_bin",
            "action": "(.*_)?rust_bin",
            "inputs": rust_inputs + clang_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "handler": "rust_bin_handler",
            "deps": "none",  # disable gcc scandeps
            "remote": remote_run,
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_cdylib",
            "action": "(.*_)?rust_cdylib",
            "inputs": rust_inputs + clang_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_macro",
            "action": "(.*_)?rust_macro",
            "inputs": rust_inputs + clang_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_rlib",
            "action": "(.*_)?rust_rlib",
            "inputs": rust_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            "remote": remote_run,
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_staticlib",
            "action": "(.*_)?rust_staticlib",
            "inputs": rust_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
    ])
    return step_config

rust = module(
    "rust",
    filegroups = __filegroups,
    handlers = __handlers,
    step_config = __step_config,
)
