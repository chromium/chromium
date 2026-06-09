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
    #
    # How to remove a script from this list and enable remote execution:
    # 1. Remove the script path from `python_scripts` (or other lists below).
    # 2. Run a remote build in strict remote mode to check for missing inputs.
    #    NOTE: By appending '^' to the script path (relative to the build directory,
    #    typically starting with '../../'), you can build targets that use the script
    #    as an input:
    #    autoninja -C out/Default -strict_remote -config=default-remote ../../<script_path>^
    # 3. If the build fails due to missing inputs on RBE (e.g., helper scripts or assets):
    #    - Fix this in the corresponding `BUILD.gn` (not in Siso star files) by adding the
    #      missing files to the `inputs` list of the action target.
    #      If the missing inputs are imported Python scripts, consider using
    #      `action_with_pydeps` instead of manually listing them in `inputs`.
    # 4. Re-build and verify the action succeeds remotely.
    python_scripts = [
        "build/modules/unified/generate_system_modulemap.py",
        # Reads .gclient_entries which is outside of the source tree.
        "build/private_code_test/list_gclient_deps.py",
        "build/private_code_test/ninja_parser.py",
        "chrome/installer/linux/debian/build.py",
        "chrome/installer/linux/debian/calculate_package_deps.py",
        "chrome/installer/linux/debian/merge_package_versions.py",
        "chrome/installer/linux/rpm/build.py",
        "chrome/installer/linux/rpm/calculate_package_deps.py",
        "chrome/installer/linux/rpm/merge_package_deps.py",
        # Parses components_locale_settings.grd and dynamically reads multiple
        # translation .xtb files, making static input tracking too complex.
        "components/language/core/browser/generate_incognito_language_list_map.py",
        "components/optimization_guide/tools/gen_on_device_proto_descriptors.py",
        # Requires dynamic globbing of hundreds of policy definition YAML files
        # under components/policy/resources/templates/ directory.
        "components/policy/resources/policy_templates.py",
        "components/vector_icons/aggregate_vector_icons.py",
        "components/zucchini/fuzzers/generate_fuzzer_data.py",
        "mojo/public/tools/bindings/minify_with_terser.py",
        "remoting/tools/build/remoting_copy_locales.py",
        "testing/libfuzzer/fuzzers/generate_v8_inspector_fuzzer_corpus.py",
        "testing/libfuzzer/research/domatolpm/fuzzer_generator.py",
        "testing/libfuzzer/research/domatolpm/generator.py",
        "testing/scripts/rust/generate_script.py",
        # Dynamically walks and loads 160+ translated grd files (xtb) and requires
        # full grit python libraries. Too many dynamic dependencies to track.
        "third_party/blink/renderer/build/scripts/generate_permission_element_grd.py",
        # Dynamically walks and reads multiple test image files under
        # web_tests/images/resources/ directory, making input tracking too complex.
        "third_party/blink/renderer/modules/webcodecs/fuzzer_seed_corpus/generate_image_corpus.py",
        "third_party/cast_core/public/src/build/chromium/cast_core_grpc_generator_wrapper.py",
        "third_party/catapult/tracing/bin/generate_about_tracing_contents",
        # Requires JDK to run. Additionally, it dynamically parses .js_library
        # metadata files to determine JS source files to load at runtime, making
        # input tracking too complex for static analysis without Starlark handlers.
        "third_party/closure_compiler/js_binary.py",
        "third_party/dawn/generator/dawn_json_generator.py",
        "third_party/dawn/generator/dawn_version_generator.py",
        "third_party/dawn/generator/opengl_loader_generator.py",
        "third_party/dawn/src/tint/cmd/bench/generate_benchmark_inputs.py",
        "third_party/dawn/tools/run.py",
        "third_party/dawn/webgpu-cts/scripts/compile_src.py",
        "third_party/dawn/webgpu-cts/scripts/copy_files.py",
        "third_party/dawn/webgpu-cts/scripts/gen_ts_dep_lists.py",
        "third_party/devtools-frontend/src/scripts/build/build_inspector_overlay.py",
        "third_party/devtools-frontend/src/scripts/build/typescript/ts_library.py",
        "third_party/inspector_protocol/check_protocol_compatibility.py",
        "third_party/inspector_protocol/code_generator.py",
        "third_party/inspector_protocol/concatenate_protocols.py",
        "third_party/libei/scanner.py",
        "third_party/lottie/minify_lottie.py",
        "third_party/perfetto/src/trace_processor/plugins/wattson/gen_wattson_curves.py",
        "third_party/perfetto/tools/gen_amalgamated_sql.py",
        "third_party/rust/cxx/chromium_integration/run_cxxbridge.py",
        "third_party/spirv-tools/src/utils/update_build_version.py",
        "third_party/swiftshader/third_party/SPIRV-Tools/utils/update_build_version.py",
        "third_party/webgpu-cts/scripts/compile_src.py",
        "third_party/webgpu-cts/scripts/gen_ts_dep_lists.py",
        "third_party/webgpu-cts/scripts/run_regenerate_internal_cts_html.py",
        "tools/grit/grit_info.py",
        "tools/grit/grit.py",
        "tools/grit/pak_util.py",
        "tools/grit/preprocess_if_expr.py",
        "tools/licenses/licenses.py",

        # merge_xml.py relies on expand_owners.py, which
        # executes dirmd (depot_tools) that queries local git repository
        # metadata. This cannot run inside clean RBE sandboxes.
        # TODO: Consider recoding the parsing in Python to sever the link to
        # dirmd, as this dependency keeps causing various problems.
        "tools/metrics/histograms/merge_xml.py",
        "tools/nocompile/wrapper.py",
        "tools/polymer/css_to_wrapper.py",
        "tools/polymer/html_to_wrapper.py",
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
        "third_party/devtools-frontend/src/scripts/build/ninja/generate-declaration.js",
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
