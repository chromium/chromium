#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generic runner library to run Fuchsia tests via orchestrate using GN
metadata."""

import json
import logging
import os
import subprocess
import sys

# Find script directory to add it to python path for imports
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

import common  # pylint: disable=wrong-import-position


def run_tests_with_orchestrate(out_dir: str,
                               test_name: str,
                               test_args: list = None,
                               logs_dir: str = None) -> int:
    """Entry point to execute the test suite via orchestrate config."""
    if not out_dir:
        raise ValueError('--out-dir must be specified.')
    if not logs_dir:
        logs_dir = '/tmp/'
    packages = common.read_package_paths(out_dir, test_name)
    logging.info(
        'Resolving package archives for \'%s\': %s',
        test_name, packages)

    config_json = os.path.join(SCRIPT_DIR, 'orchestrate.json')
    overrides = {'emulator': {'package_archives': packages}}
    overrides_str = json.dumps(overrides)

    orchestrate_bin = os.path.join(common.SDK_TOOLS_DIR, 'orchestrate')
    run_test_bin = os.path.join(SCRIPT_DIR, 'run_executable_test.py')

    target_cmd = [
        run_test_bin, '--test-name', test_name, '--out-dir', out_dir,
        '--logs-dir', logs_dir
    ]
    if test_args:
        target_cmd.extend(test_args)

    cmd = [
        orchestrate_bin, 'run', '-input', config_json, '-overrides',
        overrides_str, '--'
    ] + target_cmd

    print(f"Running command: {subprocess.list2cmdline(cmd)}")
    try:
        proc = subprocess.run(cmd, check=False)
        return proc.returncode
    except KeyboardInterrupt:
        print("\nExecution interrupted by user.", file=sys.stderr)
        return 130
