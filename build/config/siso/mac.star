# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for macOS/iOS."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_mac.star", "clang")
load("./config.star", "config")
load("./typescript_unix.star", "typescript")

def __filegroups(ctx):
    fg = {}
    fg.update(clang.filegroups(ctx))
    fg.update(typescript.filegroups(ctx))
    return fg

def __codesign(ctx, cmd):
    # codesign.py uses the last arguments as bundle.path
    # and it would remove existing files under bundle.path,
    # but siso could not detect such removal, so would cause failure
    # in subsequent steps. https://crbug.com/372628498
    # To capture such removal, specify the bundle path as output,
    # so hashfs.RetrieveUpdateEntriesFromLocal can detect them.
    bundle_path = ctx.fs.canonpath(cmd.args[-1])
    ctx.actions.fix(reconcile_outputdirs = [bundle_path])

def __package_framework(ctx, cmd):
    # package_framework.py updates symlink, but use only stamp file
    # for output.
    # https://source.chromium.org/chromium/chromium/src/+/main:build/config/mac/rules.gni;l=274;bpv=1
    # siso would miss symlink update, so would fail copy_bundle_data
    # in later step.
    framework_dir = ""
    outputs = []
    flag = ""
    for i, arg in enumerate(cmd.args):
        if flag == "--contents" and not arg.startswith("-"):
            outputs.append(path.join(framework_dir, arg))
            continue
        flag = ""
        if arg == "--framework":
            framework_dir = ctx.fs.canonpath(cmd.args[i + 1])
            outputs.append(path.join(framework_dir, "Versions", "Current"))
            continue
        if arg == "--contents":
            flag = arg
    ctx.actions.fix(outputs = cmd.outputs + outputs)

def __download_from_google_storage(ctx, cmd):
    # download_from_google_storage will replace output dir.
    ctx.actions.fix(reconcile_outputdirs = [path.dir(cmd.outputs[0])])

__handlers = {
    "codesign": __codesign,
    "package_framework": __package_framework,
    "download_from_google_storage": __download_from_google_storage,
}
__handlers.update(clang.handlers)
__handlers.update(typescript.handlers)

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config = clang.step_config(ctx, step_config)
    step_config = typescript.step_config(ctx, step_config)
    step_config["rules"].extend([
        {
            "name": "codesign",
            "command_prefix": "python3 ../../build/config/apple/codesign.py ",
            "handler": "codesign",
        },
        {
            "name": "package_framework",
            "command_prefix": "python3 ../../build/config/mac/package_framework.py ",
            "handler": "package_framework",
        },
        {
            "name": "download_from_google_storage",
            "command_prefix": "python3 ../../third_party/depot_tools/download_from_google_storage.py ",
            "handler": "download_from_google_storage",
        },
    ])
    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
