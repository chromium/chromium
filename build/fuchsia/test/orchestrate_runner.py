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
