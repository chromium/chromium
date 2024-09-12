# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for nasm scandeps.

- https://chromium.googlesource.com/chromium/deps/nasm

gn
- https://chromium.googlesource.com/chromium/deps/nasm/+/refs/heads/main/nasm_assemble.gni

sample command line:
  command = python3 ../../build/gn_run_binary.py nasm -DPIC -felf64 -P ../../third_party/dav1d/config/linux/x64/config.asm -I../../third_party/dav1d/libdav1d/src// -I../../third_party/dav1d/config/linux/x64/ -I./ -I../../ -Igen/ -DSTACK_ALIGNMENT=16 -MD obj/third_party/dav1d/dav1d_asm/${source_name_part}.o.d -o obj/third_party/dav1d/dav1d_asm/${source_name_part}.o ${in}

"""

load("@builtin//struct.star", "module")
load("@builtin//path.star", "path")

def __scan_input(ctx, src, inc_dirs):
    inputs = [src]
    curdir = path.dir(src)
    include_directive_len = len("%include \"")
    for line in str(ctx.fs.read(src)).split("\n"):
        if not line.startswith("%include \""):
            continue
        fname = line[include_directive_len:]
        i = fname.index("\"")
        fname = fname[:i]
        for d in [curdir] + inc_dirs:
            pathname = path.join(d, fname)
            if ctx.fs.exists(pathname):
                inputs.extend(__scan_input(ctx, pathname, inc_dirs))
                break
    return inputs

def __scandeps(ctx, cmd):
    nasm_args = []
    for i, arg in enumerate(cmd.args):
        if path.base(arg) == "nasm":
            nasm_args = cmd.args[i + 1:]
            break
    inc_dirs = []
    skip = False
    flag = ""
    sources = []
    for i, arg in enumerate(nasm_args):
        if flag == "-I":
            inc_dirs.append(ctx.fs.canonpath(arg))
            flag = ""
            continue
        elif flag == "-P":
            sources.append(ctx.fs.canonpath(arg))
            flag = ""
            continue
        elif skip:
            skip = False
            continue
        elif arg == "-o":
            skip = True
            continue
        elif arg == "-MD":
            skip = True
            continue
        elif arg == "-I":
            flag = arg
            continue
        elif arg == "-P":
            flag = arg
            continue
        elif arg.startswith("-I"):
            inc_dirs.append(ctx.fs.canonpath(arg[2:]))
            continue
        elif arg.startswith("-P"):
            sources.append(ctx.fs.canonpath(arg[2:]))
            continue
        elif arg.startswith("-"):
            continue
        sources.append(ctx.fs.canonpath(arg))
    inputs = []
    for src in sources:
        inputs.extend(__scan_input(ctx, src, inc_dirs))
    return inputs

nasm_scandeps = module(
    "nasm_scandeps",
    scandeps = __scandeps,
)
