#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Processes .tmpl.md files and removes comments.

For each .tmpl.md file in the specified directory, this script creates a
new .md file with the same name (minus the .annotated suffix) that has all
HTML-style comments removed.
"""

import os
import re
import shlex
import sys
import argparse


def process_file(input_path, check_only=False):
    """Processes a single .tmpl.md file.

  Args:
    input_path: The path to the .tmpl.md file.
    check_only: If True, checks for stale files without writing them.

  Returns:
    The output path if the file is stale and check_only is True, otherwise None.
  """
    output_path = input_path.replace(".tmpl.md", ".md")
    with open(input_path, "r", encoding="utf-8") as f:
        expected_content = f.read()

    # Remove HTML-style comments
    expected_content = re.sub(r"<!--.*?-->\n?",
                              "",
                              expected_content,
                              flags=re.DOTALL)

    if check_only:
        if not os.path.exists(output_path):
            return output_path
        with open(output_path, "r", encoding="utf-8") as f:
            actual_content = f.read()
        if actual_content != expected_content:
            return output_path
    else:
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(expected_content)
        print(f"Processed '{input_path}' -> '{output_path}'")
    return None


def process_path(path, check_only=False):
    """Processes all .tmpl.md files in a path.

  Args:
    path: The file or directory to search for .tmpl.md files.
    check_only: If True, checks for stale files without writing them.

  Returns:
    A list of stale file paths if check_only is True, otherwise an empty list.
  """
    stale_files = []
    if os.path.isdir(path):
        for root, _, files in os.walk(path):
            for file in files:
                if file.endswith(".tmpl.md"):
                    stale_file = process_file(os.path.join(root, file),
                                              check_only)
                    if stale_file:
                        stale_files.append(stale_file)
    elif os.path.isfile(path):
        if not path.endswith(".tmpl.md"):
            sys.stderr.write(
                f"Expected path to have .tmpl.md suffix: {path}\n")
            sys.exit(1)
        stale_file = process_file(path, check_only)
        if stale_file:
            stale_files.append(stale_file)
    else:
        sys.stderr.write(f"Path is not a file or directory: {path}\n")
        sys.exit(1)
    return stale_files


def main():
    """Sets the directory to process and runs the processor."""
    parser = argparse.ArgumentParser(
        description="Process annotated markdown files.")
    parser.add_argument("--check",
                        action="store_true",
                        help="Check for stale files without modifying them.")
    parser.add_argument(
        "--path",
        help="File or directory to process. Defaults to the script's directory."
    )
    args = parser.parse_args()

    path_to_process = args.path
    if not path_to_process:
        path_to_process = os.path.dirname(os.path.abspath(__file__))

    stale_files = process_path(path_to_process, check_only=args.check)

    if args.check and stale_files:
        print("The following files are stale:")
        for f in stale_files:
            print(f"  {f}")
        print("\nPlease run the following command to update them:")
        without_check = list(sys.argv)
        without_check.remove('--check')
        print("  " + shlex.join(without_check))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
