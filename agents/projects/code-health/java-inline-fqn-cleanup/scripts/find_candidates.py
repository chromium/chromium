#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to find candidate directories for Java inline FQN cleanup."""

import collections
import functools
import os
import random
import re
import subprocess
import sys

SCAN_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 "../../../../../../../app_rating/src"))
if not os.path.exists(SCAN_DIR):
    SCAN_DIR = os.path.join(os.getcwd(), 'chrome', 'android')

SKIP_DIRS = {
    ".git", ".cipd", "out", "build", "testing", "tools", "infra",
    "third_party", "clank"
}
FQN_REGEX = re.compile(
    r"\b(org\.chromium|android|com\.google|androidx|java|javax)"
    r"(?:\.[a-zA-Z0-9_]+)+\b")
URL_REGEX = re.compile(r"https?://\S+")

TARGET_BATCH_SIZE = 10
PROBE_BATCH_SIZE = 100
SIBLING_SCAN_BATCH_SIZE = 150


def should_skip_line(line):
    line = line.strip()
    if not line:
        return True
    if line.startswith("import ") or line.startswith("package "):
        return True
    if line.startswith("//") or line.startswith("/*") or line.startswith("*"):
        return True
    return False


def get_file_fqns(file_path):
    fqns = set()
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            for idx, line in enumerate(f, start=1):
                # Skip license headers in first 15 lines
                if idx <= 15:
                    continue
                if should_skip_line(line):
                    continue
                # Remove URLs to avoid matching domain names in comments
                cleaned_line = URL_REGEX.sub("", line)
                for match in FQN_REGEX.finditer(cleaned_line):
                    fqn = match.group(0)

                    # Skip 'R' classes (e.g. android.R.attr) to avoid
                    # conflicting with local R imports.
                    if ".R." in fqn:
                        continue

                    if len(fqn.split(".")) >= 3:
                        fqns.add(fqn)
    except Exception:
        pass
    return fqns


@functools.lru_cache(maxsize=None)
def is_valid_import(fqn):
    # Check if the FQN or its parent classes are imported anywhere in codebase
    parts = fqn.split(".")

    # Try the full FQN, then progressively drop the last part (down to 2 parts)
    for i in range(len(parts), 1, -1):
        test_fqn = ".".join(parts[:i])
        # Escape dots for the regex
        regex_fqn = test_fqn.replace(".", r"\.")
        try:
            res = subprocess.run([
                "git", "grep", "-q", "-E", f"import (static )?{regex_fqn}[;.]",
                "--", ":/*.java"
            ],
                                 cwd=SCAN_DIR,
                                 capture_output=True,
                                 check=False)
            if res.returncode == 0:
                return True
        except Exception:
            # Fallback to true if git grep fails for some reason
            return True

    return False


def get_package_from_path(file_path):
    """Extracts the Java package name from a given file path."""
    parts = file_path.split(os.sep)
    for idx, part in enumerate(parts):
        if part in ('org', 'com', 'net'):
            return '.'.join(parts[idx:-1])
    return os.path.dirname(file_path)


def get_random_discovery_sample(search_root):
    """Retrieves tracked Java files, falling back to os.walk if needed."""
    all_java_files = []
    try:
        git_cmd = ['git', 'ls-files']
        result = subprocess.run(git_cmd,
                                stdout=subprocess.PIPE,
                                text=True,
                                check=True,
                                cwd=search_root)
        for line in result.stdout.splitlines():
            if line.endswith('.java') and not any(
                    skip in line
                    for skip in ['javatests', 'junit', 'test', 'third_party']):
                all_java_files.append(os.path.join(search_root, line.strip()))
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Fallback to os.walk if search_root is not a git repo or git is
        # not installed.
        for root, dirs, files in os.walk(search_root):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            if "javatests" in root or "junit" in root or "test" in root:
                continue
            for sf in files:
                if sf.endswith('.java') and not any(
                        skip in sf.lower()
                        for skip in ['test', 'junit', 'third_party']):
                    all_java_files.append(os.path.join(root, sf))

    if not all_java_files:
        print("No java files found.")
        return []

    # 1. Group files by package name.
    package_to_files = collections.defaultdict(list)
    for f in all_java_files:
        pkg = get_package_from_path(f)
        package_to_files[pkg].append(f)

    # 2. Shuffle files inside each package.
    for files in package_to_files.values():
        random.shuffle(files)

    # 3. Shuffle the package names.
    packages = list(package_to_files.keys())
    random.shuffle(packages)

    # 4. Mix files using round-robin (take 1st file of each package,
    # then 2nd, etc.).
    sampled_files = []
    max_files = max(
        len(files)
        for files in package_to_files.values()) if package_to_files else 0
    for i in range(max_files):
        for pkg in packages:
            if i < len(package_to_files[pkg]):
                sampled_files.append(package_to_files[pkg][i])

    return sampled_files


def scan_for_candidates(files):
    """Scans a list of files for valid inline FQN candidates."""
    warnings_map = {}
    for f in files:
        fqns = get_file_fqns(f)
        if fqns:
            valid_fqns = set()
            banned_fqns = set()
            for fqn in fqns:
                if is_valid_import(fqn):
                    valid_fqns.add(fqn)
                else:
                    banned_fqns.add(fqn)
            # Only consider a file a candidate if it has at least one valid
            # FQN to clean.
            if valid_fqns:
                warnings_map[f] = {'valid': valid_fqns, 'banned': banned_fqns}
    return warnings_map


