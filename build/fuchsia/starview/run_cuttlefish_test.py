#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
import time

import provision_starview

STARVIEW_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
    start_time = time.time()
    parser = argparse.ArgumentParser(
        description="Test run_cuttlefish.py script"
    )
    parser.add_argument(
        '--isolated-script-test-output',
        type=os.path.abspath,
        help='Path to write isolated script test output JSON.',
    )
    args_parsed, extra_args = parser.parse_known_args()
    # Filter out any other isolated-script or timeout arguments from extra_args before passing to run_cuttlefish.py
    extra_args = [
        arg
        for arg in extra_args
        if not arg.startswith('--isolated-script-test-')
        and not arg.startswith('--timeout-scale')
    ]
    adb_path = (
        provision_starview.DEFAULT_ADB_PATH
        if os.path.exists(provision_starview.DEFAULT_ADB_PATH)
        else 'adb'
    )

    # Launch emulator via run_cuttlefish.py on a test port
    cmd = [
        sys.executable,
        os.path.join(STARVIEW_DIR, 'run_cuttlefish.py'),
        '--adb-port',
        '6525',
        '--headless',
    ] + extra_args
    print(f"Starting emulator under test: {' '.join(cmd)}")
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )

    success = False
    try:
        for line in iter(proc.stdout.readline, ''):
            print(f"[Emulator Log] {line.strip()}")
            if "Successfully connected to guest ADB!" in line:
                print("Emulator reported successful ADB connection!")
                success = True
                break

        if success:
            target = '127.0.0.1:6525'
            provision_starview.wait_adb_shell(
                adb_path,
                target,
                ['echo', 'STARNIX_ALIVE'],
                r'.*STARNIX_ALIVE.*',
                timeout=15,
                poll_interval=2.0,
            )
            print("Android shell successfully verified!")

            provision_starview.wait_adb_shell(
                adb_path,
                target,
                ['pm', 'list', 'packages'],
                r'.*package:.*',
                timeout=15,
                poll_interval=2.0,
            )
            print("Android package manager successfully verified!")

            provision_starview.wait_adb_shell(
                adb_path,
                target,
                ['wm', 'size'],
                r'.*1080x1920.*',
                timeout=15,
                poll_interval=2.0,
            )
            print("Display size successfully verified!")

            provision_starview.wait_adb_shell(
                adb_path,
                target,
                ['wm', 'density'],
                r'.*480.*',
                timeout=15,
                poll_interval=2.0,
            )
            print("Display density successfully verified!")

    except Exception as e:
        print(f"Test encountered error: {e}")
        success = False
    finally:
        print("Terminating emulator subprocess...")
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        print("Emulator process terminated.")

    if args_parsed.isolated_script_test_output:
        failure_type = 'PASS' if success else 'FAIL'
        results_json = {
            'version': 3,
            'interrupted': False,
            'num_failures_by_type': {failure_type: 1},
            'path_delimiter': '/',
            'seconds_since_epoch': start_time,
            'tests': {
                'run_cuttlefish_test': {
                    'expected': 'PASS',
                    'actual': failure_type,
                    'time': time.time() - start_time,
                },
            },
        }
        output_dir = os.path.dirname(args_parsed.isolated_script_test_output)
        if output_dir:
            os.makedirs(output_dir, exist_ok=True)
        with open(args_parsed.isolated_script_test_output, 'w') as f:
            json.dump(results_json, f, indent=2)

    if success:
        print("TEST PASSED")
        sys.exit(0)
    else:
        print("TEST FAILED")
        sys.exit(1)


if __name__ == '__main__':
    main()
