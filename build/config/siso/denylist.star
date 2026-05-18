# -*- bazel-starlark -*-
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for remote execution denylist."""

load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./platform.star", "platform")

def __step_config(ctx, step_config):
    # Denylist of tools that shouldn't run remotely.
    # TODO(http://b/513105742): Remove these scripts from list when missing inputs are fixed.
    python_scripts = [
        "base/win/embedded_i18n/create_string_rc.py",
        "build/modules/unified/generate_system_modulemap.py",
        "build/rust/gni_impl/rustc_print_cfg.py",
        "build/rust/gni_impl/write_rustflags.py",
        "chrome/browser/resources/accessibility/tools/generate_manifest.py",
        "chrome/test/chromedriver/embed_mobile_devices_in_cpp.py",
        "components/autofill/core/browser/data_model/autofill_ai/transpile_entity_schema.py",
        "components/autofill/core/browser/form_parsing/transpile_regex_patterns.py",
        "components/language/content/browser/ulp_language_code_locator/ulp_serialized_to_static_c.py",
        "components/optimization_guide/tools/gen_on_device_proto_descriptors.py",
        "components/policy/resources/policy_templates.py",
        "components/policy/tools/template_writers/template_formatter.py",
        "components/resources/ssl/ssl_error_assistant/gen_ssl_error_assistant_proto.py",
        "components/safe_browsing/content/resources/gen_file_type_proto.py",
        "components/safe_browsing/content/resources/real_time_url_checks_allowlist/gen_real_time_url_allowlist_proto.py",
        "components/vector_icons/aggregate_vector_icons.py",
        "components/zucchini/fuzzers/generate_fuzzer_data.py",
        "mojo/public/tools/bindings/minify_with_terser.py",
        "remoting/tools/build/remoting_copy_locales.py",
        "remoting/tools/build/remoting_localize.py",
        "testing/libfuzzer/fuzzers/generate_v8_inspector_fuzzer_corpus.py",
        "testing/libfuzzer/research/domatolpm/fuzzer_generator.py",
        "testing/libfuzzer/research/domatolpm/generator.py",
        "testing/libfuzzer/research/fuzzilli_idl_fuzzing/generator.py",
        "testing/scripts/rust/generate_script.py",
        "third_party/blink/renderer/bindings/scripts/check_generated_file_list.py",
        "third_party/blink/renderer/build/scripts/generate_permission_element_grd.py",
        "third_party/blink/renderer/build/scripts/make_instrumenting_probes.py",
        "third_party/blink/renderer/build/scripts/run_with_pythonpath.py",
        "third_party/blink/renderer/core/lcp_critical_path_predictor/generate_element_locator_binary_proto.py",
        "third_party/blink/renderer/modules/webcodecs/fuzzer_seed_corpus/generate_image_corpus.py",
        "third_party/cast_core/public/src/build/chromium/cast_core_grpc_generator_wrapper.py",
        "third_party/catapult/tracing/bin/generate_about_tracing_contents",
        "third_party/closure_compiler/js_binary.py",
        "third_party/dawn/generator/dawn_gpu_info_generator.py",
        "third_party/dawn/generator/dawn_json_generator.py",
        "third_party/dawn/generator/dawn_version_generator.py",
        "third_party/dawn/generator/opengl_loader_generator.py",
        "third_party/dawn/src/tint/cmd/bench/generate_benchmark_inputs.py",
        "third_party/dawn/webgpu-cts/scripts/compile_src.py",
        "third_party/dawn/webgpu-cts/scripts/copy_files.py",
        "third_party/dawn/webgpu-cts/scripts/gen_ts_dep_lists.py",
        "third_party/devtools-frontend/src/scripts/build/build_inspector_overlay.py",
        "third_party/inspector_protocol/check_protocol_compatibility.py",
        "third_party/inspector_protocol/code_generator.py",
        "third_party/inspector_protocol/concatenate_protocols.py",
        "third_party/libdrm/src/gen_table_fourcc.py",
        "third_party/libei/scanner.py",
        "third_party/lottie/minify_lottie.py",
        "third_party/perfetto/src/trace_processor/plugins/wattson/gen_wattson_curves.py",
        "third_party/perfetto/tools/gen_amalgamated_sql.py",
        "third_party/pffft/generate_seed_corpus.py",
        "third_party/rust/cxx/chromium_integration/run_cxxbridge.py",
        "third_party/spirv-tools/src/utils/update_build_version.py",
        "third_party/swiftshader/third_party/SPIRV-Tools/utils/update_build_version.py",
        "third_party/webgpu-cts/scripts/compile_src.py",
        "third_party/webgpu-cts/scripts/gen_ts_dep_lists.py",
        "third_party/webgpu-cts/scripts/run_regenerate_internal_cts_html.py",
        "tools/flags/generate_expired_list.py",
        "tools/grit/grit_info.py",
        "tools/grit/grit.py",
        "tools/grit/pak_util.py",
        "tools/grit/preprocess_if_expr.py",
        "tools/json_schema_compiler/compiler.py",
        "tools/json_schema_compiler/feature_compiler.py",
        "tools/json_to_struct/json_to_struct.py",
        "tools/licenses/licenses.py",
        "tools/media_engagement_preload/make_dafsa.py",
        "tools/metrics/histograms/generate_expired_histograms_array.py",
        "tools/metrics/histograms/merge_xml.py",
        "tools/metrics/private_metrics/gen_private_metrics_builders.py",
        "tools/metrics/structured/gen_events.py",
        "tools/metrics/structured/gen_validator.py",
        "tools/metrics/ukm/gen_builders.py",
        "tools/nocompile/wrapper.py",
        "tools/polymer/css_to_wrapper.py",
        "tools/polymer/html_to_wrapper.py",
        "tools/variations/fieldtrial_to_struct.py",
        "ui/webui/resources/tools/bundle_js.py",
        "ui/webui/resources/tools/eslint_ts.py",
        "ui/webui/resources/tools/generate_code_cache.py",
        "ui/webui/resources/tools/minify_js.py",
        "ui/webui/resources/tools/stylelint.py",
        "v8/third_party/inspector_protocol/check_protocol_compatibility.py",
        "v8/third_party/inspector_protocol/code_generator.py",
    ]

    for py_file in python_scripts:
        step_config["rules"].append({
            "name": py_file,
            "command_prefix": platform.python_bin + " ../../" + py_file,
            "remote": False,
        })

    node_js_files = [
        "third_party/devtools-frontend/src/front_end/core/i18n/collect-ui-strings.js",
        "third_party/devtools-frontend/src/front_end/core/i18n/generate-locales-js.js",
        "third_party/devtools-frontend/src/front_end/Images/generate-css-vars.js",
        "third_party/devtools-frontend/src/front_end/panels/timeline/enable-easter-egg.js",
        "third_party/devtools-frontend/src/node_modules/rollup/dist/bin/rollup",
        "third_party/devtools-frontend/src/scripts/build/compress_files.js",
        "third_party/devtools-frontend/src/scripts/build/esbuild.js",
        "third_party/devtools-frontend/src/scripts/build/generate_css_js_files.js",
        "third_party/devtools-frontend/src/scripts/build/generate_devtools_json.mjs",
        "third_party/devtools-frontend/src/scripts/build/generate_html_entrypoint.js",
        "third_party/devtools-frontend/src/scripts/build/ninja/copy-file.js",
        "third_party/devtools-frontend/src/scripts/build/ninja/copy-files.js",
        "third_party/devtools-frontend/src/scripts/build/ninja/generate-tsconfig.js",
        "third_party/devtools-frontend/src/scripts/build/ninja/minify-json-files.js",
        "third_party/devtools-frontend/src/scripts/component_docs/generate_docs.mjs",
    ]

    for js_file in node_js_files:
        step_config["rules"].append({
            "name": js_file,
            "command_prefix": platform.python_bin + " ../../third_party/node/node.py ../../" + js_file,
            "remote": False,
        })

    binary_files = [
        "character_data_generator",
        "country_native_names_generator",
        "crx3_build_action",
        "flatc",
        "make_top_domain_list_variables",
        "top_domain_generator",
        "./v8_context_snapshot_generator",
    ]

    for binary_file in binary_files:
        step_config["rules"].append({
            "name": binary_file,
            "command_prefix": platform.python_bin + " ../../build/gn_run_binary.py " + binary_file,
            "remote": False,
        })

    if config.get(ctx, "default-remote"):
        step_config["rules"].append({
            "name": "default",
            "action": ".*",
            "remote": True,
        })

    return step_config

denylist = module(
    "denylist",
    step_config = __step_config,
)
