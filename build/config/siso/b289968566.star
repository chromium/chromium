# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""workaround for b/289968566. they often faile with exit=137 (OOM?)."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")

def __step_config(ctx, step_config):
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
        "./lacros_clang_x64/obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./lacros_clang_x64/obj/chrome/browser/browser/chrome_content_browser_client.o",
        "./lacros_clang_x64/obj/content/browser/browser/browser_interface_binders.o",
        "./lacros_clang_x64/obj/content/browser/browser/render_frame_host_impl.o",
        "./lacros_clang_x64/obj/content/browser/browser/web_contents_impl.o",
        "./lacros_clang_x64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        "./lacros_clang_x64/obj/third_party/blink/public/mojom/mojom_platform/speech_recognition_grammar.mojom.o",
        "./obj/chrome/browser/ash/ash/autotest_private_api.o",
        "./obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
        "./obj/chrome/browser/ash/ash/user_session_manager.o",
        "./obj/chrome/browser/ash/ash/webui_login_view.o",
        "./obj/chrome/browser/ash/system_web_apps/apps/browser_tests/media_app_integration_browsertest.o",
        "./obj/chrome/browser/ash/system_web_apps/browser_tests/system_web_app_manager_browsertest.o",
        "./obj/chrome/browser/ash/unit_tests/wizard_controller_unittest.o",
        "./obj/chrome/browser/browser/browser_prefs.o",
        "./obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./obj/chrome/browser/browser/chrome_browser_main_extra_parts_profiles.o",
        "./obj/chrome/browser/browser/chrome_content_browser_client.o",
        "./obj/chrome/browser/browser/render_view_context_menu.o",
        "./obj/chrome/browser/ui/ash/holding_space/browser_tests/holding_space_ui_browsertest.o",
        "./obj/chrome/test/browser_tests/browser_non_client_frame_view_chromeos_browsertest.o",
        "./obj/chrome/test/browser_tests/chrome_shelf_controller_browsertest.o",
        "./obj/chrome/test/browser_tests/device_local_account_browsertest.o",
        "./obj/chrome/test/browser_tests/file_manager_browsertest_base.o",
        "./obj/chrome/test/browser_tests/file_tasks_browsertest.o",
        "./obj/chrome/test/browser_tests/full_restore_app_launch_handler_browsertest.o",
        "./obj/chrome/test/browser_tests/safe_browsing_blocking_page_test.o",
        "./obj/chrome/test/browser_tests/scalable_iph_browsertest.o",
        "./obj/chrome/test/interactive_ui_tests/local_card_migration_uitest.o",
        "./obj/chrome/test/interactive_ui_tests/system_web_app_interactive_uitest.o",
        "./obj/chrome/test/interactive_ui_tests/tab_drag_controller_interactive_uitest.o",
        "./obj/chrome/test/test_support_ui/web_app_integration_test_driver.o",
        "./obj/chrome/test/unit_tests/chrome_browsing_data_remover_delegate_unittest.o",
        "./obj/chrome/test/unit_tests/chrome_compose_client_unittest.o",
        "./obj/chrome/test/unit_tests/chrome_shelf_controller_unittest.o",
        "./obj/chrome/test/unit_tests/render_view_context_menu_unittest.o",
        "./obj/content/browser/browser/browser_interface_binders.o",

        # Fallback happens with follwoing args.gn (try/linux-lacros-rel-compilator).
        # Fallback may happen in other build config too.
        # also_build_ash_chrome = true
        # chromeos_is_browser_only = true
        # dcheck_always_on = true
        # is_clang = true
        # is_component_build = false
        # is_debug = false
        # symbol_level = 0
        # target_os = "chromeos"
        # use_cups = true
        # use_remoteexec = true
        "./ash_clang_x64/obj/chrome/browser/ash/ash/autotest_private_api.o",
        "./ash_clang_x64/obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
        "./ash_clang_x64/obj/chrome/browser/browser/browser_prefs.o",
        "./ash_clang_x64/obj/chrome/browser/browser/chrome_browser_interface_binders.o",
        "./ash_clang_x64/obj/chrome/browser/browser/chrome_content_browser_client.o",
        "./ash_clang_x64/obj/content/browser/browser/browser_interface_binders.o",

        # Fallback happens with follwoing args.gn (try/android_compile_dbg).
        # Fallback may happen in other build config too.
        # debuggable_apks = false
        # ffmpeg_branding = "Chrome"
        # is_component_build = true
        # is_debug = true
        # proprietary_codecs = true
        # symbol_level = 0
        # target_cpu = "arm64"
        # target_os = "android"
        # use_remoteexec = true
        "./android_clang_arm/obj/content/browser/browser/browser_interface_binders.o",
        "./obj/chrome/test/unit_tests__library/chrome_browsing_data_remover_delegate_unittest.o",
        "./obj/content/test/content_browsertests__library/fenced_frame_browsertest.o",
        "./obj/content/test/content_unittests__library/ad_auction_service_impl_unittest.o",
        "./obj/content/test/content_unittests__library/auction_runner_unittest.o",

        # Fallback happens with follwoing args.gn (try/fuchsia-x64-cast-receiver-rel).
        # Fallback may happen in other build config too.
        # cast_streaming_enable_remoting = true
        # chrome_pgo_phase = 0
        # dcheck_always_on = true
        # enable_cast_receiver = true
        # enable_dav1d_decoder = false
        # enable_hidpi = false
        # enable_libaom = false
        # enable_library_cdms = false
        # enable_logging_override = true
        # enable_pdf = false
        # enable_plugins = false
        # enable_printing = false
        # fuchsia_code_coverage = true
        # is_component_build = false
        # is_debug = false
        # optimize_for_size = true
        # optional_trace_events_enabled = false
        # produce_v8_compile_hints = false
        # symbol_level = 0
        # target_os = "fuchsia"
        # use_remoteexec = true
        # use_thin_lto = false
        "./obj/fuchsia_web/runners/cast_runner_integration_tests__exec/cast_runner_integration_test.o",
        "./obj/fuchsia_web/webengine/web_engine_core/frame_impl.o",

        # Fallback happens with follwoing args.gn (try/linux_chromium_asan_rel_ng).
        # dcheck_always_on = true
        # fail_on_san_warnings = true
        # is_asan = true
        # is_component_build = false
        # is_debug = false
        # is_lsan = true
        # symbol_level = 1
        # use_remoteexec = true
        "./obj/components/autofill/core/browser/unit_tests/browser_autofill_manager_unittest.o",
        "./obj/content/browser/browser/render_frame_host_impl.o",
        "./obj/content/browser/browser/web_contents_impl.o",
        "./obj/content/test/content_browsertests/back_forward_cache_features_browsertest.o",
        "./obj/content/test/content_browsertests/back_forward_cache_internal_browsertest.o",
        "./obj/content/test/content_browsertests/fenced_frame_browsertest.o",
        "./obj/content/test/content_browsertests/interest_group_browsertest.o",
        "./obj/content/test/content_browsertests/navigation_controller_impl_browsertest.o",
        "./obj/content/test/content_browsertests/navigation_request_browsertest.o",
        "./obj/content/test/content_browsertests/prerender_browsertest.o",
        "./obj/content/test/content_browsertests/render_frame_host_impl_browsertest.o",
        "./obj/content/test/content_browsertests/shared_storage_browsertest.o",
        "./obj/content/test/content_browsertests/site_per_process_browsertest.o",
        "./obj/content/test/content_browsertests/web_contents_impl_browsertest.o",
        "./obj/content/test/content_unittests/ad_auction_service_impl_unittest.o",
        "./obj/content/test/content_unittests/auction_runner_unittest.o",
        "./obj/content/test/content_unittests/authenticator_impl_unittest.o",
        "./obj/content/test/content_unittests/web_usb_service_impl_unittest.o",
        "./obj/net/third_party/quiche/quiche_tests/quic_connection_test.o",
        "./obj/third_party/abseil-cpp/absl/functional/any_invocable_test/any_invocable_test.o",
        "./obj/third_party/blink/renderer/core/unit_tests/web_frame_test.o",
        "./obj/third_party/blink/renderer/core/unit_tests/web_media_player_impl_unittest.o",
        "./obj/third_party/perfetto/protos/perfetto/trace/merged_trace_lite/perfetto_trace.pb.o",
        "./obj/ui/gl/gl_unittest_utils/gl_bindings_autogen_mock.o",

        # Fallback happens with follwoing args.gn (ci/Linux MSan Builder).
        # dcheck_always_on = false
        # is_component_build = false
        # is_debug = false
        # is_msan = true
        # msan_track_origins = 2
        # use_remoteexec = true
        "./obj/chrome/test/unit_tests/site_settings_handler_unittest.o",
        "./obj/components/policy/chrome_settings_proto_generated_compile_proto/chrome_settings.pb.o",
        "./obj/content/test/content_browsertests/cross_origin_opener_policy_browsertest.o",
        "./obj/content/test/content_browsertests/navigation_controller_impl_browsertest.o",
        "./obj/content/test/content_unittests/auction_runner_unittest.o",
        "./obj/content/test/test_support/service_worker_test_utils.o",
        "./obj/net/dns/tests/host_resolver_manager_unittest.o",
        "./obj/net/third_party/quiche/quiche_tests/oghttp2_adapter_test.o",
        "./obj/net/third_party/quiche/quiche_tests/quic_connection_test.o",
        "./obj/net/third_party/quiche/quiche_tests/structured_headers_generated_test.o",
        "./obj/ui/accessibility/accessibility_unittests/ax_node_position_unittest.o",
        "./obj/ui/gl/gl_unittest_utils/gl_bindings_autogen_mock.o",
        "./obj/ui/gl/gl_unittest_utils/gl_mock.o",
        "./obj/v8/v8_turboshaft/csa-optimize-phase.o",
        # Fallback happens with following args.gn (try/android-arm64-rel)
        # android_static_analysis = "off"
        # coverage_instrumentation_input_file = "//.code-coverage/files_to_instrument.txt"
        # dcheck_always_on = true
        # debuggable_apks = false
        # fail_on_android_expectations = true
        # ffmpeg_branding = "Chrome"
        # is_component_build = false
        # is_debug = false
        # proprietary_codecs = true
        # skip_secondary_abi_for_cq = true
        # strip_debug_info = true
        # symbol_level = 0
        # system_webview_package_name = "com.google.android.apps.chrome"
        # target_cpu = "arm64"
        # target_os = "android"
        # use_clang_coverage = true
        # use_remoteexec = false
        # use_siso = true
        "./obj/content/test/content_browsertests__library/interest_group_browsertest.o",
        "./obj/content/test/content_browsertests__library/prerender_browsertest.o",
        "./obj/content/test/content_browsertests__library/site_per_process_browsertest.o",
        "./robolectric_x64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        # Fallback happens with the following args.gn
        # (linux-build-perf-developer, win-build-perf-developer)
        # is_component_build = true
        # is_debug = true
        # symbol_level = 2
        # use_siso = true
        "./clang_x64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        "./obj/content/browser/browser/render_process_host_impl.o",
        "./obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        # Fallback happens with the following args.gn
        # (try/mac-rel)
        # coverage_instrumentation_input_file = "//.code-coverage/files_to_instrument.txt"
        # dcheck_always_on = true
        # enable_backup_ref_ptr_feature_flag = true
        # enable_dangling_raw_ptr_checks = true
        # enable_dangling_raw_ptr_feature_flag = true
        # ffmpeg_branding = "Chrome"
        # is_component_build = false
        # is_debug = false
        # proprietary_codecs = true
        # symbol_level = 0
        # target_cpu = "x64"
        # use_clang_coverage = true
        # use_remoteexec = false
        # use_siso = true
        "./arm64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        "./arm64_v8_x64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        "./clang_arm64_v8_x64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        # Fallback happens with the following args.gn
        # (try/ios-simulator)
        # coverage_instrumentation_input_file = "//.code-coverage/files_to_instrument.txt"
        # enable_run_ios_unittests_with_xctest = true
        # is_component_build = false
        # is_debug = true
        # symbol_level = 1
        # target_cpu = "x64"
        # target_environment = "simulator"
        # target_os = "ios"
        # use_clang_coverage = true
        # use_remoteexec = false
        # use_siso = true
        "./clang_arm64/obj/net/http/transport_security_state_generated_files/transport_security_state.o",
        # Fallback happens with the following args.gn
        # (mac-build-perf-developer, win-build-perf-developer)
        # is_component_build = true
        # is_debug = true
        # symbol_level = 2
        # use_siso = true
        "./obj/content/browser/browser/storage_partition_impl.o",
        "./obj/third_party/blink/renderer/core/core/local_frame_view.o",
        "./obj/third_party/blink/renderer/core/core_hot/document.o",
        # Fallback happens with the following args.gn
        # (android-build-perf-developer)
        # is_component_build = true
        # is_debug = true
        # symbol_level = 2
        # target_cpu = "arm64"
        # target_os = "android"
        # use_siso = true
        "./obj/third_party/sentencepiece/sentencepiece/unicode_script.o",
    ]
    if runtime.os == "windows":
        exit137_list = [obj.removesuffix(".o") + ".obj" for obj in exit137_list if obj.startswith("./obj/")]

        # Fallback happens with the following args.gn
        # (win-build-perf-developer)
        # is_component_build = true
        # is_debug = true
        # symbol_level = 2
        # use_siso = true
        exit137_list.extend([
            "./obj/third_party/blink/renderer/core/core/local_frame.obj",
        ])

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

        # Some large compile take longer than the default timeout 2m.
        r["timeout"] = "4m"

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

b289968566 = module(
    "b289968566",
    step_config = __step_config,
)
