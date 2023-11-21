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
    remote_run = True

    step_config["input_deps"].update({
        "third_party/devtools-frontend/src/third_party/typescript/ts_library.py": [
            "third_party/devtools-frontend/src/node_modules/typescript:typescript",
            "third_party/devtools-frontend/src/node_modules:node_modules",
        ],
    })

    step_config["rules"].extend([
        {
            "name": "devtools-frontend/typescript/ts_library",
            "command_prefix": "python3 ../../third_party/devtools-frontend/src/third_party/typescript/ts_library.py",
            # TODO: b/308405411 - Support more actions. blocked on crbug.com/1503020
            "action": "__third_party_devtools-frontend_src_front_end_third_party_.*",
            "exclude_input_patterns": [
                "*.stamp",
            ],
            # TODO: crbug.com/1503020 - Fix devtools_entrypoint to propagate d.ts output.
            "outputs_map": {
                "./gen/third_party/devtools-frontend/src/front_end/third_party/i18n/i18n-tsconfig.json": {
                    "inputs": [
                        "./gen/third_party/devtools-frontend/src/front_end/third_party/intl-messageformat/intl-messageformat.d.ts",
                    ],
                },
                "./gen/third_party/devtools-frontend/src/front_end/third_party/diff/diff-tsconfig.json": {
                    "inputs": [
                        "./gen/third_party/devtools-frontend/src/front_end/core/common/common.d.ts",
                    ],
                },
            },
            "remote": remote_run,
            "handler": "devtools_frontend/typescript_ts_library",
            "output_local": True,
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
    inputs = tsc.scandeps(ctx, tsconfig_path, tsconfig)
    ctx.actions.fix(inputs = cmd.inputs + inputs + sources)

devtools_frontend = module(
    "devtools_frontend",
    step_config = __step_config,
    handlers = {
        "devtools_frontend/typescript_ts_library": _ts_library,
    },
    filegroups = __filegroups,
)