def extract_package(file_path):
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("package "):
                    return line.strip().split(" ")[1].rstrip(";")
    except Exception:
        pass
    return "Unknown"


def build_and_print_batch(anchor_file, warnings_map, search_root):
    """Expands search radially outward from anchor file to fill pool."""
    batch_files = [anchor_file]
    anchor_dir = os.path.dirname(anchor_file)
    checked_files = {anchor_file}

    curr_dir = anchor_dir
    skip_dir = None

    while len(
            batch_files) < TARGET_BATCH_SIZE and curr_dir and curr_dir != '/':
        if skip_dir:
            print(
                f"Expanding search up to: {curr_dir} "
                f"(Candidates found: {len(batch_files)}/{TARGET_BATCH_SIZE})")
        else:
            print(
                f"Searching for siblings in: {curr_dir} "
                f"(Candidates found: {len(batch_files)}/{TARGET_BATCH_SIZE})")

        other_files = []
        for sub_root, sub_dirs, sub_files in os.walk(curr_dir):
            if skip_dir and (sub_root == skip_dir
                             or sub_root.startswith(skip_dir + os.sep)):
                sub_dirs[:] = []
                continue

            sub_dirs[:] = [d for d in sub_dirs if d not in SKIP_DIRS]
            if any(part in sub_root
                   for part in ("javatests", "junit", "test")):
                continue

            for sf in sub_files:
                if sf.endswith('.java'):
                    full_path = os.path.join(sub_root, sf)
                    if full_path not in checked_files:
                        other_files.append(full_path)

        # Shuffle to get a representative sample across all sibling packages.
        random.shuffle(other_files)

        # Scan sibling files in batches of SIBLING_SCAN_BATCH_SIZE until
        # we have enough candidates.
        for idx in range(0, len(other_files), SIBLING_SCAN_BATCH_SIZE):
            batch = other_files[idx:idx + SIBLING_SCAN_BATCH_SIZE]
            checked_files.update(batch)

            chunk_warnings = scan_for_candidates(batch)
            for f, warns in chunk_warnings.items():
                if f not in batch_files:
                    batch_files.append(f)
                    warnings_map[f] = warns

            if len(batch_files) >= TARGET_BATCH_SIZE:
                break

        if curr_dir == search_root or not curr_dir.startswith(search_root):
            break

        parent_dir = os.path.dirname(curr_dir)
        if not parent_dir or parent_dir == curr_dir:
            break

        skip_dir = curr_dir
        curr_dir = parent_dir

    # Output the candidate batch.
    print_proposed_batch(batch_files[:TARGET_BATCH_SIZE], anchor_dir,
                         warnings_map, search_root)
    return True


def print_proposed_batch(batch_files, anchor_dir, warnings_map, search_root):
    """Outputs the details of the constructed batch."""
    package_name = extract_package(
        batch_files[0]) if batch_files else "Unknown"
    rel_dir = os.path.relpath(anchor_dir, search_root)

    all_valid_fqns = set()
    all_banned_fqns = set()
    for f in batch_files:
        if f in warnings_map:
            all_valid_fqns.update(warnings_map[f]['valid'])
            all_banned_fqns.update(warnings_map[f]['banned'])

    print("Candidate Batch Found:")
    print(f"Package: {package_name}")
    print(f"Directory: {rel_dir}")
    print(f"File Count: {len(batch_files)}")
    print("Files:")
    for f in batch_files:
        print(f"  - {f}")

    print("Unique FQNs/Imports to Clean:")
    sorted_fqns = sorted(list(all_valid_fqns))
    for fqn in sorted_fqns[:5]:
        print(f"  - {fqn}")
    if len(sorted_fqns) > 5:
        print(f"  - ... and {len(sorted_fqns) - 5} more")

    if all_banned_fqns:
        print("Banned FQNs (DO NOT TOUCH THESE):")
        for fqn in sorted(list(all_banned_fqns)):
            print(f"  - {fqn}")


def main():
    search_root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        'chrome', 'android')
    print(f"Scanning first-party Java files in {search_root}...",
          file=sys.stderr)

    all_files = get_random_discovery_sample(search_root)
    if not all_files:
        return 0

    # Continuously pull batches of files until we find an anchor candidate.
    for batch_start in range(0, len(all_files), PROBE_BATCH_SIZE):
        sample_files = all_files[batch_start:batch_start + PROBE_BATCH_SIZE]
        print(
            f"\n--- Testing a new random batch of {len(sample_files)} "
            "files ---",
            file=sys.stderr)

        # Batch scan the files for candidates.
        warnings_map = scan_for_candidates(sample_files)

        # Find the first file that has candidates to use as our anchor.
        anchor_file = None
        for f in sample_files:
            if f in warnings_map:
                anchor_file = f
                break

        if anchor_file:
            print(
                f"Found anchor file with inline FQN candidates: {anchor_file}",
                file=sys.stderr)

            # We found an anchor! Expand the batch around this file.
            if build_and_print_batch(anchor_file, warnings_map, search_root):
                return 0

    print("No suitable batches found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
