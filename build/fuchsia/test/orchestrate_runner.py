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

import common  # noqa: E402


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
        'services_unittests',
        'shell_dialogs_unittests',
        'skia_unittests',
        'snapshot_unittests',
        'sql_unittests',
        'storage_unittests',
        # The test_type string passed to run_test.py when setting up the test
        # environment via orchestrate (equivalent to test_env_setup.py).
        'test_env_expose.py',
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


def run_tests_with_orchestrate(
    out_dir: str, packages: list, target_cmd: list, logs_dir: str = None
) -> int:
    """Entry point to execute the test suite via orchestrate config."""
    if not out_dir:
        raise ValueError('--out-dir must be specified.')
    if not logs_dir:
        logs_dir = '/tmp/'

    assert os.path.exists(common.ORCHESTRATE_CONFIG)
    cmd = [
        os.path.join(common.SDK_TOOLS_DIR, 'orchestrate'),
        'run',
        '-input',
        common.ORCHESTRATE_CONFIG,
    ]
    if packages:
        packages = list(dict.fromkeys(packages))
        emulator_overrides = {'package_archives': packages}
        build_ids = [
            ids_path
            for ids_path in dict.fromkeys(
                os.path.join(os.path.dirname(pkg), 'ids.txt')
                for pkg in packages
            )
            if os.path.isfile(os.path.join(out_dir, ids_path))
        ]
        if build_ids:
            emulator_overrides['build_ids'] = build_ids
        cmd.extend(['-overrides', json.dumps({'emulator': emulator_overrides})])
    cmd.extend(['--', *target_cmd])

    # TODO(crbug.com/346624801): Revert this env injection after
    # fxrev.dev/1716732 is merged and SDK is rolled.
    env = os.environ.copy()
    if logs_dir:
        env['TEST_UNDECLARED_OUTPUTS_DIR'] = logs_dir
    common.catch_sigterm()
    print(f"Running command: {subprocess.list2cmdline(cmd)}")
    with subprocess.Popen(cmd, env=env, cwd=out_dir) as proc:
        try:
            return proc.wait()
        except KeyboardInterrupt:
            print("\nExecution interrupted by user.", file=sys.stderr)
            proc.terminate()
            return 130
        except SystemExit:
            proc.terminate()
            return proc.wait()
