# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration main entry."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("@builtin//time.star", "time")
load("./backend_config/backend.star", "backend")
load("./blink_all.star", "blink_all")
load("./config.star", "config")
load("./denylist.star", "denylist")
load("./gn_logs.star", "gn_logs")
load("./grit.star", "grit")
load("./linux.star", chromium_linux = "chromium")
load("./mac.star", chromium_mac = "chromium")
load("./mojo.star", "mojo")
load("./platform.star", "platform")
load("./reclient.star", "reclient")
load("./rust.star", "rust")
load("./simple.star", "simple")
load("./typescript_all.star", "typescript_all")
load("./windows.star", chromium_windows = "chromium")

def __setup_python(ctx, step_config):
    for rule in step_config["rules"]:
        if rule.get("remote_command") == platform.remote_python_bin:
            inputs = rule.get("inputs", [])
            if "third_party/cpython3/linux-amd64:cpython3" not in inputs:
                inputs.append("third_party/cpython3/linux-amd64:cpython3")
                rule["inputs"] = inputs
    return step_config

def __disable_remote(ctx, step_config):
    gn_logs_data = gn_logs.read(ctx)
    if gn_logs_data.get("use_remoteexec") == "true":
        return step_config
    for rule in step_config["rules"]:
        rule["remote"] = False
    return step_config

def __unset_timeout(ctx, step_config):
    if not config.get(ctx, "no-remote-timeout"):
        return step_config
    for rule in step_config["rules"]:
        # if no timeout, default is 60m timeout.
        # better to keep longer timeout instead of using shorter timeout.
        timeout = rule.get("timeout")
        if timeout and \
           time.parse_duration(timeout) > time.parse_duration("60m"):
            continue
        rule.pop("timeout", None)
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
        "platforms": backend.platform_properties(ctx),
        "input_deps": {},
        "scandeps": {
            "step_inputs": {
                "excludes": [
                    "*.json",
                    "*.proto",
                    # Evaluating pyc files causes cache invalidation
                    # and potential non determinism with SHA256 generation.
                    "*.pyc",
                    "*.xml",
                ],
            },
        },
        "rules": [],
        # Outputs with known missing deps. They don't fail the build with
        # -missing_deps=error/fatal.
        "missing_deps": {
            "./gen/third_party/devtools-frontend/src/front_end/panels/application/application.js": "crbug.com/556413211",
            "./gen/third_party/devtools-frontend/src/front_end/panels/sources/sources.js": "crbug.com/556926446",
            "./gen/third_party/devtools-frontend/src/front_end/panels/timeline/timeline.js": "crbug.com/556600964",
            "./gen/third_party/devtools-frontend/src/front_end/ui/legacy/components/cookie_table/cookie_table.js": "crbug.com/556881890",
            "./obj/ash/quick_pair/repository/repository/device_address_map.o": "crbug.com/546524333",
            "./obj/ash/quick_pair/repository/repository/device_image_store.o": "crbug.com/546524333",
            "./obj/chrome/browser/ash/guest_os/public/public/guest_os_mount_provider.o": "crbug.com/565544340",
            "./obj/chrome/browser/ui/views/upgrade_notification_controller/upgrade_notification_controller.o": "crbug.com/555387059",
            "./obj/chrome/common/apps/platform_apps/api/api_features/api_features.o": "crbug.com/570385211",
            "./obj/chrome/common/apps/platform_apps/api/permission_features/permission_features.o": "crbug.com/570385211",
            "./obj/chrome/common/controlled_frame/api/api_features/api_features.o": "crbug.com/570385211",
            "./obj/chrome/common/extensions/api/api_features/api_features.o": "crbug.com/570385211",
            "./obj/chrome/common/extensions/api/manifest_features/manifest_features.o": "crbug.com/570385211",
            "./obj/chrome/common/extensions/api/permission_features/permission_features.o": "crbug.com/570385211",
            "./obj/chrome/common/extensions/extension_features_unittest/extension_features_unittest.o": "crbug.com/570385211",
            "./obj/extensions/common/api/api_features/api_features.o": "crbug.com/570385211",
            "./obj/extensions/common/api/behavior_features/behavior_features.o": "crbug.com/570385211",
            "./obj/extensions/common/api/manifest_features/manifest_features.o": "crbug.com/570385211",
            "./obj/extensions/common/api/permission_features/permission_features.o": "crbug.com/570385211",
            "./obj/tools/json_schema_compiler/test/features_compiler_test/features_compiler_test.o": "crbug.com/570385211",
        },
        # Executables sent from Windows host to Linux workers need to set executable bit explicitly.
        # This is necessary for cross platform build actions. e.g. node binary for typescript
        "executables": [
            "third_party/node/linux/node-linux-x64/bin/node",
            "third_party/typescript/linux-amd64/src/lib/tsc",
            "third_party/cpython3/linux-amd64/bin/python3",
        ],
    }
    step_config = blink_all.step_config(ctx, step_config)
    step_config = grit.step_config(ctx, step_config)
    step_config = host.step_config(ctx, step_config)
    step_config = mojo.step_config(ctx, step_config)
    step_config = rust.step_config(ctx, step_config)
    step_config = simple.step_config(ctx, step_config)
    step_config = typescript_all.step_config(ctx, step_config)
    if reclient.enabled(ctx):
        step_config = reclient.step_config(ctx, step_config)

    step_config = denylist.step_config(ctx, step_config)

    step_config = __setup_python(ctx, step_config)
    step_config = __disable_remote(ctx, step_config)
    step_config = __unset_timeout(ctx, step_config)

    filegroups = {}
    filegroups.update(blink_all.filegroups(ctx))
    filegroups.update(host.filegroups(ctx))
    filegroups.update(platform.filegroups(ctx))
    filegroups.update(rust.filegroups(ctx))
    filegroups.update(simple.filegroups(ctx))
    filegroups.update(typescript_all.filegroups(ctx))

    handlers = {}
    handlers.update(blink_all.handlers)
    handlers.update(host.handlers)
    handlers.update(rust.handlers)
    handlers.update(simple.handlers)
    handlers.update(reclient.handlers)
    handlers.update(typescript_all.handlers)

    return module(
        "config",
        step_config = json.encode(step_config),
        filegroups = filegroups,
        handlers = handlers,
    )
