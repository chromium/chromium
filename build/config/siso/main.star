# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration main entry."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./blink_all.star", "blink_all")
load("./clang_exception.star", "clang_exception")
load("./gn_logs.star", "gn_logs")
load("./linux.star", chromium_linux = "chromium")
load("./mac.star", chromium_mac = "chromium")
load("./mojo.star", "mojo")
load("./platform.star", "platform")
load("./reproxy.star", "reproxy")
load("./simple.star", "simple")
load("./windows.star", chromium_windows = "chromium")

def __disable_remote(ctx, step_config):
    if gn.args(ctx).get("use_remoteexec") == "true":
        return step_config
    for rule in step_config["rules"]:
        rule["remote"] = False
    return step_config

def init(ctx):
    print("runtime: os:%s arch:%s run:%d" % (
        runtime.os,
        runtime.arch,
        runtime.num_cpu,
    ))
    host = {
        "linux": chromium_linux,
        "darwin": chromium_mac,
        "windows": chromium_windows,
    }[runtime.os]
    properties = {}
    for k, v in gn.args(ctx).items():
        properties["gn_args:" + k] = v
    for k, v in gn_logs.read(ctx).items():
        properties["gn_logs:" + k] = v
    step_config = {
        "properties": properties,
        "platforms": {
            "default": {
                "OSFamily": "Linux",
                "container-image": "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:912808c295e578ccde53b0685bcd0d56c15d7a03e819dcce70694bfe3fdab35e",
                "label:action_default": "1",
            },
            # Large workers are usually used for Python actions like generate bindings, mojo generators etc
            # They can run on Linux workers.
            "large": {
                "OSFamily": "Linux",
                "container-image": "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:912808c295e578ccde53b0685bcd0d56c15d7a03e819dcce70694bfe3fdab35e",
                # As of Jul 2023, the action_large pool uses n2-highmem-8 with 200GB of pd-ssd.
                # The pool is intended for the following actions.
                #  - slow actions that can benefit from multi-cores and/or faster disk I/O. e.g. link, mojo, generate bindings etc.
                #  - actions that fail for OOM.
                "label:action_large": "1",
            },
        },
        "input_deps": {},
        "rules": [],
    }
    step_config = blink_all.step_config(ctx, step_config)
    step_config = host.step_config(ctx, step_config)
    step_config = mojo.step_config(ctx, step_config)
    step_config = simple.step_config(ctx, step_config)
    if reproxy.enabled(ctx):
        step_config = reproxy.step_config(ctx, step_config)

    #  Python actions may use an absolute path at the first argument.
    #  e.g. C:/src/depot_tools/bootstrap-2@3_8_10_chromium_26_bin/python3/bin/python3.exe
    #  It needs to set `pyhton3` or `python3.exe` to remote_command.
    for rule in step_config["rules"]:
        if rule["name"].startswith("clang-coverage"):
            # clang_code_coverage_wrapper.run() strips the python wrapper.
            # So it shouldn't set `remote_command: python3`.
            continue

        # On Linux worker, it needs to be `python3` instead of `python3.exe`.
        arg0 = rule.get("command_prefix", "").split(" ")[0].strip("\"")
        if arg0 != platform.python_bin:
            continue
        p = rule.get("reproxy_config", {}).get("platform") or step_config["platforms"].get(rule.get("platform_ref", "default"))
        if not p:
            continue
        if p.get("OSFamily") == "Linux":
            arg0 = arg0.removesuffix(".exe")
        rule["remote_command"] = arg0

    step_config = clang_exception.step_config(ctx, step_config)
    step_config = __disable_remote(ctx, step_config)

    filegroups = {}
    filegroups.update(blink_all.filegroups(ctx))
    filegroups.update(host.filegroups(ctx))
    filegroups.update(simple.filegroups(ctx))

    handlers = {}
    handlers.update(blink_all.handlers)
    handlers.update(host.handlers)
    handlers.update(simple.handlers)
    handlers.update(reproxy.handlers)

    return module(
        "config",
        step_config = json.encode(step_config),
        filegroups = filegroups,
        handlers = handlers,
    )
