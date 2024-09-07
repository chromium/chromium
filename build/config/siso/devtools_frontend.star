load("@builtin//encoding.star", "json")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./tsc.star", "tsc")

# TODO: crbug.com/1478909 - Specify typescript inputs in GN config.
def __filegroups(ctx):
    return {
        "third_party/devtools-frontend/src/node_modules/typescript:typescript": {
            "type": "glob",
            "includes": ["*"],
        },
        "third_party/devtools-frontend/src/node_modules:node_modules": {
            "type": "glob",
            "includes": ["*.js", "*.json", "*.ts"],
        },
    }

def __step_config(ctx, step_config):
    step_config["input_deps"].update({
        "third_party/devtools-frontend/src/third_party/typescript/ts_library.py": [
            "third_party/devtools-frontend/src/node_modules/typescript:typescript",
            "third_party/devtools-frontend/src/node_modules:node_modules",
        ],
    })

    # TODO: b/308405411 - Enable remote-devtools-frontend-typescript by default.
    if config.get(ctx, "remote-devtools-frontend-typescript"):
        step_config["rules"].extend([
            {
                "name": "devtools-frontend/typescript/ts_library",
                "command_prefix": "python3 ../../third_party/devtools-frontend/src/third_party/typescript/ts_library.py",
                "exclude_input_patterns": [
                    "*.stamp",
                ],
                "remote": True,
                "handler": "devtools_frontend/typescript_ts_library",
                "output_local": True,
                "timeout": "2m",
            },
        ])
    return step_config

def _ts_library(ctx, cmd):
    # Handler for https://crsrc.org/c/third_party/devtools-frontend/src/third_party/typescript/ts_library.py
    # Note that this is a different script from https://crsrc.org/c/tools/typescript/ts_library.py
    deps = []
    sources = []
    tsconfig_path = None
    flag = None
    for i, arg in enumerate(cmd.args):
        if flag != "" and arg.startswith("-"):
            flag = ""
        if arg == "--tsconfig_output_location":
            tsconfig_path = ctx.fs.canonpath(cmd.args[i + 1])
            continue
        if arg in ("--deps", "--sources"):
            flag = arg
            continue
        if flag == "--deps":
            deps.append(arg)
            continue
        if flag == "--sources":
            sources.append(ctx.fs.canonpath(arg))
            continue
    if not tsconfig_path:
        fail("missing --tsconfig_output_location")
    tsconfig = {"files": [], "references": []}
    tsconfig_dir = path.dir(tsconfig_path)
    for s in sources:
        tsconfig["files"].append(path.rel(tsconfig_dir, s))
    for d in deps:
        tsconfig["references"].append({"path": d})
        refpath = path.join(tsconfig_dir, d)
        refdir = path.dir(refpath)

        # TODO: crbug.com/1503020 - Fix devtools_entrypoint to propagate .d.ts output.
        dpath = path.join(refdir, path.base(refdir) + ".d.ts")
        if ctx.fs.exists(dpath):
            sources.append(dpath)

    inputs = tsc.scandeps(ctx, tsconfig_path, tsconfig)

    # Sources and imported files might be located in different dirs. source vs gen.
    # Try to collect the corresponding files in source or gen dir.
    # TODO: crbug.com/1505319 - Fix devtools_module import issues.
    files = {}
    gen_dir = None

    # Infer source files from gen file.
    for f in cmd.inputs + inputs:
        if f.startswith("out/"):
            # Remove out/{subdir}/gen.
            splits = f.split("/", 3)
            if len(splits) < 4:
                continue
            gen_dir = path.join(splits[0], splits[1], splits[2])
            f = splits[3]
            if ctx.fs.exists(f) and not f in files:
                files[f] = True
                continue
            if f.endswith(".js"):
                f = f.removesuffix(".js") + ".d.ts"
                if ctx.fs.exists(f) and not f in files:
                    files[f] = True

    # Infer gen files from source file.
    if gen_dir:
        for f in cmd.inputs + inputs:
            if f.endswith(".ts"):
                f = path.join(gen_dir, f)
                f = f.removesuffix(".ts") + ".d.ts"
                if ctx.fs.exists(f) and not f in files:
                    files[f] = True
            if f.endswith(".js"):
                f = path.join(gen_dir, f)
                f = f.removesuffix(".js") + ".d.ts"
                if ctx.fs.exists(f) and not f in files:
                    files[f] = True

    ctx.actions.fix(inputs = cmd.inputs + inputs + sources + files.keys())

devtools_frontend = module(
    "devtools_frontend",
    step_config = __step_config,
    handlers = {
        "devtools_frontend/typescript_ts_library": _ts_library,
    },
    filegroups = __filegroups,
)
