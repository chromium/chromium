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
    """Creates a thin archive without a symbol table."""
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

def __ar_entries(ctx, fname, build_dir):
    """Read entries from a thin archive. """

    # TODO: It may take long time to read an entire archive.
    # Is it better to read only the first X bytes?
    lines = str(ctx.fs.read(fname)).splitlines()
    lib_dir = path.rel(build_dir, path.dir(fname))
    ents = []
    if not len(lines):
        print("warning: empty archive. `%s`" % fname)
        return []
    if not lines[0].startswith("!<thin>"):
        print("not thin archive. `%s`" % fname)
        return []
    for l in lines:
        l.strip()
        if l.endswith(".obj/") or l.endswith(".o/"):
            ents.append(path.join(lib_dir, l.removesuffix("/")))
        if l.endswith(".lib/") or l.endswith(".a/"):
            fail("nested archive is not supported, yet. found `%s` in `%s`" % (l, fname))
    return ents

ar = module(
    "ar",
    create = __ar_create,
    entries = __ar_entries,
)
