# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./ar.star", "ar")
load("./config.star", "config")
load("./mac_sdk.star", "mac_sdk")
load("./win_sdk.star", "win_sdk")

def __filegroups(ctx):
    fg = {
        "third_party/libc++/src/include:headers": {
            "type": "glob",
            "includes": ["*"],
            # can't use "*.h", because c++ headers have no extension.
        },
        "third_party/libc++abi/src/include:headers": {
            "type": "glob",
            "includes": ["*.h"],
        },
        # vendor provided headers for libc++.
        "buildtools/third_party/libc++:headers": {
            "type": "glob",
            "includes": [
                "__*",
            ],
        },

        # toolchain root
        # :headers for compiling
        "third_party/llvm-build/Release+Asserts:headers": {
            "type": "glob",
            "includes": [
                "*.h",
                "*.modulemap",
                "bin/clang",
                "bin/clang++",
                "bin/clang-*",  # clang-cl, clang-<ver>
                "*_ignorelist.txt",
                # https://crbug.com/335997052
                "clang_rt.profile*.lib",
            ],
        },
        "third_party/cronet_android_mainline_clang/linux-amd64:headers": {
            "type": "glob",
            "includes": [
                "*.h",
                "*.modulemap",
                "bin/clang*",
            ],
        },
        "third_party/cronet_android_mainline_clang/linux-amd64:link": {
            "type": "glob",
            "includes": [
                "bin/clang*",
                "bin/ld.lld",
                "bin/lld",
                "bin/llvm-nm",
                "bin/llvm-objcopy",
                "bin/llvm-readelf",
                "bin/llvm-readobj",
                "bin/llvm-strip",
                "*.so",
                "*.so.*",
                "*.a",
            ],
        },
    }
    if win_sdk.enabled(ctx):
        fg.update(win_sdk.filegroups(ctx))
    if mac_sdk.enabled(ctx):
        fg.update(mac_sdk.filegroups(ctx))
    return fg

__input_deps = {
    # need this because we use
    # third_party/libc++/src/include:headers,
    # but scandeps doesn't scan `__config` file, which uses
    # `#include <__config_site>`
    # also need `__assertion_handler`. b/321171148
    "third_party/libc++/src/include": [
        "buildtools/third_party/libc++:headers",
    ],
    "third_party/llvm-build/Release+Asserts/bin/clang": [
        "build/config/unsafe_buffers_paths.txt",
    ],
    "third_party/llvm-build/Release+Asserts/bin/clang++": [
        "build/config/unsafe_buffers_paths.txt",
    ],
    "third_party/llvm-build/Release+Asserts/bin/clang-cl": [
        "build/config/unsafe_buffers_paths.txt",
    ],
    "third_party/llvm-build/Release+Asserts/bin/clang-cl.exe": [
        "build/config/unsafe_buffers_paths.txt",
    ],
    "third_party/llvm-build/Release+Asserts/bin/lld-link": [
        "build/config/c++/libc++.natvis",
        "build/win/as_invoker.manifest",
        "build/win/common_controls.manifest",
        "build/win/compatibility.manifest",
        "build/win/require_administrator.manifest",
        "build/win/segment_heap.manifest",
        "remoting/host/win/dpi_aware.manifest",
        "third_party/llvm-build/Release+Asserts/bin/lld",
        "tools/win/DebugVisualizers/blink.natvis",
        "tools/win/DebugVisualizers/chrome.natvis",
    ],
    "third_party/llvm-build/Release+Asserts/bin/lld-link.exe": [
        "build/config/c++/libc++.natvis",
        "build/win/as_invoker.manifest",
        "build/win/common_controls.manifest",
        "build/win/compatibility.manifest",
        "build/win/require_administrator.manifest",
        "build/win/segment_heap.manifest",
        "remoting/host/win/dpi_aware.manifest",
        "third_party/llvm-build/Release+Asserts/bin/lld.exe",
        "tools/win/DebugVisualizers/blink.natvis",
        "tools/win/DebugVisualizers/chrome.natvis",
    ],
    "build/toolchain/gcc_solink_wrapper.py": [
        "build/toolchain/whole_archive.py",
        "build/toolchain/wrapper_utils.py",
    ],
    "build/toolchain/gcc_solink_wrapper.py:link": [
        "build/toolchain/gcc_solink_wrapper.py",
        "build/toolchain/whole_archive.py",
        "build/toolchain/wrapper_utils.py",
    ],
    "build/toolchain/gcc_link_wrapper.py": [
        "build/toolchain/whole_archive.py",
        "build/toolchain/wrapper_utils.py",
    ],
    "build/toolchain/gcc_link_wrapper.py:link": [
        "build/toolchain/gcc_link_wrapper.py",
        "build/toolchain/whole_archive.py",
        "build/toolchain/wrapper_utils.py",
    ],
    "build/toolchain/apple/linker_driver.py:link": [
        "build/toolchain/apple/linker_driver.py",
        "build/toolchain/whole_archive.py",
    ],
    "build/toolchain/apple/solink_driver.py:link": [
        "build/toolchain/apple/linker_driver.py",
        "build/toolchain/apple/solink_driver.py",
        "build/toolchain/whole_archive.py",
    ],
}

def __lld_link(ctx, cmd):
    # Replace thin archives with /start-lib ... /end-lib in rsp file.
    new_lines = []
    for line in str(cmd.rspfile_content).split("\n"):
        new_elems = []
        for elem in line.split(" "):
            # Parse only .lib files.
            if not elem.endswith(".lib"):
                new_elems.append(elem)
                continue

            # Parse files under the out dir.
            fname = ctx.fs.canonpath(elem)
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

            new_elems.append("-start-lib")
            new_elems.extend(ents)
            new_elems.append("-end-lib")
        new_lines.append(" ".join(new_elems))

    ctx.actions.fix(rspfile_content = "\n".join(new_lines))

def __thin_archive(ctx, cmd):
    # TODO: This handler can be used despite remote linking?
    if not config.get(ctx, "remote-link"):
        return
    if "lld-link" in cmd.args[0]:
        if not "/llvmlibthin" in cmd.args:
            print("not thin archive")
            return
    else:
        # check command line to see "-T" and "-S".
        # rm -f obj/third_party/angle/libangle_common.a && "../../third_party/llvm-build/Release+Asserts/bin/llvm-ar" -T -S -r -c -D obj/third_party/angle/libangle_common.a @"obj/third_party/angle/libangle_common.a.rsp"
        if not ("-T" in cmd.args[-1] and "-S" in cmd.args[-1]):
            print("not thin archive without symbol table")
            return

    # create thin archive without symbol table by handler.
    rspfile_content = str(cmd.rspfile_content)
    inputs = []
    for line in rspfile_content.split("\n"):
        for fname in line.split(" "):
            inputs.append(ctx.fs.canonpath(fname))
    data = ar.create(ctx, path.dir(cmd.outputs[0]), inputs)
    ctx.actions.write(cmd.outputs[0], data)
    ctx.actions.exit(exit_status = 0)

__handlers = {
    "lld_link": __lld_link,
    "lld_thin_archive": __thin_archive,
}

clang_all = module(
    "clang_all",
    filegroups = __filegroups,
    input_deps = __input_deps,
    handlers = __handlers,
)
