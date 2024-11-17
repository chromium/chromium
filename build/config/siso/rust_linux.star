# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rust/linux."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./fuchsia.star", "fuchsia")

def __filegroups(ctx):
    fg = {
        "third_party/rust-toolchain:toolchain": {
            "type": "glob",
            "includes": [
                "bin/rustc",
                "lib/*.so",
                "lib/libclang.so.*",
                "lib/rustlib/src/rust/library/std/src/lib.rs",
                "lib/rustlib/x86_64-unknown-linux-gnu/lib/*",
            ],
        },
        "third_party/rust:rustlib": {
            "type": "glob",
            "includes": [
                "*.rs",
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
    }
    if fuchsia.enabled(ctx):
        fg.update(fuchsia.filegroups(ctx))
    return fg

def __rust_link_handler(ctx, cmd):
    inputs = []
    use_android_toolchain = None
    target = None
    for i, arg in enumerate(cmd.args):
        if arg.startswith("--sysroot=../../third_party/fuchsia-sdk/sdk"):
            sysroot = ctx.fs.canonpath(arg.removeprefix("--sysroot="))
            libpath = path.join(path.dir(sysroot), "lib")
            inputs.extend([
                sysroot + ":link",
                libpath + ":link",
            ])
        elif arg.startswith("--sysroot=../../third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot"):
            use_android_toolchain = True
        if arg.startswith("--target="):
            target = arg.removeprefix("--target=")
    if use_android_toolchain and target:
        # e.g. target=aarch64-linux-android26
        android_ver = ""
        i = target.find("android")
        if i >= 0:
            android_ver = target[i:].removeprefix("android").removeprefix("eabi")
        if android_ver:
            android_arch = target.removesuffix(android_ver)
            filegroup = "third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/%s/%s:link" % (android_arch, android_ver)
            inputs.append(filegroup)

    ctx.actions.fix(inputs = cmd.inputs + inputs)

def __rust_build_handler(ctx, cmd):
    inputs = []
    for i, arg in enumerate(cmd.args):
        if arg == "--src-dir":
            inputs.append(ctx.fs.canonpath(cmd.args[i + 1]))
    ctx.actions.fix(inputs = cmd.inputs + inputs)

__handlers = {
    "rust_link_handler": __rust_link_handler,
    "rust_build_handler": __rust_build_handler,
}

def __step_config(ctx, step_config):
    platform_ref = "large"  # Rust actions run faster on large workers.
    clang_inputs = [
        "build/linux/debian_bullseye_amd64-sysroot:rustlink",
        "third_party/llvm-build/Release+Asserts:rustlink",
    ]
    rust_toolchain = [
        # TODO(b/285225184): use precomputed subtree
        "third_party/rust-toolchain:toolchain",
    ]
    rust_inputs = [
        "build/action_helpers.py",
        "build/gn_helpers.py",
        "build/rust/rustc_wrapper.py",
    ] + rust_toolchain
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
            "handler": "rust_link_handler",
            "deps": "none",  # disable gcc scandeps
            "remote": True,
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_cdylib",
            "action": "(.*_)?rust_cdylib",
            "inputs": rust_inputs + clang_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "handler": "rust_link_handler",
            "deps": "none",  # disable gcc scandeps
            "remote": True,
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_macro",
            "action": "(.*_)?rust_macro",
            "inputs": rust_inputs + clang_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "remote": True,
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_rlib",
            "action": "(.*_)?rust_rlib",
            "inputs": rust_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            "remote": True,
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
            "remote": True,
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust/run_build_script",
            "command_prefix": "python3 ../../build/rust/run_build_script.py",
            "inputs": [
                "third_party/rust-toolchain:toolchain",
                "third_party/rust:rustlib",
            ],
            "handler": "rust_build_handler",
            "remote": config.get(ctx, "cog"),
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            "name": "rust/find_std_rlibs",
            "command_prefix": "python3 ../../build/rust/std/find_std_rlibs.py",
            "inputs": [
                "third_party/rust-toolchain:toolchain",
                "third_party/rust-toolchain/lib/rustlib:rlib",
            ],
            "remote": config.get(ctx, "cog"),
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            # rust/bindgen fails remotely when *.d does not exist.
            # TODO(b/356496947): need to run scandeps?
            "name": "rust/bindgen",
            "command_prefix": "python3 ../../build/rust/run_bindgen.py",
            "inputs": rust_toolchain + clang_inputs,
            "remote": False,
            "timeout": "2m",
        },
    ])
    return step_config

rust = module(
    "rust",
    filegroups = __filegroups,
    handlers = __handlers,
    step_config = __step_config,
)
