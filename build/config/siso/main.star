# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration main entry."""

load("@builtin//encoding.star", "json")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")
load("./blink_all.star", "blink_all")
load("./linux.star", chromium_linux = "chromium")
load("./mac.star", chromium_mac = "chromium")
load("./mojo.star", "mojo")
load("./platform.star", "platform")
load("./reproxy.star", "reproxy")
load("./simple.star", "simple")
load("./windows.star", chromium_windows = "chromium")

def __use_large_b289968566(ctx, step_config):
    # TODO(b/289968566): they often faile with exit=137 (OOM?).
    # They need to run on a machine has more memory than the default machine type n2-custom-2-3840
    exit137_list = [
        "./android_clang_arm/obj/third_party/distributed_point_functions/distributed_point_functions/evaluate_prg_hwy.o",
        "./ash_clang_x64/obj/chrome/browser/ash/ash/autotest_private_api.o",
        "./ash_clang_x64/obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
        "./ash_clang_x64/obj/chrome/browser/browser/browser_prefs.o",
        "./ash_clang_x64/obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./obj/chrome/browser/ash/ash/autotest_private_api.o",
        "./obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
        "./obj/chrome/browser/ash/system_web_apps/browser_tests/system_web_app_manager_browsertest.o",
        "./obj/chrome/browser/browser/browser_prefs.o",
        "./obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./obj/chrome/browser/ui/ash/holding_space/browser_tests/holding_space_ui_browsertest.o",
        "./obj/chrome/test/browser_tests/browser_non_client_frame_view_chromeos_browsertest.o",
        "./obj/chrome/test/browser_tests/chrome_shelf_controller_browsertest.o",
        "./obj/chrome/test/browser_tests/device_local_account_browsertest.o",
        "./obj/chrome/test/browser_tests/file_manager_browsertest_base.o",
        "./obj/chrome/test/browser_tests/remote_apps_manager_browsertest.o",
        "./obj/chrome/test/browser_tests/spoken_feedback_browsertest.o",
        "./obj/chrome/test/unit_tests/chrome_browsing_data_remover_delegate_unittest.o",
        "./obj/chrome/test/unit_tests/site_settings_handler_unittest.o",
        "./obj/content/browser/browser/browser_interface_binders.o",
        "./obj/fuchsia_web/runners/cast_runner_integration_tests__exec/cast_runner_integration_test.o",
        "./obj/fuchsia_web/webengine/web_engine_core/frame_impl.o",
    ]
    if runtime.os == "windows":
        exit137_list = [obj.removesuffix(".o") + ".obj" for obj in exit137_list if obj.startswith("./obj/")]

    new_rules = []
    for rule in step_config["rules"]:
        if not rule["name"].endswith("/cxx"):
            new_rules.append(rule)
            continue
        if "action_outs" in rule:
            fail("unexpeced \"action_outs\" in cxx rule %s" % rule["name"])
        r = {}
        r.update(rule)
        r["name"] += "/b289968566/exit-137"
        r["action_outs"] = exit137_list
        r["platform_ref"] = "large"
        new_rules.append(r)
        new_rules.append(rule)
    step_config["rules"] = new_rules
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
    step_config = {
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

    step_config = __use_large_b289968566(ctx, step_config)

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
