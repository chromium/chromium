# -*- bazel-starlark -*-
load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./typescript_all.star", "typescript_all")

__handlers = {}
__handlers.update(typescript_all.handlers)

def __step_config(ctx, step_config):
    remote_run = True
    step_config["input_deps"].update(typescript_all.input_deps)

    # crbug.com/345528247 - use_javascript_coverage
    # b/348104171: absolute path used in //ash/webui/recorder_app_ui/resources:build_ts?
    use_input_root_absolute_path = True

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
            "command_prefix": "python3 ../../tools/typescript/ts_library.py",
            "indirect_inputs": {
                "includes": [
                    "*.js",
                    "*.ts",
                    "*.json",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            "remote": remote_run,
            "timeout": "2m",
            "handler": "typescript_ts_library",
            "output_local": True,
            "input_root_absolute_path": use_input_root_absolute_path,
        },
        {
            "name": "typescript/ts_definitions",
            "command_prefix": "python3 ../../tools/typescript/ts_definitions.py",
            "indirect_inputs": {
                "includes": [
                    "*.ts",  # *.d.ts, *.css.ts, *.html.ts, etc
                    "*.json",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            "remote": remote_run,
            "timeout": "2m",
            "handler": "typescript_ts_definitions",
            "input_root_absolute_path": use_input_root_absolute_path,
        },
    ])
    return step_config

typescript = module(
    "typescript",
    step_config = __step_config,
    handlers = __handlers,
    filegroups = typescript_all.filegroups,
)
