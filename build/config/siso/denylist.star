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
        "build/modules/unified/generate_system_modulemap.py",
        "build/rust/gni_impl/rustc_print_cfg.py",
        "build/rust/gni_impl/write_rustflags.py",
        "chrome/browser/resources/accessibility/tools/generate_manifest.py",
        "components/autofill/core/browser/data_model/autofill_ai/transpile_entity_schema.py",
        "components/autofill/core/browser/form_parsing/transpile_regex_patterns.py",
        "components/language/content/browser/ulp_language_code_locator/ulp_serialized_to_static_c.py",
        "components/optimization_guide/tools/gen_on_device_proto_descriptors.py",
        "components/policy/resources/policy_templates.py",
        "components/resources/ssl/ssl_error_assistant/gen_ssl_error_assistant_proto.py",
        "components/safe_browsing/content/resources/gen_file_type_proto.py",
        "components/vector_icons/aggregate_vector_icons.py",
        "mojo/public/tools/bindings/minify_with_terser.py",
        "third_party/blink/renderer/bindings/scripts/check_generated_file_list.py",
        "third_party/blink/renderer/build/scripts/generate_permission_element_grd.py",
        "third_party/blink/renderer/build/scripts/make_instrumenting_probes.py",
        "third_party/blink/renderer/build/scripts/run_with_pythonpath.py",
        "third_party/catapult/tracing/bin/generate_about_tracing_contents",
        "third_party/dawn/generator/dawn_gpu_info_generator.py",
        "third_party/dawn/generator/dawn_json_generator.py",
        "third_party/dawn/generator/dawn_version_generator.py",
        "third_party/dawn/generator/opengl_loader_generator.py",
        "third_party/devtools-frontend/src/scripts/build/build_inspector_overlay.py",
        "third_party/inspector_protocol/check_protocol_compatibility.py",
        "third_party/inspector_protocol/code_generator.py",
        "third_party/inspector_protocol/concatenate_protocols.py",
        "third_party/libdrm/src/gen_table_fourcc.py",
        "third_party/lottie/minify_lottie.py",
        "third_party/rust/cxx/chromium_integration/run_cxxbridge.py",
        "tools/flags/generate_expired_list.py",
        "tools/grit/grit_info.py",
        "tools/grit/grit.py",
        "tools/grit/pak_util.py",
        "tools/grit/preprocess_if_expr.py",
        "tools/json_schema_compiler/compiler.py",
        "tools/json_schema_compiler/feature_compiler.py",
        "tools/json_to_struct/json_to_struct.py",
        "tools/metrics/histograms/generate_expired_histograms_array.py",
        "tools/metrics/private_metrics/gen_private_metrics_builders.py",
        "tools/metrics/structured/gen_events.py",
        "tools/metrics/structured/gen_validator.py",
        "tools/metrics/ukm/gen_builders.py",
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
