# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for linux."""

load("@builtin//struct.star", "module")
load("./android.star", "android")
load("./clang_linux.star", "clang")
load("./config.star", "config")
load("./cros.star", "cros")
load("./devtools_frontend.star", "devtools_frontend")
load("./nacl_linux.star", "nacl")
load("./nasm_linux.star", "nasm")
load("./proto_linux.star", "proto")
load("./reproxy.star", "reproxy")
load("./rust_linux.star", "rust")
load("./typescript_unix.star", "typescript")
load("./v8.star", "v8")

def __filegroups(ctx):
    fg = {}
    fg.update(android.filegroups(ctx))
    fg.update(clang.filegroups(ctx))
    fg.update(cros.filegroups(ctx))
    fg.update(devtools_frontend.filegroups(ctx))
    fg.update(nacl.filegroups(ctx))
    fg.update(nasm.filegroups(ctx))
    fg.update(proto.filegroups(ctx))
    fg.update(rust.filegroups(ctx))
    fg.update(typescript.filegroups(ctx))
    fg["third_party/perfetto/python:python"] = {
        "type": "glob",
        "includes": ["*.py"],
    }
    fg["third_party/perfetto/src/trace_processor:trace_processor"] = {
        "type": "glob",
        "includes": ["*.py"],
    }
    return fg

__handlers = {}
__handlers.update(android.handlers)
__handlers.update(clang.handlers)
__handlers.update(cros.handlers)
__handlers.update(devtools_frontend.handlers)
__handlers.update(nacl.handlers)
__handlers.update(nasm.handlers)
__handlers.update(proto.handlers)
__handlers.update(rust.handlers)
__handlers.update(typescript.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)

    if android.enabled(ctx):
        step_config = android.step_config(ctx, step_config)

    # nacl step config should be added before clang step config for link step
    # rules.
    step_config = nacl.step_config(ctx, step_config)

    step_config = clang.step_config(ctx, step_config)
    step_config = cros.step_config(ctx, step_config)
    step_config = devtools_frontend.step_config(ctx, step_config)
    step_config = nasm.step_config(ctx, step_config)
    step_config = proto.step_config(ctx, step_config)
    step_config = rust.step_config(ctx, step_config)
    step_config = typescript.step_config(ctx, step_config)
    step_config = v8.step_config(ctx, step_config)

    step_config["rules"].extend([
        {
            "name": "write_buildflag_header",
            "command_prefix": "python3 ../../build/write_buildflag_header.py",
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "write_build_date_header",
            "command_prefix": "python3 ../../base/write_build_date_header.py",
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "version_py",
            "command_prefix": "python3 ../../build/util/version.py",
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "perfetto/touch_file",
            "command_prefix": "python3 ../../third_party/perfetto/tools/touch_file.py",
            "remote": config.get(ctx, "cog"),
            "replace": True,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "perfetto/write_buildflag_header",
            "command_prefix": "python3 ../../third_party/perfetto/gn/write_buildflag_header.py",
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "perfetto/gen_tp_table_headers",
            "command_prefix": "python3 ../../third_party/perfetto/tools/gen_tp_table_headers.py",
            "inputs": [
                "third_party/perfetto/python:python",
                "third_party/perfetto/src/trace_processor:trace_processor",
            ],
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "perfetto/gen_cc_proto_descriptor",
            "command_prefix": "python3 ../../third_party/perfetto/tools/gen_cc_proto_descriptor.py",
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            # b/331716896: local fails due to link(2) error.
            "name": "generate_fontconfig_cache",
            "command_prefix": "python3 ../../build/gn_run_binary.py generate_fontconfig_caches",
            "remote": config.get(ctx, "cog"),
            "canonicalize_dir": True,
            "timeout": "2m",
        },
    ])

    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
