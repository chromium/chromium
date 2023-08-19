# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for mojo."""

load("@builtin//struct.star", "module")

__filegroups = {}

__handlers = {}

def __step_config(ctx, step_config):
    # mojom_bindings_generator.py will run faster on n2-highmem-8 than
    # n2-custom-2-3840
    # e.g.
    #  n2-highmem-8: exec: 880.202978ms
    #  n2-custom-2-3840: exec: 2.42808488s
    platform_ref = "large"
    step_config["rules"].extend([
        {
            "name": "mojo/mojom_bindings_generator",
            "command_prefix": "python3 ../../mojo/public/tools/bindings/mojom_bindings_generator.py",
            "inputs": [
                "mojo/public/tools/bindings/mojom_bindings_generator.py",
            ],
            "indirect_inputs": {
                "includes": [
                    "*.js",
                    "*.mojom",
                    "*.mojom-module",
                    "*.test-mojom",
                    "*.test-mojom-module",
                    "*.zip",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            # TODO(crbug.com/1437820): unspecified outputs of mojom_bindings_generator.py
            "outputs_map": {
                "./gen/components/aggregation_service/aggregation_service.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/components/aggregation_service/aggregation_service.mojom-webui.js",
                    ],
                },
                "./gen/components/attribution_reporting/eligibility_error.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/components/attribution_reporting/eligibility_error.mojom-webui.js",
                        "./gen/mojom-webui/components/attribution_reporting/registration_eligibility.mojom-webui.js",
                        "./gen/mojom-webui/components/attribution_reporting/source_registration_error.mojom-webui.js",
                        "./gen/mojom-webui/components/attribution_reporting/trigger_registration_error.mojom-webui.js",
                    ],
                },
                "./gen/components/attribution_reporting/registration.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/components/attribution_reporting/registration.mojom-webui.js",
                    ],
                },
                "./gen/media/capture/mojom/image_capture.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/media/capture/mojom/image_capture.mojom-webui.js",
                    ],
                },
                "./gen/services/device/public/mojom/usb_device.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/services/device/public/mojom/usb_device.mojom-webui.js",
                        "./gen/mojom-webui/services/device/public/mojom/usb_enumeration_options.mojom-webui.js",
                        "./gen/mojom-webui/services/device/public/mojom/usb_manager.mojom-webui.js",
                        "./gen/mojom-webui/services/device/public/mojom/usb_manager_client.mojom-webui.js",
                    ],
                },
                "./gen/services/media_session/public/mojom/audio_focus.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/services/media_session/public/mojom/audio_focus.mojom-webui.js",
                        "./gen/mojom-webui/services/media_session/public/mojom/constants.mojom-webui.js",
                        "./gen/mojom-webui/services/media_session/public/mojom/media_controller.mojom-webui.js",
                        "./gen/mojom-webui/services/media_session/public/mojom/media_session.mojom-webui.js",
                    ],
                },
                "./gen/services/network/public/mojom/attribution.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/services/network/public/mojom/attribution.mojom-webui.js",
                    ],
                },
                "./gen/services/network/public/mojom/schemeful_site.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/services/network/public/mojom/schemeful_site.mojom-webui.js",
                    ],
                },
                "./gen/third_party/blink/public/mojom/quota/quota_manager_host.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/third_party/blink/public/mojom/quota/quota_manager_host.mojom-webui.js",
                        "./gen/mojom-webui/third_party/blink/public/mojom/quota/quota_types.mojom-webui.js",
                    ],
                },
                "./gen/third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-webui.js",
                        "./gen/mojom-webui/third_party/blink/public/mojom/storage_key/storage_key.mojom-webui.js",
                    ],
                },
                "./gen/ui/base/mojom/ui_base_types.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/ui/base/mojom/ui_base_types.mojom-webui.js",
                        "./gen/mojom-webui/ui/base/mojom/window_open_disposition.mojom-webui.js",
                    ],
                },
                "./gen/ui/gfx/image/mojom/image.mojom.js": {
                    "outputs": [
                        "./gen/mojom-webui/ui/gfx/image/mojom/image.mojom-webui.js",
                    ],
                },
            },
            "restat": True,
            "remote": True,
            "timeout": "2m",
            "output_local": True,
            "platform_ref": platform_ref,
        },
        {
            "name": "mojo/mojom_parser",
            "command_prefix": "python3 ../../mojo/public/tools/mojom/mojom_parser.py",
            "indirect_inputs": {
                "includes": [
                    "*.build_metadata",
                    "*.mojom",
                    "*.mojom-module",
                    "*.test-mojom",
                    "*.test-mojom-module",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            # TODO(b/288523418): missing inputs for mojom_parser?
            "outputs_map": {
                "./gen/mojo/public/interfaces/bindings/tests/sample_import2.mojom-module": {
                    "inputs": [
                        "./gen/mojo/public/interfaces/bindings/tests/test_mojom_import_wrapper.build_metadata",
                        "./gen/mojo/public/interfaces/bindings/tests/test_mojom_import_wrapper_wrapper.build_metadata",
                    ],
                },
                "./gen/mojo/public/interfaces/bindings/tests/math_calculator.mojom-module": {
                    "inputs": [
                        "./gen/mojo/public/interfaces/bindings/tests/test_mojom_import_wrapper.build_metadata",
                        "./gen/mojo/public/interfaces/bindings/tests/test_mojom_import_wrapper_wrapper.build_metadata",
                    ],
                },
                "./gen/mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom-module": {
                    "inputs": [
                        "./gen/mojo/public/interfaces/bindings/tests/test_mojom_import_wrapper.build_metadata",
                        "./gen/mojo/public/interfaces/bindings/tests/test_mojom_import_wrapper_wrapper.build_metadata",
                    ],
                },
            },
            "remote": True,
            "input_root_absolute_path": True,
            "output_local": True,
            "platform_ref": platform_ref,
        },
    ])
    return step_config

mojo = module(
    "mojo",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
