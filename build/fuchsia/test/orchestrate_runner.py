#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generic runner library to run Fuchsia tests via orchestrate using GN
metadata."""

import json
import os
import subprocess
import sys

# Find script directory to add it to python path for imports
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

import common  # pylint: disable=wrong-import-position


def support_orchestrate(test_type: str) -> bool:
    """Returns True if test_type is supported by orchestrate."""
    return test_type in {
        'absl_hardening_tests',
        'accessibility_unittests',
        'aura_unittests',
        'base_unittests',
        'blink_common_unittests',
        'blink_fuzzer_unittests',
        'blink_heap_unittests',
        'blink_platform_unittests',
        'blink_unittests',
        'boringssl_crypto_tests',
        'boringssl_ssl_tests',
        'capture_unittests',
        'cast_runner_browsertests',
        'cast_runner_integration_tests',
        'cast_runner_unittests',
        'cast_unittests',
        'cc_unittests',
        'components_browsertests',
        'components_unittests',
        'compositor_unittests',
        'content_browsertests',
        'content_unittests',
        'crypto_unittests',
        'display_unittests',
        'events_unittests',
        'filesystem_service_unittests',
        'gcm_unit_tests',
        'gfx_unittests',
        'gin_unittests',
        'google_apis_unittests',
        'gpu_unittests',
        'gwp_asan_unittests',
        'headless_browsertests',
        'headless_unittests',
        'ipc_tests',
        'latency_unittests',
        'libjingle_xmpp_unittests',
        'liburlpattern_unittests',
        'media_unittests',
        'message_center_unittests',
        'midi_unittests',
        'mojo_unittests',
        'native_theme_unittests',
        'net_unittests',
        'ozone_gl_unittests',
        'ozone_unittests',
        'perfetto_unittests',
        'rust_gtest_interop_unittests',
        'services_unittests',
        'shell_dialogs_unittests',
        'skia_unittests',
        'snapshot_unittests',
        'sql_unittests',
        'storage_unittests',
        'ui_base_unittests',
        'ui_touch_selection_unittests',
        'ui_unittests',
        'url_unittests',
        'views_examples_unittests',
        'views_unittests',
        'viz_unittests',
        'web_engine_browsertests',
        'web_engine_integration_tests',
        'web_engine_unittests',
        'wm_unittests',
        'wtf_unittests',
        'zlib_unittests',
    }


def run_tests_with_orchestrate(out_dir: str,
                               packages: list,
                               target_cmd: list,
                               logs_dir: str = None) -> int:
    """Entry point to execute the test suite via orchestrate config."""
    if not out_dir:
        raise ValueError('--out-dir must be specified.')
    if not logs_dir:
        logs_dir = '/tmp/'

    config_json = os.path.join(SCRIPT_DIR, 'orchestrate.json')
    overrides = {'emulator': {'package_archives': packages}}
    overrides_str = json.dumps(overrides)

    orchestrate_bin = os.path.join(common.SDK_TOOLS_DIR, 'orchestrate')

    cmd = [
        orchestrate_bin, 'run', '-input', config_json, '-overrides',
        overrides_str, '--'
    ] + target_cmd

    # TODO(crbug.com/346624801): Revert this env injection after
    # fxrev.dev/1716732 is merged and SDK is rolled.
    env = os.environ.copy()
    if logs_dir:
        env['TEST_UNDECLARED_OUTPUTS_DIR'] = logs_dir
    print(f"Running command: {subprocess.list2cmdline(cmd)}")
    try:
        proc = subprocess.run(cmd, env=env, cwd=out_dir, check=False)
        return proc.returncode
    except KeyboardInterrupt:
        print("\nExecution interrupted by user.", file=sys.stderr)
        return 130
