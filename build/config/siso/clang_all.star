# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./ar.star", "ar")
load("./config.star", "config")
load("./gn_logs.star", "gn_logs")
load("./mac_sdk.star", "mac_sdk")
load("./win_sdk.star", "win_sdk")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")

def __clang_plugin_configs(ctx):
    configs = [
        "build/config/unsafe_buffers_paths.txt",
        "build/config/warning_suppression.txt",
        # crbug.com/418842344: Angle, PDFium use a different plugin config.
        "unsafe_buffers_paths.txt",
    ]

    if "args.gn" in ctx.metadata and gn.args(ctx).get("sanitizer_coverage_skip_stdlib_and_absl"):
        configs += ["build/config/sanitizers/ignorelist_stdlib_and_absl.txt"]
    return configs

def __check_crash_diagnostics(ctx, args):
    # If multiple -fcrash-diagnostics-dir flags are provided, clang uses the last one.
    crash_dir = None
    skip = False
    for i, arg in enumerate(args):
        if skip:
            skip = False
            continue
        if arg.startswith("-fcrash-diagnostics-dir="):
            crash_dir = arg.removeprefix("-fcrash-diagnostics-dir=")
        elif arg == "-fcrash-diagnostics-dir" and i + 1 < len(args):
            crash_dir = args[i + 1]
            skip = True

    if crash_dir:
        if path.isabs(crash_dir):
            # RBE requires relative paths for output directories.
            # If the crash dir is absolute (e.g. /tmp/...), we can't capture it easily.
            # For now, just skip it to avoid build failures.
            return
        crash_dir = ctx.fs.canonpath(crash_dir)
        ctx.actions.fix(auxiliary_log_output_dirs = [crash_dir])

def __filegroups(ctx):
    gn_logs_data = gn_logs.read(ctx)

    # source_root is absolute path of chromium source top directory ("//"),
    # set only for CrOS's chroot builds that use rbe_exec_root="/".
    root = gn_logs_data.get("source_root", "")

    fg = {
        path.join(root, "third_party/libc++/src/include") + ":headers": {
            "type": "glob",
            "includes": ["*"],
            # can't use "*.h", because c++ headers have no extension.
        },
        path.join(root, "third_party/libc++abi/src/include") + ":headers": {
            "type": "glob",
            "includes": ["*.h"],
        },
        # vendor provided headers for libc++.
        path.join(root, "buildtools/third_party/libc++") + ":headers": {
            "type": "glob",
            "includes": [
                "__*",
            ],
        },

        # toolchain root
        # :headers for compiling
        path.join(root, "third_party/llvm-build/Release+Asserts") + ":headers": {
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
    }
    if win_sdk.enabled(ctx):
        fg.update(win_sdk.filegroups(ctx))
    if mac_sdk.enabled(ctx):
        fg.update(mac_sdk.filegroups(ctx))
    return fg

def __input_deps(ctx):
    build_dir = ctx.fs.canonpath(".")
    clang_plugin_configs = __clang_plugin_configs(ctx)

    return {
        # need this because we use
        # third_party/libc++/src/include:headers,
        # but scandeps doesn't scan `__config` file, which uses
        # `#include <__config_site>`
        # also need `__assertion_handler`. b/321171148
        "third_party/libc++/src/include": [
            "buildtools/third_party/libc++:headers",
        ],
        # This is necessary for modules build where libc++ headers are copied to build directory.
        path.join(build_dir, "gen/third_party/libc++/src/include") + ":headers": [
            path.join(build_dir, "gen/third_party/libc++/src/include/module.modulemap"),
            path.join(build_dir, "phony/buildtools/third_party/libc++/copy_custom_headers") + ":inputs",
            path.join(build_dir, "phony/buildtools/third_party/libc++/copy_libcxx_headers") + ":inputs",
        ],
        "third_party/llvm-build/Release+Asserts/bin/clang": clang_plugin_configs,
        "third_party/llvm-build/Release+Asserts/bin/clang++": clang_plugin_configs,
        "third_party/llvm-build/Release+Asserts/bin/clang-cl": clang_plugin_configs,
        "third_party/llvm-build/Release+Asserts/bin/clang-cl.exe": clang_plugin_configs,
        "third_party/llvm-build/Release+Asserts/bin/lld-link": [
            "build/config/c++/libc++.natvis",
            "build/win/as_invoker.manifest",
            "build/win/common_controls.manifest",
            "build/win/compatibility.manifest",
            "build/win/require_administrator.manifest",
            "build/win/segment_heap.manifest",
            "remoting/host/win/dpi_aware.manifest",
            "third_party/llvm-build/Release+Asserts/bin/lld",
            "tools/win/DebugVisualizers/absl.natvis",
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
            "tools/win/DebugVisualizers/absl.natvis",
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

def __compile(ctx, cmd):
    __check_crash_diagnostics(ctx, cmd.args)

def __compile_coverage(ctx, cmd):
    clang_command = clang_code_coverage_wrapper.run(ctx, list(cmd.args))
    __check_crash_diagnostics(ctx, clang_command)
    ctx.actions.fix(args = clang_command)

__handlers = {
    "clang_compile": __compile,
    "clang_compile_coverage": __compile_coverage,
    "lld_link": __lld_link,
    "lld_thin_archive": __thin_archive,
}

clang_all = module(
    "clang_all",
    filegroups = __filegroups,
    input_deps = __input_deps,
    handlers = __handlers,
)
