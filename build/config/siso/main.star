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
        # Fallback happens with follwoing args.gn (try/linux-chromeos-rel-compilator).
        # Fallback may happen in other build config too.
        # also_build_lacros_chrome = true
        # dcheck_always_on = true
        # enable_backup_ref_ptr_feature_flag = true
        # enable_dangling_raw_ptr_checks = true
        # enable_dangling_raw_ptr_feature_flag = true
        # ffmpeg_branding = "ChromeOS"
        # is_component_build = false
        # is_debug = false
        # proprietary_codecs = true
        # symbol_level = 0
        # target_os = "chromeos"
        # use_cups = true
        # use_remoteexec = true
        "./lacros_clang_x64/obj/chrome/browser/browser/chrome_content_browser_client.o",
        "./lacros_clang_x64/obj/content/browser/browser/browser_interface_binders.o",
        "./obj/chrome/browser/ash/ash/autotest_private_api.o",
        "./obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
        "./obj/chrome/browser/ash/ash/webui_login_view.o",
        "./obj/chrome/browser/ash/system_web_apps/apps/browser_tests/media_app_integration_browsertest.o",
        "./obj/chrome/browser/ash/system_web_apps/browser_tests/system_web_app_manager_browsertest.o",
        "./obj/chrome/browser/ash/unit_tests/wizard_controller_unittest.o",
        "./obj/chrome/browser/browser/browser_prefs.o",
        "./obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./obj/chrome/browser/browser/chrome_content_browser_client.o",
        "./obj/chrome/browser/ui/ash/holding_space/browser_tests/holding_space_ui_browsertest.o",
        "./obj/chrome/test/browser_tests/browser_non_client_frame_view_chromeos_browsertest.o",
        "./obj/chrome/test/browser_tests/chrome_shelf_controller_browsertest.o",
        "./obj/chrome/test/browser_tests/device_local_account_browsertest.o",
        "./obj/chrome/test/browser_tests/file_manager_browsertest_base.o",
        "./obj/chrome/test/browser_tests/full_restore_app_launch_handler_browsertest.o",
        "./obj/chrome/test/browser_tests/remote_apps_manager_browsertest.o",
        "./obj/chrome/test/browser_tests/safe_browsing_blocking_page_test.o",
        "./obj/chrome/test/browser_tests/spoken_feedback_browsertest.o",
        "./obj/chrome/test/interactive_ui_tests/local_card_migration_uitest.o",
        "./obj/chrome/test/interactive_ui_tests/system_web_app_interactive_uitest.o",
        "./obj/chrome/test/interactive_ui_tests/tab_drag_controller_interactive_uitest.o",
        "./obj/chrome/test/test_support_ui/web_app_integration_test_driver.o",
        "./obj/chrome/test/unit_tests/chrome_browsing_data_remover_delegate_unittest.o",
        "./obj/chrome/test/unit_tests/chrome_shelf_controller_unittest.o",
        "./obj/content/browser/browser/browser_interface_binders.o",

        # Fallback with unknown build configs.
        "./android_clang_arm/obj/content/browser/browser/browser_interface_binders.o",
        "./android_clang_arm/obj/third_party/distributed_point_functions/distributed_point_functions/evaluate_prg_hwy.o",
        "./ash_clang_x64/obj/chrome/browser/ash/ash/autotest_private_api.o",
        "./ash_clang_x64/obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
        "./ash_clang_x64/obj/chrome/browser/ash/ash/user_session_manager.o",
        "./ash_clang_x64/obj/chrome/browser/browser/browser_prefs.o",
        "./ash_clang_x64/obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./ash_clang_x64/obj/chrome/browser/browser/chrome_content_browser_client.o",
        "./ash_clang_x64/obj/chrome/test/test_support_ui/offer_notification_bubble_views_test_base.o",
        "./ash_clang_x64/obj/chrome/test/test_support_ui/web_app_integration_test_driver.o",
        "./ash_clang_x64/obj/content/browser/browser/browser_interface_binders.o",
        "./obj/ash_clang_x64/chrome/test/test_support_ui/offer_notification_bubble_views_test_base.o",
        "./obj/chrome/browser/ash/ash/user_session_manager.o",
        "./obj/chrome/browser/ash/system_web_apps/apps/browser_tests/personalization_app_time_of_day_browsertest.o",
        "./obj/chrome/browser/ash/system_web_apps/apps/browser_tests/personalization_app_wallpaper_daily_refresh_browsertest.o",
        "./obj/chrome/browser/ash/system_web_apps/apps/browser_tests/projector_app_integration_browsertest.o",
        "./obj/chrome/browser/ash/test_support/oobe_screens_utils.o",
        "./obj/chrome/browser/autofill/interactive_ui_tests/autofill_interactive_uitest.o",
        "./obj/chrome/browser/web_applications/web_applications_browser_tests/isolated_web_app_policy_manager_ash_browsertest.o",
        "./obj/chrome/test/browser_tests/app_list_client_impl_browsertest.o",
        "./obj/chrome/test/browser_tests/ash_hud_login_browsertest.o",
        "./obj/chrome/test/browser_tests/assistant_optin_flow_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/autotest_private_apitest.o",
        "./obj/chrome/test/browser_tests/browser_non_client_frame_view_browsertest.o",
        "./obj/chrome/test/browser_tests/capture_mode_browsertest.o",
        "./obj/chrome/test/browser_tests/configuration_based_oobe_browsertest.o",
        "./obj/chrome/test/browser_tests/consolidated_consent_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/consumer_update_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/demo_setup_browsertest.o",
        "./obj/chrome/test/browser_tests/devtools_browsertest.o",
        "./obj/chrome/test/browser_tests/display_size_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/drive_pinning_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/enrollment_embedded_policy_server_browsertest.o",
        "./obj/chrome/test/browser_tests/enterprise_remote_apps_apitest.o",
        "./obj/chrome/test/browser_tests/error_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/existing_user_controller_browsertest.o",
        "./obj/chrome/test/browser_tests/file_manager_policy_browsertest.o",
        "./obj/chrome/test/browser_tests/file_tasks_browsertest.o",
        "./obj/chrome/test/browser_tests/hid_detection_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/kiosk_browsertest.o",
        "./obj/chrome/test/browser_tests/lacros_data_migration_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/login_browsertest.o",
        "./obj/chrome/test/browser_tests/login_manager_test.o",
        "./obj/chrome/test/browser_tests/login_ui_browsertest.o",
        "./obj/chrome/test/browser_tests/login_ui_shelf_visibility_browsertest.o",
        "./obj/chrome/test/browser_tests/management_transition_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/marketing_opt_in_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/metadata_processor_ash_browsertest.o",
        "./obj/chrome/test/browser_tests/minimum_version_policy_handler_browsertest.o",
        "./obj/chrome/test/browser_tests/notification_display_client_browsertest.o",
        "./obj/chrome/test/browser_tests/oauth2_browsertest.o",
        "./obj/chrome/test/browser_tests/offline_login_test_mixin.o",
        "./obj/chrome/test/browser_tests/oobe_base_test.o",
        "./obj/chrome/test/browser_tests/oobe_browsertest.o",
        "./obj/chrome/test/browser_tests/oobe_interactive_ui_test.o",
        "./obj/chrome/test/browser_tests/oobe_metrics_browsertest.o",
        "./obj/chrome/test/browser_tests/policy_certs_browsertest.o",
        "./obj/chrome/test/browser_tests/pwa_install_view_browsertest.o",
        "./obj/chrome/test/browser_tests/quick_start_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/save_card_bubble_views_browsertest.o",
        "./obj/chrome/test/browser_tests/scalable_iph_browsertest.o",
        "./obj/chrome/test/browser_tests/session_login_browsertest.o",
        "./obj/chrome/test/browser_tests/sync_consent_browsertest.o",
        "./obj/chrome/test/browser_tests/theme_selection_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/touchpad_scroll_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/update_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/usertype_by_devicetype_metrics_provider_browsertest.o",
        "./obj/chrome/test/browser_tests/web_view_browsertest.o",
        "./obj/chrome/test/browser_tests/webview_login_browsertest.o",
        "./obj/chrome/test/browser_tests/welcome_screen_browsertest.o",
        "./obj/chrome/test/browser_tests/wizard_controller_browsertest.o",
        "./obj/chrome/test/interactive_ui_tests/iban_bubble_view_uitest.o",
        "./obj/chrome/test/interactive_ui_tests/login_manager_test.o",
        "./obj/chrome/test/test_support_ui/offer_notification_bubble_views_test_base.o",
        "./obj/chrome/test/unit_tests/site_settings_handler_unittest.o",
        "./obj/fuchsia_web/runners/cast_runner_exe/main.o",
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

        # use `_large` variant of platform if it doesn't use default platform,
        # i.e. mac/win case.
        if "platform_ref" in r:
            r["platform_ref"] = r["platform_ref"] + "_large"
        else:
            r["platform_ref"] = "large"
        if r.get("handler") == "rewrite_rewrapper":
            r["handler"] = "rewrite_rewrapper_large"
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
