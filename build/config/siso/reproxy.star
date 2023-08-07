# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote calls into reproxy config."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./rewrapper_cfg.star", "rewrapper_cfg")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")

__filegroups = {}

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

def __rewrite_rewrapper(ctx, cmd):
    args, cfg_file, wrapped = __parse_rewrapper_cmdline(ctx, cmd)
    if not wrapped:
        return
    if not cfg_file:
        fail("couldn't find rewrapper cfg file in %s" % str(cmd.args))
    ctx.actions.fix(
        args = args,
        reproxy_config = json.encode(rewrapper_cfg.parse(ctx, cfg_file)),
    )

def __strip_rewrapper(ctx, cmd):
    args, _, wrapped = __parse_rewrapper_cmdline(ctx, cmd)
    if not wrapped:
        return
    ctx.actions.fix(args = args)

# TODO(b/278225415): change gn so this wrapper (and by extension this handler) becomes unnecessary.
def __rewrite_clang_code_coverage_wrapper(ctx, cmd):
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

    ctx.actions.fix(
        args = clang_command,
        reproxy_config = json.encode(rewrapper_cfg.parse(ctx, cfg_file)),
    )

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
    "rewrite_clang_code_coverage_wrapper": __rewrite_clang_code_coverage_wrapper,
    "rewrite_action_remote_py": __rewrite_action_remote_py,
}

def __use_remoteexec(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.parse_args(ctx.metadata["args.gn"])
        if gn_args.get("use_remoteexec") == "true":
            return True
    return False

def __step_config(ctx, step_config):
    # New rules to convert commands calling rewrapper to use reproxy instead.
    new_rules = [
        # mojo/mojom_bindings_generator will not always have rewrapper args.
        # Use this rule for commands with rewrapper args, the native remote rule is converted above.
        {
            "name": "mojo/mojom_bindings_generator_rewrapper",
            "action": "mojom_(.*_)?__generator",
            "command_prefix": "python3 ../../build/util/action_remote.py ../../buildtools/reclient/rewrapper --cfg=",
            "handler": "rewrite_action_remote_py",
        },
        # Handle generic action_remote calls.
        {
            "name": "action_remote",
            "command_prefix": "python3 ../../build/util/action_remote.py ../../buildtools/reclient/rewrapper",
            "handler": "rewrite_action_remote_py",
        },
    ]

    for rule in step_config["rules"]:
        # mojo/mojom_parser will always have rewrapper config when use_remoteexec=true.
        # Mutate the original step rule to rewrite rewrapper and convert its rewrapper config to reproxy config.
        # Stop handling the rule so that it's not modified below.
        # TODO(b/292838933): Implement mojom_parser processor in Starlark?
        if rule["name"] == "mojo/mojom_parser":
            rule.update({
                "command_prefix": "python3 ../../build/util/action_remote.py ../../buildtools/reclient/rewrapper --custom_processor=mojom_parser",
                "handler": "rewrite_action_remote_py",
            })
            new_rules.insert(0, rule)
            continue

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
        if rule["name"].startswith("clang/") or rule["name"].startswith("clang-cl/"):
            if not rule.get("action"):
                fail("clang rule %s found without action" % rule["name"])
            new_rule = {
                "name": rule["name"],
                "action": rule["action"],
                "handler": "rewrite_rewrapper",
            }
            new_rules.append(new_rule)
            continue

        # clang-coverage will always have rewrapper config when use_remoteexec=true.
        # Remove the native siso handling and replace with custom rewrapper-specific handling.
        # All other rule values are not reused, instead use rewrapper config via handler.
        # TODO(b/278225415): change gn so this wrapper (and by extension these rules) become unnecessary.
        if rule["name"].startswith("clang-coverage"):
            if rule["command_prefix"].find("../../build/toolchain/clang_code_coverage_wrapper.py") < 0:
                fail("clang-coverage rule %s found without clang_code_coverage_wrapper.py in command_prefix" % rule["name"])
            new_rule = {
                "name": rule["name"],
                "command_prefix": rule["command_prefix"],
                "handler": "rewrite_clang_code_coverage_wrapper",
            }
            # Insert clang-coverage/ rules at the top.
            # They are more specific than reproxy clang/ rules, therefore should not be placed after.
            new_rules.insert(0, new_rule)
            continue

        # Other rules where it's enough to only convert native remote config to reproxy config.
        if not rule.get("remote"):
            continue
        platform_ref = rule.get("platform_ref")
        if platform_ref:
            platform = step_config["platforms"].get(platform_ref)
            if not platform:
                fail("Rule %s uses undefined platform '%s'" % (rule["name"], platform_ref))
        else:
            platform = step_config.get("platforms", {}).get("default")
            if not platform:
                fail("Rule %s did not set platform_ref but no default platform exists" % rule["name"])
        rule["reproxy_config"] = {
            "platform": platform,
            "labels": {
                "type": "tool",
            },
            "inputs": rule.get("inputs", []),
            "canonicalize_working_dir": rule.get("canonicalize_dir", False),
            "exec_strategy": "remote",
            "exec_timeout": rule.get("timeout", "10m"),
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
