#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to migrate and clean Chrome profile directories for Telemetry."""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def run_cmd(cmd):
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        print(f"Error running command: {' '.join(cmd)}")
        print(f"Stdout:\n{result.stdout}")
        print(f"Stderr:\n{result.stderr}")
        sys.exit(result.returncode)
    return result.stdout


def clean_profile(profile_dir, delete_state=False):
    print(f"Cleaning profile: {profile_dir} (delete_state={delete_state})")

    # 1. Delete empty directories recursively (bottom-up)
    deleted_dirs = True
    while deleted_dirs:
        deleted_dirs = False
        for root, dirs, files in os.walk(profile_dir, topdown=False):
            for d in dirs:
                dir_path = os.path.join(root, d)
                if not os.listdir(dir_path):
                    print(f"Removing empty directory: {dir_path}")
                    os.rmdir(dir_path)
                    deleted_dirs = True

    # 2. Delete active tabs and sessions (always required to prevent DevTools
    # WebSocket hangs)
    to_delete_patterns = [
        "app_active_tabs", "app_tabs", "Sessions", "Session Storage",
        "SessionStorage"
    ]

    # 3. Optional: Delete user state, history, cookies, and LocalStorage
    if delete_state:
        to_delete_patterns.extend(["History", "Cookies", "LocalStorage"])

    for root, dirs, files in os.walk(profile_dir, topdown=True):
        # Check directories
        for d in list(dirs):
            if any(pattern in d for pattern in to_delete_patterns):
                dir_path = os.path.join(root, d)
                print(f"Removing directory: {dir_path}")
                shutil.rmtree(dir_path)
                dirs.remove(d)

        # Check files
        for f in files:
            if any(pattern in f for pattern in to_delete_patterns):
                file_path = os.path.join(root, f)
                print(f"Removing file: {file_path}")
                os.remove(file_path)


def main():
    parser = argparse.ArgumentParser(
        description="Migrate and clean Chrome profile for Telemetry.")
    parser.add_argument("--output-dir",
                        required=True,
                        help="Local directory to save the cleaned profile.")
    parser.add_argument("--device", help="Device serial (Android only).")
    parser.add_argument("--package",
                        default="org.chromium.chrome",
                        help="App package name (Android only).")
    parser.add_argument("--input-dir",
                        help="Input profile directory (Desktop only).")
    parser.add_argument(
        "--delete-state",
        action="store_true",
        help="Delete user state (history, cookies, LocalStorage).")

    args = parser.parse_args()

    args.output_dir = os.path.normpath(args.output_dir)
    if args.input_dir:
        args.input_dir = os.path.normpath(args.input_dir)

    is_android = args.device is not None

    if is_android:
        if args.input_dir:
            print("Error: --input-dir should not be used when --device is "
                  "specified.")
            sys.exit(1)

        with tempfile.TemporaryDirectory() as temp_dir:
            print("Pulling profile from device...")
            run_cmd(["adb", "-s", args.device, "root"])

            device_profile_path = f"/data/data/{args.package}"
            run_cmd([
                "adb", "-s", args.device, "pull", f"{device_profile_path}/",
                temp_dir
            ])

            pulled_profile_path = os.path.join(temp_dir, args.package)
            if not os.path.exists(pulled_profile_path):
                pulled_profile_path = temp_dir

            clean_profile(pulled_profile_path, delete_state=args.delete_state)

            if os.path.exists(args.output_dir):
                shutil.rmtree(args.output_dir)
            os.makedirs(os.path.dirname(args.output_dir), exist_ok=True)
            shutil.copytree(pulled_profile_path, args.output_dir)
    else:
        if not args.input_dir:
            print(
                "Error: --input-dir is required for desktop profile migration."
            )
            sys.exit(1)

        if not os.path.exists(args.input_dir):
            print(f"Error: Input directory does not exist: {args.input_dir}")
            sys.exit(1)

        print(f"Copying profile from {args.input_dir} to {args.output_dir}...")
        if os.path.exists(args.output_dir):
            shutil.rmtree(args.output_dir)
        os.makedirs(os.path.dirname(args.output_dir), exist_ok=True)
        shutil.copytree(args.input_dir, args.output_dir)

        clean_profile(args.output_dir, delete_state=args.delete_state)

    print(f"Warmed profile successfully saved to: {args.output_dir}")


if __name__ == "__main__":
    main()
