# -*- bazel-starlark -*-
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for typescript."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//encoding.star", "json")
load("@builtin//path.star", "path")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./platform.star", "platform")
load("./tsc.star", "tsc")

# TODO: crbug.com/1298825 - fix missing *.d.ts in tsconfig.
__input_deps = {
    "third_party/material_web_components/tsconfig_base.json": [
        "third_party/material_web_components/components-chromium/node_modules:node_modules",
    ],
}

def __step_config(ctx, step_config):
    remote_run = config.get(ctx, "googlechrome")
    step_config["input_deps"].update(typescript_all.input_deps)

    use_input_root_absolute_path = False
    if gn.args(ctx).get("use_javascript_coverage") == "true":
        # crbug.com/345528247: The mismatch of checkout paths between the bot
        # and remote workers breaks js coverage builds.
        use_input_root_absolute_path = True

        # However, the checkout paths cannot be aligned between Windows and
        # Linux workers, so disable remote execution in this case.
        if runtime.os == "windows":
            remote_run = False

    # TODO: crbug.com/1478909 - Specify typescript inputs in GN config.
    step_config["input_deps"].update({
        "tools/typescript/ts_definitions.py": [
            "third_party/node/linux/node-linux-x64/bin/node",
            "third_party/node/node_modules:node_modules",
        ],
        "tools/typescript/ts_library.py": [
            "third_party/node/linux/node-linux-x64/bin/node",
            "third_party/node/node.py",
            "third_party/node/node_modules:node_modules",
        ],
    })
    step_config["rules"].extend([
        {
            "name": "typescript/ts_library",
            "command_prefix": platform.python_bin + " ../../tools/typescript/ts_library.py",
            "indirect_inputs": {
                "includes": [
                    "*.js",
                    "*.ts",
                    "*.json",
                ],
            },
            "remote": remote_run,
            "timeout": "2m",
            "handler": "typescript_ts_library",
            "output_local": True,
            "input_root_absolute_path": use_input_root_absolute_path,
            # Only runs on Linux workers.
            "remote_command": "python3",
        },
        {
            "name": "typescript/ts_definitions",
            "command_prefix": platform.python_bin + " ../../tools/typescript/ts_definitions.py",
            "indirect_inputs": {
                "includes": [
                    "*.ts",  # *.d.ts, *.css.ts, *.html.ts, etc
                    "*.json",
                ],
            },
            "remote": remote_run,
            "timeout": "2m",
            "handler": "typescript_ts_definitions",
            "input_root_absolute_path": use_input_root_absolute_path,
            # Only runs on Linux workers.
            "remote_command": "python3",
        },
    ])
    return step_config

# TODO: crbug.com/1478909 - Specify typescript inputs in GN config.
def __filegroups(ctx):
    return {
        "third_party/node/node_modules:node_modules": {
            "type": "glob",
            "includes": [
                "*.js",
                "*.json",
                "*.ts",
                "tsc",
            ],
        },
        "third_party/material_web_components/components-chromium/node_modules:node_modules": {
            "type": "glob",
            "includes": [
                # This is necessary for
                # gen/third_party/cros-components/tsconfig_cros_components__ts_library.json
                "package.json",
            ],
        },
    }

def _ts_library(ctx, cmd):
    in_files = []
    deps = []
    definitions = []
    path_mappings = []
    flag = ""
    tsconfig_base = None
    for i, arg in enumerate(cmd.args):
        if flag != "" and arg.startswith("-"):
            flag = ""
        if flag == "--in_files":
            in_files.append(arg)
            continue
        if flag == "--definitions":
            definitions.append(arg)
            continue
        if flag == "--deps":
            deps.append(arg)
            continue
        if flag == "--path_mappings":
            path_mappings.append(arg)
            continue
        if arg == "--root_dir":
            root_dir = cmd.args[i + 1]
        if arg == "--gen_dir":
            gen_dir = cmd.args[i + 1]
        if arg == "--out_dir":
            out_dir = cmd.args[i + 1]
        if arg == "--tsconfig_base":
            tsconfig_base = cmd.args[i + 1]
        if arg in ("--in_files", "--definitions", "--deps", "--path_mappings"):
            flag = arg
    root_dir = path.rel(gen_dir, root_dir)
    out_dir = path.rel(gen_dir, out_dir)
    gen_dir = ctx.fs.canonpath(gen_dir)
    tsconfig = {}
    if tsconfig_base:
        tsconfig["extends"] = tsconfig_base
    tsconfig["files"] = [path.join(root_dir, f) for f in in_files]
    tsconfig["files"].extend(definitions)
    tsconfig["references"] = [{"path": dep} for dep in deps]
    tsconfig_path = path.join(gen_dir, "tsconfig.json")
    deps = tsc.scandeps(ctx, tsconfig_path, tsconfig)
    for m in path_mappings:
        _, _, pathname = m.partition("|")
        if pathname.endswith("/*"):
            continue
        deps.append(path.join(gen_dir, pathname))
    ctx.actions.fix(inputs = cmd.inputs + deps)

def _ts_definitions(ctx, cmd):
    js_files = []
    flag = ""
    for i, arg in enumerate(cmd.args):
        if flag != "" and arg.startswith("-"):
            flag = ""
        if flag == "--js_files":
            js_files.append(arg)
            continue
        if flag == "--path_mappings":
            continue
        if arg == "--gen_dir":
            gen_dir = cmd.args[i + 1]
        if arg == "--out_dir":
            out_dir = cmd.args[i + 1]
        if arg == "--root_dir":
            root_dir = cmd.args[i + 1]
        if arg in ("--js_files", "--path_mappings"):
            flag = arg
    tsconfig = json.decode(str(ctx.fs.read("tools/typescript/tsconfig_definitions_base.json")))
    root_dir = path.rel(gen_dir, root_dir)
    out_dir = path.rel(gen_dir, out_dir)
    gen_dir = ctx.fs.canonpath(gen_dir)
    tsconfig["files"] = [path.join(root_dir, f) for f in js_files]
    tsconfig_path = path.join(gen_dir, "tsconfig.definitions.json")
    deps = tsc.scandeps(ctx, tsconfig_path, tsconfig)
    print("_ts_definitions: tsconfig=%s, deps=%s" % (tsconfig, deps))
    ctx.actions.fix(inputs = cmd.inputs + deps)

typescript_all = module(
    "typescript_all",
    handlers = {
        "typescript_ts_library": _ts_library,
        "typescript_ts_definitions": _ts_definitions,
    },
    step_config = __step_config,
    filegroups = __filegroups,
    input_deps = __input_deps,
)
