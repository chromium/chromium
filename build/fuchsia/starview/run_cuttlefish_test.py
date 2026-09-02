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
    starview_dir = os.path.dirname(os.path.abspath(__file__))
    run_script = os.path.join(starview_dir, 'run_cuttlefish.py')
    adb_path = os.path.join(
        starview_dir,
        '../../../third_party/android_sdk/public/platform-tools/adb',
    )
    if not os.path.exists(adb_path):
        adb_path = 'adb'

    # Launch emulator via run_cuttlefish.py on a test port
    adb_port = 6525
    cmd = [
        sys.executable,
        run_script,
        '--adb-port',
        str(adb_port),
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
            # Run adb shell command to verify the Android container is alive
            adb_cmd = [
                adb_path,
                '-s',
                f'127.0.0.1:{adb_port}',
                'shell',
                'echo',
                'STARNIX_ALIVE',
            ]
            print(f"Running verification command: {' '.join(adb_cmd)}")
            for attempt in range(5):
                res = subprocess.run(adb_cmd, capture_output=True, text=True)
                print(f"adb output: {res.stdout.strip()}")
                if 'STARNIX_ALIVE' in res.stdout:
                    print("Android shell successfully verified!")
                    break
                time.sleep(2)
            else:
                print("Failed to get expected output from adb shell.")
                success = False

            if success:
                pm_cmd = [
                    adb_path,
                    '-s',
                    f'127.0.0.1:{adb_port}',
                    'shell',
                    'pm',
                    'list',
                    'packages',
                ]
                print(
                    f"Running package manager verification: {' '.join(pm_cmd)}"
                )
                for attempt in range(5):
                    res = subprocess.run(pm_cmd, capture_output=True, text=True)
                    if 'package:' in res.stdout:
                        print("Android package manager successfully verified!")
                        break
                    time.sleep(2)
                else:
                    print("Failed to get package list from package manager.")
                    success = False

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
