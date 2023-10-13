# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote calls into reproxy config."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")
load("./config.star", "config")
load("./platform.star", "platform")
load("./rewrapper_cfg.star", "rewrapper_cfg")

def __filegroups(ctx):
    return {}

def __parse_rewrapper_cmdline(ctx, cmd):
    if not "rewrapper" in cmd.args[0]:
        return [], "", False

    # Example command:
    #   ../../buildtools/reclient/rewrapper
    #     -cfg=../../buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg
    #     -exec_root=/path/to/your/chromium/src/
    #     ../../third_party/llvm-build/Release+Asserts/bin/clang++
    #     [rest of clang args]
    # We don't need to care about:
    #   -exec_root: Siso already knows this.
    wrapped_command_pos = -1
    cfg_file = None
    for i, arg in enumerate(cmd.args):
        if i == 0:
            continue
        if arg.startswith("-cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("-cfg="))
            continue
        if not arg.startswith("-"):
            wrapped_command_pos = i
            break
    if wrapped_command_pos < 1:
        fail("couldn't find first non-arg passed to rewrapper for %s" % str(cmd.args))
    return cmd.args[wrapped_command_pos:], cfg_file, True

# TODO(b/278225415): change gn so this wrapper (and by extension this handler) becomes unnecessary.
def __parse_clang_code_coverage_wrapper_cmdline(ctx, cmd):
    # Example command:
    #   python3
    #     ../../build/toolchain/clang_code_coverage_wrapper.py
    #     --target-os=...
    #     --files_to_instrument=...
    #     ../../buildtools/reclient/rewrapper
    #     -cfg=../../buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg
    #     -exec_root=/path/to/your/chromium/src/
    #     ../../third_party/llvm-build/Release+Asserts/bin/clang++
    #     [rest of clang args]
    # We don't need to care about:
    #   most args to clang_code_coverage_wrapper (need --files_to_instrument as tool_input)
    #   -exec_root: Siso already knows this.
    rewrapper_pos = -1
    wrapped_command_pos = -1
    cfg_file = None
    for i, arg in enumerate(cmd.args):
        if i < 2:
            continue
        if rewrapper_pos == -1 and not arg.startswith("-"):
            rewrapper_pos = i
            continue
        if rewrapper_pos > 0 and arg.startswith("-cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("-cfg="))
            continue
        if rewrapper_pos > 0 and not arg.startswith("-"):
            wrapped_command_pos = i
            break
    if rewrapper_pos < 1:
        fail("couldn't find rewrapper in %s" % str(cmd.args))
    if wrapped_command_pos < 1:
        fail("couldn't find first non-arg passed to rewrapper for %s" % str(cmd.args))
    if not cfg_file:
        fail("couldn't find rewrapper cfg file in %s" % str(cmd.args))
    coverage_wrapper_command = cmd.args[:rewrapper_pos] + cmd.args[wrapped_command_pos:]
    clang_command = clang_code_coverage_wrapper.run(ctx, list(coverage_wrapper_command))
    return clang_command, cfg_file

def __rewrite_rewrapper(ctx, cmd):
    # If clang-coverage, needs different handling.
    if len(cmd.args) > 2 and "clang_code_coverage_wrapper.py" in cmd.args[1]:
        args, cfg_file = __parse_clang_code_coverage_wrapper_cmdline(ctx, cmd)
    else:
        # handling for generic rewrapper.
        args, cfg_file, wrapped = __parse_rewrapper_cmdline(ctx, cmd)
        if not wrapped:
            print("command doesn't have rewrapper. %s" % str(cmd.args))
            return
    if not cfg_file:
        fail("couldn't find rewrapper cfg file in %s" % str(cmd.args))
    reproxy_config = rewrapper_cfg.parse(ctx, cfg_file)
    if cmd.outputs[0] == ctx.fs.canonpath("./obj/third_party/abseil-cpp/absl/functional/any_invocable_test/any_invocable_test.o"):
        # need longer timeout for any_invocable_test.o crbug.com/1484474
        reproxy_config.update({
            "exec_timeout": "4m",
        })
    ctx.actions.fix(
        args = args,
        reproxy_config = json.encode(reproxy_config),
    )

def __strip_rewrapper(ctx, cmd):
    # If clang-coverage, needs different handling.
    if len(cmd.args) > 2 and "clang_code_coverage_wrapper.py" in cmd.args[1]:
        args, _ = __parse_clang_code_coverage_wrapper_cmdline(ctx, cmd)
    else:
        args, _, wrapped = __parse_rewrapper_cmdline(ctx, cmd)
        if not wrapped:
            print("command doesn't have rewrapper. %s" % str(cmd.args))
            return
    ctx.actions.fix(args = args)

def __rewrite_action_remote_py(ctx, cmd):
    # Example command:
    #   python3
    #     ../../build/util/action_remote.py
    #     ../../buildtools/reclient/rewrapper
    #     --custom_processor=mojom_parser
    #     --cfg=../../buildtools/reclient_cfgs/python/rewrapper_linux.cfg
    #     --exec_root=/path/to/your/chromium/src/
    #     --input_list_paths=gen/gpu/ipc/common/surface_handle__parser__remote_inputs.rsp
    #     --output_list_paths=gen/gpu/ipc/common/surface_handle__parser__remote_outputs.rsp
    #     python3
    #     ../../mojo/public/tools/mojom/mojom_parser.py
    #     [rest of mojo args]
    # We don't need to care about:
    #   --exec_root: Siso already knows this.
    #   --custom_processor: Used by action_remote.py to apply mojo handling.
    #   --[input,output]_list_paths: We should always use mojo.star for Siso.
    wrapped_command_pos = -1
    cfg_file = None
    for i, arg in enumerate(cmd.args):
        if i < 3:
            continue

        # TODO: b/300046750 - Fix GN args and/or implement input processor.
        if arg == "--custom_processor=mojom_parser":
            print("--custom_processor=mojom_parser is not supported. " +
                  "Running locally. cmd=%s" % " ".join(cmd.args))
            return
        if arg.startswith("--cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("--cfg="))
            continue
        if not arg.startswith("-"):
            wrapped_command_pos = i
            break
    if wrapped_command_pos < 1:
        fail("couldn't find action command in %s" % str(cmd.args))
    ctx.actions.fix(
        args = cmd.args[wrapped_command_pos:],
        reproxy_config = json.encode(rewrapper_cfg.parse(ctx, cfg_file)),
    )

__handlers = {
    "rewrite_rewrapper": __rewrite_rewrapper,
    "strip_rewrapper": __strip_rewrapper,
    "rewrite_action_remote_py": __rewrite_action_remote_py,
}

def __use_remoteexec(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("use_remoteexec") == "true":
            return True
    return False

def __step_config(ctx, step_config):
    # New rules to convert commands calling rewrapper to use reproxy instead.
    new_rules = [
        # Disabling remote should always come first.
        {
            # TODO(b/281663988): missing headers.
            "name": "b281663988/missing-headers",
            "action_outs": [
                "./obj/ui/qt/qt5_shim/qt_shim.o",
                "./obj/ui/qt/qt6_shim/qt_shim.o",
                "./obj/ui/qt/qt5_shim/qt5_shim_moc.o",
                "./obj/ui/qt/qt6_shim/qt6_shim_moc.o",
                "./obj/ui/qt/qt_interface/qt_interface.o",
            ],
            "remote": False,
            "handler": "strip_rewrapper",
        },
        # Handle generic action_remote calls.
        {
            "name": "action_remote",
            "command_prefix": platform.python_bin + " ../../build/util/action_remote.py ../../buildtools/reclient/rewrapper",
            "handler": "rewrite_action_remote_py",
            "remote_command": "python3",
        },
    ]

    # Disable racing on builders since bots don't have many CPU cores.
    # TODO: b/297807325 - Siso wants to handle local execution.
    # However, Reclient's alerts require racing and local fallback to be
    # done on Reproxy side.
    exec_strategy = "racing"
    if config.get(ctx, "builder"):
        exec_strategy = "remote_local_fallback"

    for rule in step_config["rules"]:
        # Replace nacl-clang/clang++ rules without command_prefix, because they will incorrectly match rewrapper.
        # Replace the original step rule with one that only rewrites rewrapper and convert its rewrapper config to reproxy config.
        if rule["name"].find("nacl-clang") >= 0 and not rule.get("command_prefix"):
            new_rule = {
                "name": rule["name"],
                "action": rule["action"],
                "handler": "rewrite_rewrapper",
            }
            new_rules.append(new_rule)
            continue

        # clang will always have rewrapper config when use_remoteexec=true.
        # Remove the native siso handling and replace with custom rewrapper-specific handling.
        # All other rule values are not reused, instead use rewrapper config via handler.
        # (In particular, command_prefix should be avoided because it will be rewrapper.)
        if rule["name"].startswith("clang/") or rule["name"].startswith("clang-cl/"):
            if not rule.get("action"):
                fail("clang rule %s found without action" % rule["name"])

            # TODO(b/294160948): reclient doesn't work well with cros wrapper symlink tricks.
            cros_rule = {
                "name": rule["name"] + "/cros",
                "action": rule["action"],
                "command_prefix": "../../build/cros_cache/",
                "use_remote_exec_wrapper": True,
            }
            new_rules.append(cros_rule)
            new_rule = {
                "name": rule["name"],
                "action": rule["action"],
                "handler": "rewrite_rewrapper",
            }
            new_rules.append(new_rule)
            continue

        # clang-coverage/ is handled by the rewrite_rewrapper handler of clang/{cxx, cc} action rules above, so ignore these rules.
        if rule["name"].startswith("clang-coverage/"):
            continue

        # Add non-remote rules as-is.
        if not rule.get("remote"):
            new_rules.append(rule)
            continue

        # Finally handle remaining remote rules. It's assumed it is enough to only convert native remote config to reproxy config.
        platform_ref = rule.get("platform_ref")
        if platform_ref:
            p = step_config["platforms"].get(platform_ref)
            if not p:
                fail("Rule %s uses undefined platform '%s'" % (rule["name"], platform_ref))
        else:
            p = step_config.get("platforms", {}).get("default")
            if not p:
                fail("Rule %s did not set platform_ref but no default platform exists" % rule["name"])
        rule["reproxy_config"] = {
            "platform": p,
            "labels": {
                "type": "tool",
                "siso_rule": rule["name"],
            },
            "canonicalize_working_dir": rule.get("canonicalize_dir", False),
            "exec_strategy": exec_strategy,
            "exec_timeout": rule.get("timeout", "10m"),
            "reclient_timeout": rule.get("timeout", "10m"),
            "download_outputs": True,
        }
        new_rules.append(rule)

    step_config["rules"] = new_rules
    return step_config

reproxy = module(
    "reproxy",
    enabled = __use_remoteexec,
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
