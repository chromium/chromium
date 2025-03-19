# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rust/linux."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./ar.star", "ar")
load("./config.star", "config")
load("./fuchsia.star", "fuchsia")
load("./win_sdk.star", "win_sdk")

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
                "bin/*lld*",
                "bin/clang",
                "bin/clang++",
                "lib/clang/*/share/cfi_ignorelist.txt",
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
    args = cmd.args

    # there is a case that command line sets environment variable
    # like `TOOL_VERSION=xxxx "python3" ..`
    if args[0] == "/bin/sh":
        args = args[2].split(" ")
    for i, arg in enumerate(args):
        if arg.startswith("--sysroot=../../third_party/fuchsia-sdk/sdk"):
            sysroot = ctx.fs.canonpath(arg.removeprefix("--sysroot="))
            libpath = path.join(path.dir(sysroot), "lib")
            inputs.extend([
                sysroot + ":link",
                libpath + ":link",
            ])
        elif arg.startswith("--sysroot=../../third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot"):
            use_android_toolchain = True
            inputs.append("third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot:headers")
        if arg == "-isysroot":
            sysroot = ctx.fs.canonpath(args[i + 1])
            inputs.extend([
                sysroot + ":link",
            ])
        if arg.startswith("--target="):
            target = arg.removeprefix("--target=")
        if arg.startswith("-Clinker="):
            linker = arg.removeprefix("-Clinker=")
            if linker.startswith("\""):
                linker = linker[1:len(linker) - 1]
            linker_base = path.dir(path.dir(linker))

            # TODO(crbug.com/380798907): expand input_deps, instead of using label?
            inputs.append(ctx.fs.canonpath(linker_base) + ":link")
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

    # Replace thin archives (.lib) with -start-lib ... -end-lib in rsp file for
    # Windows builds.
    new_rspfile_content = None
    is_windows = None
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") == '"win"':
            is_windows = True
    if runtime.os == "windows":
        is_windows = True
    if is_windows:
        new_lines = []
        for line in str(cmd.rspfile_content).split("\n"):
            new_elems = []
            for elem in line.split(" "):
                # Parse only .lib files.
                if not elem.endswith(".lib"):
                    new_elems.append(elem)
                    continue

                # Parse files under the out dir.
                fname = ctx.fs.canonpath(elem.removeprefix("-Clink-arg="))
                if not ctx.fs.exists(fname):
                    new_elems.append(elem)
                    continue

                # Check if the library is generated or not.
                # The source libs are not under the build dir.
                build_dir = ctx.fs.canonpath("./")
                if path.rel(build_dir, fname).startswith("../../"):
                    new_elems.append(elem)
                    continue

                ents = ar.entries(ctx, fname, build_dir)
                if not ents:
                    new_elems.append(elem)
                    continue

                new_elems.append("-Clink-arg=-start-lib")
                new_elems.extend(["-Clink-arg=" + e for e in ents])
                new_elems.append("-Clink-arg=-end-lib")
            new_lines.append(" ".join(new_elems))
        new_rspfile_content = "\n".join(new_lines)

    ctx.actions.fix(inputs = cmd.inputs + inputs, rspfile_content = new_rspfile_content or cmd.rspfile_content)

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

    remote = True
    if runtime.os != "linux":
        remote = False
    clang_inputs = [
        "build/linux/debian_bullseye_amd64-sysroot:rustlink",
        "third_party/llvm-build/Release+Asserts:rustlink",
    ]
    if win_sdk.enabled(ctx):
        clang_inputs.append(win_sdk.toolchain_dir(ctx) + ":libs")

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
            "remote": remote,
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
            "remote": remote,
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_macro",
            "action": "(.*_)?rust_macro",
            "inputs": rust_inputs + clang_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "handler": "rust_link_handler",
            "deps": "none",  # disable gcc scandeps
            # "canonicalize_dir": True,  # TODO(b/300352286)
            "remote": remote,
            "timeout": "2m",
            "platform_ref": platform_ref,
        },
        {
            "name": "rust_rlib",
            "action": "(.*_)?rust_rlib",
            "inputs": rust_inputs,
            "indirect_inputs": rust_indirect_inputs,
            "deps": "none",  # disable gcc scandeps
            "remote": remote,
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
            "remote": remote,
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
            "remote": remote and config.get(ctx, "cog"),
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
            "remote": remote and config.get(ctx, "cog"),
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
