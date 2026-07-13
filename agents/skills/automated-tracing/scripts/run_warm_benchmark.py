#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script to execute a warm Telemetry benchmark run.

It automates the warmup run, profile migration/cleanup, actual run,
and cleanup of temporary directories for both Android and Desktop.
"""

import argparse
import os
import shutil
import subprocess
import sys
import time


def run_cmd(cmd):
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        print(f"Error running command: {' '.join(cmd)}")
        print(f"Stdout:\n{result.stdout}")
        print(f"Stderr:\n{result.stderr}")
        sys.exit(result.returncode)
    return result.stdout


def is_package_installed(device, package):
    cmd = ["adb", "-s", device, "shell", "pm", "list", "packages", package]
    output = subprocess.run(cmd, capture_output=True, text=True,
                            check=False).stdout
    for line in output.splitlines():
        if line.strip() == f"package:{package}":
            return True
    return False


def clear_package_data(device, package):
    if is_package_installed(device, package):
        print(f"Clearing app data for {package} on device...")
        run_cmd(["adb", "-s", device, "shell", "pm", "clear", package])
    else:
        print(f"Package {package} is not installed, skipping pm clear.")


def infer_package_name(browser_executable):
    if not browser_executable:
        return "org.chromium.chrome"
    basename = os.path.basename(browser_executable).lower()
    if "chromepublic" in basename:
        return "org.chromium.chrome"
    elif "chrome" in basename or "monochrome" in basename:
        return "com.android.chrome"
    elif "webview" in basename:
        return "com.android.webview"
    return "org.chromium.chrome"


def main():
    parser = argparse.ArgumentParser(
        description="Run warm Telemetry benchmark.")
    parser.add_argument("--benchmark", required=True, help="Benchmark name.")
    parser.add_argument("--story", required=True, help="Story name.")
    parser.add_argument(
        "--browser-executable",
        required=True,
        help="Path to browser APK or desktop binary.",
    )
    parser.add_argument("--device", help="Device serial (Android only).")
    parser.add_argument("--output-dir",
                        required=True,
                        help="Final benchmark output directory.")
    parser.add_argument(
        "--delete-state",
        action="store_true",
        help="Delete user state (history, cookies, LocalStorage).",
    )

    args, extra_args = parser.parse_known_args()
    args.output_dir = os.path.normpath(args.output_dir)

    is_android = args.device is not None
    if is_android:
        args.package = infer_package_name(args.browser_executable)
    profile_suffix = str(int(time.time()))

    # Define temporary paths in out/ directory
    temp_raw_dir = f"out/warmup_raw_profile_{profile_suffix}"
    temp_warmed_dir = f"out/warmed_profile_{profile_suffix}"
    temp_output_dir = os.path.join(os.path.dirname(args.output_dir),
                                   f"warmup_output_{profile_suffix}")

    # Find path to migrate_profile.py relative to this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    migrate_script = os.path.join(script_dir, "migrate_profile.py")

    try:
        if is_android:
            # Step 1: Warmup run (Android)
            print("Step 1/3: Running warmup benchmark on Android...")
            clear_package_data(args.device, args.package)

            warmup_cmd = [
                "vpython3",
                "tools/perf/run_benchmark",
                "run",
                args.benchmark,
                f"--story={args.story}",
                "--browser=exact",
                f"--browser-executable={args.browser_executable}",
                f"--device={args.device}",
                "--profile-type=default",
                f"--output-dir={temp_output_dir}",
                "--pageset-repeat=1",
            ] + extra_args
            run_cmd(warmup_cmd)

            # Step 2: Migrate and clean (Android)
            print("Step 2/3: Migrating and cleaning profile from Android...")
            migrate_cmd = [
                "vpython3",
                migrate_script,
                f"--device={args.device}",
                f"--package={args.package}",
                f"--output-dir={temp_warmed_dir}",
            ]
            if args.delete_state:
                migrate_cmd.append("--delete-state")
            run_cmd(migrate_cmd)

            # Step 3: Run the actual warm benchmark (Android)
            print("Step 3/3: Running final warm benchmark on Android...")
            clear_package_data(args.device, args.package)

            run_cmd_args = [
                "vpython3",
                "tools/perf/run_benchmark",
                "run",
                args.benchmark,
                f"--story={args.story}",
                "--browser=exact",
                f"--browser-executable={args.browser_executable}",
                f"--device={args.device}",
                f"--profile-dir={temp_warmed_dir}",
                "--profile-type=clean",
                f"--output-dir={args.output_dir}",
                "--pageset-repeat=1",
            ]
            run_cmd_args.extend(extra_args)
            run_cmd(run_cmd_args)

        else:
            # Step 1: Warmup run (Desktop)
            print("Step 1/3: Running warmup benchmark on Desktop...")
            os.makedirs(temp_raw_dir, exist_ok=True)

            warmup_cmd = [
                "vpython3",
                "tools/perf/run_benchmark",
                "run",
                args.benchmark,
                f"--story={args.story}",
                "--browser=exact",
                f"--browser-executable={args.browser_executable}",
                f"--profile-dir={temp_raw_dir}",
                "--profile-type=exact",
                f"--output-dir={temp_output_dir}",
                "--pageset-repeat=1",
            ] + extra_args
            if sys.platform.startswith(
                    "linux") and "DISPLAY" not in os.environ:
                print("DISPLAY not set, wrapping with xvfb.py...")
                warmup_cmd = ["python3", "testing/xvfb.py"] + warmup_cmd
            run_cmd(warmup_cmd)

            # Step 2: Migrate and clean (Desktop)
            print("Step 2/3: Migrating and cleaning local profile...")
            migrate_cmd = [
                "vpython3",
                migrate_script,
                f"--input-dir={temp_raw_dir}",
                f"--output-dir={temp_warmed_dir}",
            ]
            if args.delete_state:
                migrate_cmd.append("--delete-state")
            run_cmd(migrate_cmd)

            # Step 3: Run the actual warm benchmark (Desktop)
            print("Step 3/3: Running final warm benchmark on Desktop...")
            run_cmd_args = [
                "vpython3",
                "tools/perf/run_benchmark",
                "run",
                args.benchmark,
                f"--story={args.story}",
                "--browser=exact",
                f"--browser-executable={args.browser_executable}",
                f"--profile-dir={temp_warmed_dir}",
                "--profile-type=clean",
                f"--output-dir={args.output_dir}",
                "--pageset-repeat=1",
            ]
            run_cmd_args.extend(extra_args)
            if sys.platform.startswith(
                    "linux") and "DISPLAY" not in os.environ:
                print("DISPLAY not set, wrapping with xvfb.py...")
                run_cmd_args = ["python3", "testing/xvfb.py"] + run_cmd_args
            run_cmd(run_cmd_args)

        print("Warm benchmark execution completed! Results saved to"
              f" {args.output_dir}")

    # Cleanup temporary directories
    finally:
        print("Cleaning up temporary directories...")
        if os.path.exists(temp_raw_dir):
            shutil.rmtree(temp_raw_dir)
        if os.path.exists(temp_warmed_dir):
            shutil.rmtree(temp_warmed_dir)
        if os.path.exists(temp_output_dir):
            shutil.rmtree(temp_output_dir)


if __name__ == "__main__":
    main()
