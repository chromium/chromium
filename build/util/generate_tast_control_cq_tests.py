#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Populates chromeos/tast_control_cq_tests.txt
from test_metadata.jsonpb and chromeos/tast_control_additional_cq_tests.txt.
"""

import argparse
import glob
import json
import os
import sys


def print_warnings(tests, template, **kwargs):
    """Prints warnings for a list of tests using template.

    The template can use placeholders: {tests}, {Tests}, {tests_list}, {them},
    {they}, {are}, {do}, {have}, and any key-value pairs in kwargs.
    Any parts of the template enclosed in ** will be bolded.
    """
    if not tests:
        return
    is_singular = len(tests) == 1
    context = {
        "tests_list": ", ".join(sorted(tests)),
        "tests": "test" if is_singular else "tests",
        "Tests": "Test" if is_singular else "Tests",
        "them": "it" if is_singular else "them",
        "they": "it" if is_singular else "they",
        "are": "is" if is_singular else "are",
        "do": "does" if is_singular else "do",
        "have": "has" if is_singular else "have",
    }
    context.update(kwargs)
    warning_msg = template.format(**context)

    if sys.stderr.isatty():
        parts = warning_msg.split("**")
        res = []
        for i, part in enumerate(parts):
            if i % 2 == 1:
                # \033[1m: Bold, \033[22m: Normal intensity (ends bold)
                res.append(f"\033[1m{part}\033[22m")
            else:
                res.append(part)
        # \033[33m: Yellow warning color, \033[0m: Reset all attributes
        warning_msg = f"\033[33m{''.join(res)}\033[0m"
    else:
        warning_msg = warning_msg.replace("**", "")
    print(warning_msg, file=sys.stderr)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-o",
        "--output",
        required=True,
        help="Output tast_control_cq_tests.txt file.",
    )
    parser.add_argument(
        "-a",
        "--additional-tests",
        required=True,
        help="Input additional CQ tests file.",
    )
    parser.add_argument(
        "-m",
        "--metadata-dir",
        required=True,
        help="Input test_metadata.jsonpb cache directory.",
    )
    parser.add_argument(
        "-b", "--boards", help="Comma or colon separated list of CROS boards."
    )

    args = parser.parse_known_args(argv[1:])[0]

    # Determine target board
    board = None
    if args.boards:
        # Split by colon or comma and get first non-empty board name
        boards = [
            b.strip()
            for b in args.boards.replace(',', ':').split(':')
            if b.strip()
        ]
        if boards:
            board = boards[0]

    if not board:
        print(
            (
                "Error: No CROS board specified."
                " Please set cros_boards in gclient or pass via --boards."
            ),
            file=sys.stderr,
        )
        return 1

    src_dir = os.path.normpath(
        os.path.join(os.path.dirname(__file__), '..', '..')
    )
    lkgm_path = os.path.join(src_dir, 'chromeos', 'CHROMEOS_LKGM')

    if not os.path.exists(lkgm_path):
        print(f"Error: LKGM file not found at {lkgm_path}", file=sys.stderr)
        return 1

    with open(lkgm_path, 'r') as f:
        lkgm_version = f.read().strip()

    if not lkgm_version:
        print(f"Error: LKGM version is empty in {lkgm_path}", file=sys.stderr)
        return 1

    # Read additional tests
    additional_tests = set()
    if not os.path.exists(args.additional_tests):
        print(
            f"Error: Additional tests file not found at {args.additional_tests}",
            file=sys.stderr,
        )
        return 1

    with open(args.additional_tests, 'r') as f:
        for line in f:
            stripped = line.strip()
            if not stripped or stripped.startswith('#'):
                continue
            additional_tests.add(stripped)

    # Read test_metadata.jsonpb using full-version mapping
    misc_dir = os.path.dirname(args.metadata_dir)
    full_version_filename = f"full-version+{board}+{lkgm_version}"
    full_version_path = os.path.join(misc_dir, full_version_filename)

    if not os.path.exists(full_version_path):
        print(
            f"Error: Full version mapping file not found at {full_version_path}",
            file=sys.stderr,
        )
        return 1

    with open(full_version_path, 'r') as f:
        full_version = f.read().strip()

    if not full_version:
        print(
            f"Error: Full version string is empty in {full_version_path}",
            file=sys.stderr,
        )
        return 1

    metadata_files = glob.glob(
        os.path.join(
            args.metadata_dir, f"*{full_version}*-metadata-test_metadata.jsonpb"
        )
    )
    if not metadata_files:
        print(
            (
                f"Error: No matching test_metadata.jsonpb found in {args.metadata_dir}"
                f"for full version {full_version}"
            ),
            file=sys.stderr,
        )
        return 1

    metadata_file = metadata_files[0]

    with open(metadata_file, 'r') as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError as e:
            print(
                f"Error: Failed to parse JSON from {metadata_file}: {e}",
                file=sys.stderr,
            )
            return 1

    cq_tests = set()
    non_mainline_tests = []
    invalid_additional_tests = []
    no_dep_chrome_additional_tests = []
    processed_additional_tests = set()

    for entry in data.get("values", []):
        tc = entry.get("test_case", {})
        tc_id = tc.get("id", {}).get("value", "")
        tags = [tag.get("value", "") for tag in tc.get("tags", [])]

        # Filter by group:cq-medium && dep:chrome && !informational
        has_mainline = "group:mainline" in tags
        has_cq_medium = "group:cq-medium" in tags
        has_dep_chrome = "dep:chrome" in tags
        is_informational = "informational" in tags

        if has_cq_medium and has_dep_chrome and not is_informational:
            if not tc_id.startswith("tast."):
                continue
            if not has_mainline:
                non_mainline_tests.append(tc_id)
            else:
                cq_tests.add(tc_id)

        if tc_id in additional_tests:
            processed_additional_tests.add(tc_id)
            if not has_mainline or is_informational:
                invalid_additional_tests.append(tc_id)
            else:
                if not has_dep_chrome:
                    no_dep_chrome_additional_tests.append(tc_id)
                cq_tests.add(tc_id)

    # Add unprocessed additional tests (tests not found in metadata at all)
    unprocessed_additional_tests = list(
        additional_tests - processed_additional_tests
    )
    for tc_id in unprocessed_additional_tests:
        cq_tests.add(tc_id)

    print_warnings(
        non_mainline_tests,
        "Warning: {Tests} {tests_list} {are} in group:cq-medium, but "
        "**not in group:mainline (ignored/not added to CQ)**. "
        "Please consider fixing {them}.",
    )

    print_warnings(
        invalid_additional_tests,
        "Warning: Additional {tests} {tests_list} {are} **non-mainline "
        "or informational (ignored/not added to CQ)**. "
        "Please consider fixing or removing {them} "
        "from {additional_tests_path}.",
        additional_tests_path=args.additional_tests,
    )

    print_warnings(
        no_dep_chrome_additional_tests,
        "Warning: Additional {tests} {tests_list} {do} **not have "
        "dep:chrome**, but {are} added to final list and will run on CQ "
        "(defined in {additional_tests_path}).",
        additional_tests_path=args.additional_tests,
    )

    print_warnings(
        unprocessed_additional_tests,
        "Warning: Additional {tests} {tests_list} {do} **not exist in "
        "metadata (is it a typo?)**, but {are} added to final list anyway "
        "(but may not run since {they} may not exist at all) "
        "from {additional_tests_path}.",
        additional_tests_path=args.additional_tests,
    )

    # Write generated output file
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, 'w') as out:
        out.write(
            f"# Autogenerated by build/util/generate_tast_control_cq_tests.py\n"
        )
        out.write(f"# Do not edit.\n\n")
        for test in sorted(cq_tests):
            out.write(f"{test}\n")

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
