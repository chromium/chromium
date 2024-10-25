# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for ChromeOS builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./config.star", "config")

def __custom_toolchain(ctx):
    if not "args.gn" in ctx.metadata:
        print("no args.gn")
        return None
    gn_args = gn.args(ctx)
    if gn_args.get("target_os") != '"chromeos"':
        return None
    if not "cros_target_cxx" in gn_args:
        print("no cros_target_cxx")
        return None
    cros_target_cxx = gn_args.get("cros_target_cxx")
    cros_target_cxx = cros_target_cxx.strip('"')
    cros_target_cxx = ctx.fs.canonpath(cros_target_cxx)
    toolchain = path.dir(path.dir(cros_target_cxx))
    if not toolchain:
        fail("failed to detect cros custom toolchain. cros_target_cxx = %s" % gn_args.get("cros_target_cxx"))
    return toolchain

def __custom_sysroot(ctx):
    if not "args.gn" in ctx.metadata:
        print("no args.gn")
        return None
    gn_args = gn.args(ctx)
    if gn_args.get("target_os") != '"chromeos"':
        return None
    if not "target_sysroot" in gn_args:
        print("no target_sysroot")
        return None
    sysroot = gn_args.get("target_sysroot")
    sysroot = sysroot.strip('"')
    sysroot = ctx.fs.canonpath(sysroot)
    if not sysroot:
        fail("failed to detect cros custom sysroot. target_sysroot = %s" % gn_args.get("target_sysroot"))
    return sysroot

def __filegroups(ctx):
    fg = {}
    toolchain = __custom_toolchain(ctx)
    print("toolchain = %s" % toolchain)
    if toolchain:
        fg[toolchain + ":headers"] = {
            "type": "glob",
            "includes": ["*"],
        }
        fg[path.join(toolchain, "lib") + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
        fg[path.join(toolchain, "lib64") + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
        fg[path.join(toolchain, "usr/lib64") + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
        fg[path.join(toolchain, "usr/bin") + ":clang"] = {
            "type": "glob",
            "includes": [
                "clang*",
                "sysroot_wrapper.hardened.ccache*",
                "x86_64-cros-linux-gnu-clang*",
            ],
        }
    sysroot = __custom_sysroot(ctx)
    print("sysroot = %s" % sysroot)
    if sysroot:
        fg[path.join(sysroot, "usr/include") + ":include"] = {
            "type": "glob",
            "includes": ["*"],
            # needs bits/stab.def, c++/*
        }
        fg[path.join(sysroot, "usr/lib") + ":headers"] = {
            "type": "glob",
            "includes": ["*.h", "crtbegin.o"],
        }
        fg[path.join(sysroot, "usr/lib64") + ":headers"] = {
            "type": "glob",
            "includes": ["*.h"],
        }
        fg[path.join(sysroot, "usr/lib") + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
        fg[path.join(sysroot, "usr/lib64") + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
        fg[path.join(sysroot, "lib64") + ":libs"] = {
            "type": "glob",
            "includes": ["*.so", "*.so.*", "*.a", "*.o"],
        }
    print(fg)
    return fg

def __step_config(ctx, step_config):
    toolchain = __custom_toolchain(ctx)
    sysroot = __custom_sysroot(ctx)
    if not (toolchain and sysroot):
        return step_config

    step_config["rules"].extend([
        {
            "name": "clang-cros/cxx",
            "action": "(.*_)?cxx",
            "command_prefix": "../../build/cros_cache/chrome-sdk/",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "5m",
        },
        {
            "name": "clang-cros/cc",
            "action": "(.*_)?cc",
            "command_prefix": "../../build/cros_cache/chrome-sdk/",
            "remote": True,
            "canonicalize_dir": True,
            "timeout": "5m",
        },
        {
            "name": "clang-cros/alink/llvm-ar",
            # Other alink steps should use clang/alink/llvm-ar rule or a
            # nacl rule.
            "action": "(target_with_system_allocator_)?alink",
            "inputs": [
                path.join(toolchain, "bin/llvm-ar"),
            ],
            "exclude_input_patterns": [
                "*.cc",
                "*.h",
                "*.js",
                "*.pak",
                "*.py",
                "*.stamp",
            ],
            "remote": config.get(ctx, "remote-link"),
            "canonicalize_dir": True,
            "timeout": "5m",
            "platform_ref": "large",
            "accumulate": True,
        },
        {
            "name": "clang-cros/solink/gcc_solink_wrapper",
            "action": "(target_with_system_allocator_)?solink",
            "command_prefix": "\"python3\" \"../../build/toolchain/gcc_solink_wrapper.py\"",
            "inputs": [
                "build/toolchain/gcc_solink_wrapper.py",
                path.join(toolchain, "bin/ld.lld"),
            ],
            "exclude_input_patterns": [
                "*.cc",
                "*.h",
                "*.js",
                "*.pak",
                "*.py",
                "*.stamp",
            ],
            "remote": config.get(ctx, "remote-link"),
            # TODO: Do not use absolute paths for custom toolchain/sysroot GN
            # args.
            "input_root_absolute_path": True,
            "platform_ref": "large",
            "timeout": "2m",
        },
        {
            "name": "clang-cros/link/gcc_link_wrapper",
            "action": "(target_with_system_allocator_)?link",
            "command_prefix": "\"python3\" \"../../build/toolchain/gcc_link_wrapper.py\"",
            "handler": "clang_link",
            "inputs": [
                "build/toolchain/gcc_link_wrapper.py",
                path.join(toolchain, "bin/ld.lld"),
            ],
            "exclude_input_patterns": [
                "*.cc",
                "*.h",
                "*.js",
                "*.pak",
                "*.py",
                "*.stamp",
            ],
            "remote": config.get(ctx, "remote-link"),
            "canonicalize_dir": True,
            "platform_ref": "large",
            "timeout": "10m",
        },
    ])
    step_config["input_deps"].update({
        sysroot + ":headers": [
            path.join(sysroot, "usr/include") + ":include",
            path.join(sysroot, "usr/lib") + ":headers",
            path.join(sysroot, "usr/lib64") + ":headers",
        ],
        path.join(toolchain, "bin/llvm-ar"): [
            path.join(toolchain, "bin/llvm-ar.elf"),
            path.join(toolchain, "lib") + ":libs",
            path.join(toolchain, "usr/lib64") + ":libs",
        ],
        path.join(toolchain, "bin/ld.lld"): [
            path.join(toolchain, "bin/lld"),
            path.join(toolchain, "bin/lld.elf"),
            path.join(toolchain, "bin/llvm-nm"),
            path.join(toolchain, "bin/llvm-nm.elf"),
            path.join(toolchain, "bin/llvm-readelf"),
            path.join(toolchain, "bin/llvm-readobj"),
            path.join(toolchain, "bin/llvm-readobj.elf"),
            path.join(toolchain, "bin/x86_64-cros-linux-gnu-clang++"),
            path.join(toolchain, "bin/x86_64-cros-linux-gnu-ld.lld"),
            path.join(toolchain, "lib") + ":libs",
            path.join(toolchain, "lib64") + ":libs",
            path.join(toolchain, "usr/bin:clang"),
            path.join(toolchain, "usr/lib64") + ":libs",
            path.join(sysroot, "lib64") + ":libs",
            path.join(sysroot, "usr/lib") + ":libs",
            path.join(sysroot, "usr/lib64") + ":libs",
        ],
    })
    return step_config

cros = module(
    "cros",
    custom_toolchain = __custom_toolchain,
    custom_sysroot = __custom_sysroot,
    filegroups = __filegroups,
    handlers = {},
    step_config = __step_config,
)
