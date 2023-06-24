# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for nacl/linux."""

load("@builtin//struct.star", "module")

__filegroups = {
    "native_client/toolchain/linux_x86/pnacl_newlib/bin/pydir:pydir": {
        "type": "glob",
        "includes": ["*.py"],
    },
    "native_client/toolchain/linux_x86/pnacl_newlib/lib:libllvm": {
        "type": "glob",
        "includes": ["libLLVM*.so"],
    },
    "native_client/toolchain/linux_x86/saigo_newlib/bin:clang": {
        "type": "glob",
        "includes": ["clang-*"],
    },
    "native_client/toolchain/linux_x86/saigo_newlib/lib:libso": {
        "type": "glob",
        "includes": ["*.so*"],
    },
    "native_client/toolchain/linux_x86/nacl_x86_glibc/lib/gcc/x86_64-nacl:crtbegin": {
        "type": "glob",
        "includes": ["crtbegin.o"],
    },
    "native_client/toolchain/linux_x86/nacl_x86_glibc/libexec/gcc/x86_64-nacl:ccbackend": {
        "type": "glob",
        "includes": ["cc1", "cc1plus", "collect2"],
    },
    # for precomputed subtrees
    "native_client/toolchain/linux_x86/nacl_x86_glibc:header-files": {
        "type": "glob",
        "includes": ["*.h", "*/include/c++/*/*", "*/include/c++/*/*/*"],
    },
    "native_client/toolchain/linux_x86/pnacl_newlib:header-files": {
        "type": "glob",
        "includes": ["*.h", "*/include/c++/*/*", "*/include/c++/*/*/*"],
    },
    "native_client/toolchain/linux_x86/saigo_newlib:header-files": {
        "type": "glob",
        "includes": ["*.h", "*/include/c++/*/*", "*/include/c++/*/*/*"],
    },
}

__handlers = {}

def __step_config(ctx, step_config):
    step_config["rules"].extend([
        {
            "name": "nacl_linux/pnacl-clang++",
            "action": "newlib_pnacl.*_cxx",
            "command_prefix": "../../native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang++",
            "inputs": [
                "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang++",
            ],
            "remote": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            "name": "nacl_linux/pnacl-clang",
            "action": "newlib_pnacl.*_cc",
            "command_prefix": "../../native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang",
            "inputs": [
                "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang",
            ],
            "remote": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            "name": "nacl_linux/glibc/x86_64-nacl-gcc",
            "action": "glibc_x64_cc",
            "inputs": [
                "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-gcc",
            ],
            # ELF-32 doesn't work on gVisor,
            # so will local-fallback if gVisor is used.
            # TODO(b/278485912): remote=True for trusted instance.
            "remote": False,
            "input_root_absolute_path": True,
        },
        {
            "name": "nacl_linux/glibc/x86_64-nacl-g++",
            "action": "glibc_x64_cxx",
            "inputs": [
                "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-g++",
            ],
            # ELF-32 doesn't work on gVisor,
            # so will local-fallback if gVisor is used.
            # TODO(b/278485912): remote=True for trusted instance.
            "remote": False,
            "input_root_absolute_path": True,
        },
        {
            "name": "nacl_linux/pnacl_newlib/x86_64-nacl-clang++",
            "action": "clang_newlib_x64_cxx",
            "inputs": [
                "native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang++",
                "native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/bin/ld",
            ],
            "remote": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            "name": "nacl_linux/pnacl_newlib/x86_64-nacl-clang",
            "action": "clang_newlib_x64_cc",
            "inputs": [
                "native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang",
                "native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/bin/ld",
            ],
            "remote": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            "name": "nacl_linux/saigo_newlib/x86_64-nacl-clang++",
            "action": "irt_x64_cxx",
            "command_prefix": "../../native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang++",
            "inputs": [
                "native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang++",
            ],
            "remote": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
        {
            "name": "nacl_linux/saigo_newlib/x86_64-nacl-clang",
            "action": "irt_x64_cc",
            "command_prefix": "../../native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang",
            "inputs": [
                "native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang",
            ],
            "remote": True,
            "input_root_absolute_path": True,
            "timeout": "2m",
        },
    ])

    step_config["input_deps"].update({
        "native_client/toolchain/linux_x86/nacl_x86_glibc:headers": [
            "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-gcc",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-g++",
            "native_client/toolchain/linux_x86/nacl_x86_glibc:header-files",
        ],
        "native_client/toolchain/linux_x86/pnacl_newlib:headers": [
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang++",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang++",
            "native_client/toolchain/linux_x86/pnacl_newlib:header-files",
        ],
        "native_client/toolchain/linux_x86/saigo_newlib:headers": [
            "native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang",
            "native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang++",
            "native_client/toolchain/linux_x86/saigo_newlib:header-files",
        ],
        "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang": [
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/clang",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/driver.conf",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-llc",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/pydir:pydir",
            "native_client/toolchain/linux_x86/pnacl_newlib/lib:libllvm",
            "native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/bin/ld",
        ],
        "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang++": [
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/clang",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/driver.conf",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/pnacl-llc",
            "native_client/toolchain/linux_x86/pnacl_newlib/bin/pydir:pydir",
            "native_client/toolchain/linux_x86/pnacl_newlib/lib:libllvm",
            "native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/bin/ld",
        ],
        "native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang": [
            "native_client/toolchain/linux_x86/pnacl_newlib/lib:libllvm",
        ],
        "native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang++": [
            "native_client/toolchain/linux_x86/pnacl_newlib/lib:libllvm",
        ],
        "native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang": [
            "native_client/toolchain/linux_x86/saigo_newlib/bin:clang",
            "native_client/toolchain/linux_x86/saigo_newlib/lib:libso",
            "native_client/toolchain/linux_x86/saigo_newlib/x86_64-nacl/bin/ld",
        ],
        "native_client/toolchain/linux_x86/saigo_newlib/bin/x86_64-nacl-clang++": [
            "native_client/toolchain/linux_x86/saigo_newlib/bin:clang",
            "native_client/toolchain/linux_x86/saigo_newlib/lib:libso",
            "native_client/toolchain/linux_x86/saigo_newlib/x86_64-nacl/bin/ld",
        ],
        "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-gcc": [
            "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-as",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/lib/gcc/x86_64-nacl:crtbegin",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/libexec/gcc/x86_64-nacl:ccbackend",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/x86_64-nacl/bin/as",
        ],
        "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-g++": [
            "native_client/toolchain/linux_x86/nacl_x86_glibc/bin/x86_64-nacl-as",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/lib/gcc/x86_64-nacl:crtbegin",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/libexec/gcc/x86_64-nacl:ccbackend",
            "native_client/toolchain/linux_x86/nacl_x86_glibc/x86_64-nacl/bin/as",
        ],
    })
    return step_config

nacl = module(
    "nacl",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
