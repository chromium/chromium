# -*- bazel-starlark -*-
load("@builtin//encoding.star", "json")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./tsc.star", "tsc")

# TODO: crbug.com/1298825 - fix missing *.d.ts in tsconfig.
__input_deps = {
    "third_party/material_web_components/tsconfig_base.json": [
        "third_party/material_web_components/components-chromium/node_modules:node_modules",
    ],
}

# TODO: crbug.com/1478909 - Specify typescript inputs in GN config.
def __filegroups(ctx):
    return {
        "third_party/node/node_modules:node_modules": {
            "type": "glob",
            "includes": ["*.js", "*.cjs", "*.mjs", "*.json", "*.js.flow", "*.ts", "rollup", "terser", "tsc"],
        },
        "third_party/material_web_components/components-chromium/node_modules:node_modules": {
            "type": "glob",
            "includes": ["package.json"],
        },
    }

def _ts_library(ctx, cmd):
    in_files = []
    deps = []
    definitions = []
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
    filegroups = __filegroups,
    input_deps = __input_deps,
)
