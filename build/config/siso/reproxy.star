# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote calls into reproxy config."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//path.star", "path")
load("@builtin//runtime.star", "runtime")
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
    #     -inputs=build/config/unsafe_buffers_paths.txt
    #     -exec_root=/path/to/your/chromium/src/
    #     ../../third_party/llvm-build/Release+Asserts/bin/clang++
    #     [rest of clang args]
    # We don't need to care about:
    #   -exec_root: Siso already knows this.
    wrapped_command_pos = -1
    cfg_file = None
    skip = ""
    rw_cmd_opts = {}
    for i, arg in enumerate(cmd.args):
        if i == 0:
            continue
        if arg.startswith("-cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("-cfg="))
            continue
        if arg.startswith("-inputs=") or skip == "-inputs":
            rw_cmd_opts["inputs"] = arg.removeprefix("-inputs=").split(",")
            skip = ""
            continue
        if arg == "-inputs":
            skip = arg
            continue
        if not arg.startswith("-"):
            wrapped_command_pos = i
            break
    if wrapped_command_pos < 1:
        fail("couldn't find first non-arg passed to rewrapper from %s" % str(cmd.args))
    if not cfg_file:
        fail("couldn't find rewrapper cfg file from %s" % str(cmd.args))

    # Config options are the lowest prioity.
    rw_opts = rewrapper_cfg.parse(ctx, cfg_file)

    # TODO: Read RBE_* envvars.
    if runtime.os == "windows":
        # Experimenting if longer timeouts resolve slow Windows developer builds. b/335525655
        rw_opts.update({
            "exec_timeout": "4m",
            "reclient_timeout": "8m",
        })
    if runtime.os == "darwin":
        # Mac gets timeouts occasionally on large input uploads (likely due to large invalidations)
        # b/356981080
        rw_opts.update({
            "exec_timeout": "3m",
            "reclient_timeout": "6m",
        })

    # Command line options are the highest priority.
    rw_opts.update(rw_cmd_opts)
    return cmd.args[wrapped_command_pos:], rw_opts, True

def __parse_cros_rewrapper_cmdline(ctx, cmd):
    # fix cros sdk clang command line and extract rewrapper cfg.
    # Example command:
    #   ../../build/cros_cache/chrome-sdk/symlinks/amd64-generic+15629.0.0+target_toolchain/bin/x86_64-cros-linux-gnu-clang++
    #  -MMD -MF obj/third_party/abseil-cpp/absl/base/base/spinlock.o.d
    #  ...
    #  --rewrapper-path /usr/local/google/home/ukai/src/chromium/src/build/args/chromeos/rewrapper_amd64-generic
    #  --rewrapper-cfg ../../buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg
    #  -pipe -march=x86-64 -msse3 ...
    cfg_file = None
    skip = ""
    args = []
    toolchainpath = None
    for i, arg in enumerate(cmd.args):
        if i == 0:
            toolchainpath = path.dir(path.dir(ctx.fs.canonpath(arg)))
            args.append(arg)
            continue
        if skip:
            if skip == "--rewrapper-cfg":
                cfg_file = ctx.fs.canonpath(arg)
            skip = ""
            continue
        if arg in ("--rewrapper-path", "--rewrapper-cfg"):
            skip = arg
            continue
        args.append(arg)
    if not cfg_file:
        fail("couldn't find rewrapper cfg file in %s" % str(cmd.args))
    rwcfg = rewrapper_cfg.parse(ctx, cfg_file)
    inputs = rwcfg.get("inputs", [])
    inputs.extend([
        path.join(toolchainpath, "bin"),
        path.join(toolchainpath, "lib"),
        path.join(toolchainpath, "usr/bin"),
        path.join(toolchainpath, "usr/lib64/clang"),
        # TODO: b/320189180 - Simple Chrome builds should use libraries under usr/lib64.
        # But, Ninja/Reclient also don't use them unexpectedly.
    ])
    rwcfg["inputs"] = inputs
    rwcfg["preserve_symlinks"] = True
    return args, rwcfg

# TODO(b/278225415): change gn so this wrapper (and by extension this handler) becomes unnecessary.
def __parse_clang_code_coverage_wrapper_cmdline(ctx, cmd):
    # Example command:
    #   python3
    #     ../../build/toolchain/clang_code_coverage_wrapper.py
    #     --target-os=...
    #     --files_to_instrument=...
    #     ../../buildtools/reclient/rewrapper
    #     -cfg=../../buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg
    #     -inputs=build/config/unsafe_buffers_paths.txt
    #     -exec_root=/path/to/your/chromium/src/
    #     ../../third_party/llvm-build/Release+Asserts/bin/clang++
    #     [rest of clang args]
    # We don't need to care about:
    #   most args to clang_code_coverage_wrapper (need --files_to_instrument as tool_input)
    #   -exec_root: Siso already knows this.
    rewrapper_pos = -1
    wrapped_command_pos = -1
    cfg_file = None
    skip = None
    rw_ops = {}
    for i, arg in enumerate(cmd.args):
        if i < 2:
            continue
        if rewrapper_pos == -1 and not arg.startswith("-"):
            rewrapper_pos = i
            continue
        if rewrapper_pos > 0 and arg.startswith("-cfg="):
            cfg_file = ctx.fs.canonpath(arg.removeprefix("-cfg="))
            continue
        if arg.startswith("-inputs=") or skip == "-inputs":
            rw_ops["inputs"] = arg.removeprefix("-inputs=").split(",")
            skip = ""
            continue
        if arg == "-inputs":
            skip = arg
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
    if len(clang_command) > 1 and "/chrome-sdk/" in clang_command[0]:
        # TODO: implement cros sdk support under code coverage wrapper
        fail("need to fix handler for cros sdk under code coverage wrapper")
    rw_cfg_opts = rewrapper_cfg.parse(ctx, cfg_file)

    # Command line options have higher priority than the ones in the cfg file.
    rw_cfg_opts.update(rw_ops)
    return clang_command, rw_cfg_opts

def __rewrite_rewrapper(ctx, cmd, use_large = False):
    # If clang-coverage, needs different handling.
    if len(cmd.args) > 2 and "clang_code_coverage_wrapper.py" in cmd.args[1]:
        args, rwcfg = __parse_clang_code_coverage_wrapper_cmdline(ctx, cmd)
    elif len(cmd.args) > 1 and "/chrome-sdk/" in cmd.args[0]:
        args, rwcfg = __parse_cros_rewrapper_cmdline(ctx, cmd)
    else:
        # handling for generic rewrapper.
        args, rwcfg, wrapped = __parse_rewrapper_cmdline(ctx, cmd)
        if not wrapped:
            print("command doesn't have rewrapper. %s" % str(cmd.args))
            return
    if not rwcfg:
        fail("couldn't find rewrapper cfg file in %s" % str(cmd.args))
    if use_large:
        platform = rwcfg.get("platform", {})
        if platform.get("OSFamily") == "Windows":
            # Since there is no large Windows workers, it needs to run locally.
            ctx.actions.fix(args = args)
            return
        if platform:
            action_key = None
            for key in rwcfg["platform"]:
                if key.startswith("label:action_"):
                    action_key = key
                    break
            if action_key:
                rwcfg["platform"].pop(action_key)
        else:
            rwcfg["platform"] = {}
        rwcfg["platform"].update({
            "label:action_large": "1",
        })

        # Some large compiles take longer than the default timeout 2m.
        rwcfg["exec_timeout"] = "4m"
        rwcfg["reclient_timeout"] = "4m"
    ctx.actions.fix(
        args = args,
        reproxy_config = json.encode(rwcfg),
    )

def __rewrite_rewrapper_large(ctx, cmd):
    return __rewrite_rewrapper(ctx, cmd, use_large = True)

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

__handlers = {
    "rewrite_rewrapper": __rewrite_rewrapper,
    "rewrite_rewrapper_large": __rewrite_rewrapper_large,
    "strip_rewrapper": __strip_rewrapper,
}

def __use_reclient(ctx):
    use_remoteexec = False
    use_reclient = None
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("use_remoteexec") == "true":
            use_remoteexec = True
        if gn_args.get("use_reclient") == "false":
            use_reclient = False
    if use_reclient == None:
        use_reclient = use_remoteexec
    return use_reclient

def __step_config(ctx, step_config):
    # New rules to convert commands calling rewrapper to use reproxy instead.
    new_rules = []

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

        # clang cxx/cc/objcxx/objc will always have rewrapper config when use_remoteexec=true.
        # Remove the native siso handling and replace with custom rewrapper-specific handling.
        # All other rule values are not reused, instead use rewrapper config via handler.
        # (In particular, command_prefix should be avoided because it will be rewrapper.)
        if (rule["name"].startswith("clang/cxx") or rule["name"].startswith("clang/cc") or
            rule["name"].startswith("clang-cl/cxx") or rule["name"].startswith("clang-cl/cc") or
            rule["name"].startswith("clang/objc")):
            if not rule.get("action"):
                fail("clang rule %s found without action" % rule["name"])

            new_rule = {
                "name": rule["name"],
                "action": rule["action"],
                "exclude_input_patterns": rule.get("exclude_input_patterns"),
                "handler": "rewrite_rewrapper",
                "input_root_absolute_path": rule.get("input_root_absolute_path"),
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
    enabled = __use_reclient,
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
