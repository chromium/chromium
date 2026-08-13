#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to remove unused C++ includes safely using clang-include-cleaner."""

import argparse
import os
import subprocess
import sys


def run_command(cmd, cwd=None):
    result = subprocess.run(
        cmd, capture_output=True, text=True, cwd=cwd, check=False
    )
    return result.returncode == 0, result.stdout.strip(), result.stderr.strip()


def get_targets_for_file(src_root, file_path, build_dir):
    rel_path = os.path.relpath(file_path, src_root)
    gn_path = f"//{rel_path.replace(os.sep, '/')}"
    cmd = ["gn", "refs", build_dir, gn_path]
    success, stdout, _ = run_command(cmd, cwd=src_root)
    if not success:
        return []
    targets = [
        line.strip().lstrip('/') for line in stdout.splitlines() if line.strip()
    ]
    return targets


def main():
    parser = argparse.ArgumentParser(
        description="Clean unused includes safely in Chromium."
    )
    parser.add_argument(
        "--src-root", required=True, help="Chromium src root directory"
    )
    parser.add_argument(
        "--folder",
        required=True,
        help="Folder to look through (absolute or relative to src-root)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print changes without applying them",
    )
    parser.add_argument(
        "--non-recursive",
        action="store_true",
        help="Do not walk subdirectories",
    )
    parser.add_argument(
        "--build-dir",
        default="out/Default",
        help="Build directory relative to src-root (default: out/Default)",
    )
    args = parser.parse_args()

    src_root = os.path.abspath(args.src_root)
    folder_path = os.path.abspath(args.folder)

    if not os.path.exists(folder_path):
        print(f"Error: Folder {folder_path} does not exist.")
        sys.exit(1)

    clang_cleaner = os.path.join(
        src_root,
        "third_party/llvm-build/Release+Asserts/bin/clang-include-cleaner",
    )
    if not os.path.exists(clang_cleaner):
        print(f"Error: clang-include-cleaner not found at {clang_cleaner}")
        sys.exit(1)

    cc_files = []
    for root, dirs, files in os.walk(folder_path):
        if args.non_recursive:
            dirs.clear()
        for f in files:
            if f.endswith(".cc"):
                cc_files.append(os.path.join(root, f))

    print(f"Found {len(cc_files)} C++ source files in {folder_path}")

    for file_path in cc_files:
        rel_file_path = os.path.relpath(file_path, src_root)
        print(f"\n[*] Analyzing {rel_file_path}...")

        # Run preview first
        cmd = [
            clang_cleaner,
            "-p",
            ".",
            "--print=changes",
            "--disable-insert",
            file_path,
        ]
        success, stdout, stderr = run_command(cmd, cwd=src_root)
        if not success:
            print(f"  [Error] Failed to run preview: {stderr}")
            continue
        if not stdout:
            print("  No unused includes found.")
            continue

        print(f"  Unused includes detected:\n{stdout}")
        if args.dry_run:
            continue

        # Get targets using gn refs
        targets = get_targets_for_file(src_root, file_path, args.build_dir)
        if not targets:
            print(
                f"  [Warning] No GN targets found referencing "
                f"{rel_file_path}. Skipping safety check and edit!"
            )
            continue

        print(f"  Referencing targets: {', '.join(targets)}")

        # Read original content to safely revert on compilation failure
        with open(file_path, "r", encoding="utf-8") as f:
            original_content = f.read()

        # Apply edits
        edit_cmd = [
            clang_cleaner,
            "-p",
            ".",
            "--edit",
            "--disable-insert",
            file_path,
        ]
        success, _, edit_err = run_command(edit_cmd, cwd=src_root)
        if not success:
            print(f"  [Error] Failed to apply edit: {edit_err}")
            continue

        # Verify compilation of all referencing targets
        compile_failed = False
        for target in targets:
            print(f"  Compiling target {target}...")
            compile_cmd = ["autoninja", "-C", args.build_dir, target]
            success, compile_out, compile_err = run_command(
                compile_cmd, cwd=src_root
            )
            if (
                not success
                or "The build has finished with an error" in compile_out
            ):
                print(f"  [Error] Compilation failed for target {target}!")
                # Print output to help diagnosis
                print(compile_out or compile_err)
                compile_failed = True
                break

        if compile_failed:
            print(f"  Reverting changes to {rel_file_path}...")
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(original_content)
        else:
            print(
                f"  [Success] Unused includes successfully removed "
                f"from {rel_file_path}!"
            )


if __name__ == "__main__":
    main()
