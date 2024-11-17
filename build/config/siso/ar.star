# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for ar."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

# https://en.wikipedia.org/wiki/Ar_(Unix)

def __file_header(fname, size):
    fid = fname + " " * 16
    header = fid[:16]
    if fname == "//":
        header += " " * 12  # file modification timestamp
        header += " " * 6  # owner id
        header += " " * 6  # group id
        header += " " * 8  # file mode
    else:
        header += "0" + " " * 11  # file modification timestamp
        header += "0" + " " * 5  # owner id
        header += "0" + " " * 5  # group id
        header += "644" + " " * 5  # file mode
    s = ("%d" % size) + " " * 10
    header += s[:10]  # file size
    header += "\x60\n"  # header trailer string
    return header

def __ref_fname(offset, fname):
    i = offset[fname]
    return "/%d" % i

def __padding(data):
    if len(data) % 2 == 0:
        return data
    return data + "\n"

def __ar_create(ctx, wd, ins):
    data = "!<thin>\n"
    offset = {}
    content = ""
    for fname in ins:
        offset[fname] = len(content)
        content += path.rel(wd, fname) + "/\n"
    content = __padding(content)
    data += __file_header("//", len(content))
    data += content
    for fname in ins:
        size = ctx.fs.size(fname)
        if size:
            data += __file_header(__ref_fname(offset, fname), size)
    return bytes(data)

ar = module(
    "ar",
    create = __ar_create,
)
