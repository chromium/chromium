#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unified CLI runner for Chromium Code Health Hub cleanups."""
# pylint: disable=line-too-long

import argparse
import collections
import importlib
import importlib.util
import os
import random
import subprocess
import sys

# Add the scripts directory to path to allow loading plugins
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# Folders that should always be ignored for all cleanups
GLOBAL_SKIP_DIRS = {
    ".git",
    ".cipd",
    "out",
    "build",
    "infra",
    "third_party",
    "node_modules",
    "docs",
    "clank",
}


def get_repo_root() -> str:
    """Finds the root of the git repository."""
    try:
        res = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
        )
        return res.stdout.strip()
    except Exception:
        return os.getcwd()


def normalize_path(filepath: str) -> str:
    """Translates legacy Java test paths to production paths for grouping."""
    replacements = [
        (
            "chrome/android/javatests/src/org/chromium/chrome/browser/",
            "chrome/browser/",
        ),
        (
            "chrome/android/junit/src/org/chromium/chrome/browser/",
            "chrome/browser/",
        ),
        (
            "chrome/android/java/src/org/chromium/chrome/browser/",
            "chrome/browser/",
        ),
    ]
    norm_path = filepath
    for src, dst in replacements:
        if src in norm_path:
            norm_path = norm_path.replace(src, dst)
            break
    return norm_path


def get_subsystem_prefix(filepath: str) -> str:
    """Extracts a subsystem prefix using hybrid depth for better grouping."""
    norm_path = normalize_path(filepath)
    parts = norm_path.split(os.sep)

    # Large monolithic folders that need deeper nesting
    if (
        len(parts) >= 3
        and parts[0] == "chrome"
        and parts[1] in ("browser", "android")
    ):
        depth = 3
    else:
        depth = 2

    depth = min(len(parts) - 1, depth)
    if depth > 0:
        return os.sep.join(parts[:depth])
    return os.path.dirname(filepath)


def get_files(search_root, extensions, plugin_skip_dirs=None):
    """Walks the repo, filters by skip dirs/extensions, shuffles directories."""
    skip_dirs = GLOBAL_SKIP_DIRS.copy()
    if plugin_skip_dirs:
        skip_dirs.update(plugin_skip_dirs)

    abs_search_root = os.path.abspath(search_root)
    raw_files = []

    try:
        # Use git ls-files for speed
        result = subprocess.run(
            ['git', 'ls-files'],
            stdout=subprocess.PIPE,
            text=True,
            check=True,
            cwd=abs_search_root,
        )
        for line in result.stdout.splitlines():
            raw_files.append(os.path.join(search_root, line.strip()))
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Fallback to os.walk
        for root, dirs, files in os.walk(abs_search_root):
            dirs[:] = [d for d in dirs if d not in skip_dirs]
            for sf in files:
                raw_files.append(os.path.join(root, sf))

    # Group files by directory
    dir_to_files = collections.defaultdict(list)
    for f in raw_files:
        # Filter by extensions
        if not any(f.endswith(ext) for ext in extensions):
            continue

        # Normalize and ensure relative path to repository CWD
        if os.path.isabs(f):
            f = os.path.relpath(f, os.getcwd())
        f = os.path.normpath(f)

        # Filter by skip directories
        path_parts = f.split(os.sep)
        if any(part in skip_dirs for part in path_parts):
            continue

        directory = os.path.dirname(f)
        dir_to_files[directory].append(f)

    # Shuffle directories to maintain random sampling
    directories = list(dir_to_files.keys())
    random.shuffle(directories)

    # Return list of (directory, files) tuples, keeping directories contiguous
    shuffled_dirs_with_files = []
    for directory in directories:
        files_in_dir = dir_to_files[directory]
        random.shuffle(files_in_dir)
        shuffled_dirs_with_files.append((directory, files_in_dir))

    return shuffled_dirs_with_files


def load_plugin_from_path(file_path: str):
    """Loads a Python module dynamically from a file path."""
    if not os.path.exists(file_path):
        print(
            f"Error: Plugin file '{file_path}' does not exist.", file=sys.stderr
        )
        sys.exit(1)

    spec = importlib.util.spec_from_file_location(
        "code_health_plugin", file_path
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules["code_health_plugin"] = module
    spec.loader.exec_module(module)
    return module


def handle_grouped_mode(
    candidates, batch_size: int, selected_subsystem: str = None
):
    """Groups file-level candidates by subsystem and prints a batch."""
    subsystem_map = collections.defaultdict(list)
    for c in candidates:
        filepath = c.get("file")
        if filepath:
            subsystem = get_subsystem_prefix(filepath)
            subsystem_map[subsystem].append(c)

    if not subsystem_map:
        print("No candidates found.")
        return

    if not selected_subsystem:
        # Prioritize full batches: if any subsystem reached batch_size, pick randomly among full ones
        full_subsystems = [
            sub
            for sub, files in subsystem_map.items()
            if len(files) >= batch_size
        ]
        if full_subsystems:
            selected_subsystem = random.choice(full_subsystems)
        else:
            # Pick a subsystem using threshold-based random selection to prevent collisions between users
            min_count = 3
            good_subsystems = [
                sub
                for sub, files in subsystem_map.items()
                if len(files) >= min_count
            ]

            if good_subsystems:
                selected_subsystem = random.choice(good_subsystems)
            else:
                selected_subsystem = random.choice(list(subsystem_map.keys()))

    batch_candidates = subsystem_map[selected_subsystem]

    random.shuffle(batch_candidates)
    batch = batch_candidates[:batch_size]

    print("Candidate Batch Found:")
    print(f"Subsystem: {selected_subsystem}")
    print(f"File Count: {len(batch)}")
    print("Files:")
    for c in batch:
        print(f"  - {c['file']}")

    metadata_keys = set()
    for c in batch:
        metadata_keys.update(
            k for k in c.keys() if k not in ("file", "subsystem")
        )

    if metadata_keys:
        print("\nBatch Metadata:")
        for key in sorted(list(metadata_keys)):
            values = set()
            for c in batch:
                val = c.get(key)
                if isinstance(val, (list, set)):
                    values.update(val)
                elif val:
                    values.add(str(val))
            if values:
                print(f"Unique {key.capitalize()}:")
                for val in sorted(list(values))[:10]:
                    print(f"  - {val}")
                if len(values) > 10:
                    print(f"  - ... and {len(values) - 10} more")


def handle_atomic_mode(plugin, count: int, repo_root: str):
    """Handles resource-centric cleanups by sampling and printing them."""
    # Consume only enough candidates to satisfy count (with a small pool for sampling)
    candidates_list = []
    pool_limit = max(count * 3, 10)
    for c in plugin.find_candidates(repo_root):
        candidates_list.append(c)
        if len(candidates_list) >= pool_limit:
            break

    if not candidates_list:
        print("No candidates found.")
        return

    sampled = random.sample(candidates_list, k=min(len(candidates_list), count))

    for h in sampled:
        print("Candidate Found:")
        for key, val in h.items():
            if key == "summary" and val:
                val = val[:150] + "..." if len(val) > 150 else val
            if isinstance(val, list):
                val = ", ".join(val) if val else "None"
            print(f"{key.capitalize()}: {val}")
        print("---")


def main():
    parser = argparse.ArgumentParser(description="Chromium Code Health Hub CLI")
    parser.add_argument("subcommand", choices=["find"], help="Command to run")
    parser.add_argument(
        "--plugin", required=True, help="Path to the Python plugin file"
    )
    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help="Number of atomic candidates to return",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=10,
        help="Batch size for grouped cleanups",
    )
    parser.add_argument("--search-root", default=None, help="Directory to scan")

    args = parser.parse_args()

    search_root = args.search_root if args.search_root else get_repo_root()

    plugin = load_plugin_from_path(args.plugin)
    mode = getattr(plugin, "MODE", "atomic")

    if mode == "grouped":
        # Get files using the shared, optimized walker
        plugin_skip_dirs = getattr(plugin, "SKIP_DIRS", None)
        dirs_with_files = get_files(
            search_root, plugin.FILE_EXTENSIONS, plugin_skip_dirs
        )

        if hasattr(plugin, "initialize"):
            all_files = []
            for _, files in dirs_with_files:
                all_files.extend(files)
            plugin.initialize(search_root, all_files)

        candidates_list = []
        scanned = 0
        sys.stderr.write("Scanning for candidates...\n")

        abs_search_root = os.path.abspath(search_root)
        has_check_directory = hasattr(plugin, "check_directory")
        seen_files = set()
        visited_dirs = set()

        def scan_dir(directory, files_in_dir):
            nonlocal scanned
            new_candidates = []
            if has_check_directory:
                results = plugin.check_directory(
                    directory, files_in_dir, abs_search_root
                )
                if results:
                    for r in results:
                        f = r.get("file")
                        if f and f not in seen_files:
                            seen_files.add(f)
                            new_candidates.append(r)
            else:
                for f in files_in_dir:
                    if f in seen_files:
                        continue
                    metadata = plugin.check_file(f, abs_search_root)
                    if metadata:
                        seen_files.add(f)
                        new_candidates.append({"file": f, **metadata})
            scanned += len(files_in_dir)
            visited_dirs.add(directory)
            return new_candidates

        # Stage 1: Global Discovery (stop at batch_size matches or ~1,000 files scanned)
        for directory, files_in_dir in dirs_with_files:
            results = scan_dir(directory, files_in_dir)
            if results:
                candidates_list.extend(results)

            if candidates_list:
                subsystems = [
                    get_subsystem_prefix(c["file"]) for c in candidates_list
                ]
                counts = collections.Counter(subsystems)
                max_count = max(counts.values())

                # Stop early if ANY subsystem reaches full batch_size
                if max_count >= args.batch_size:
                    break

                # Stop broad discovery after 1,000 files if at least one candidate found
                if scanned >= 1000 and max_count >= 1:
                    break

            if scanned >= 5000:
                break

        if not candidates_list:
            print("No candidates found.")
            return

        # Map candidates by subsystem
        subsystem_map = collections.defaultdict(list)
        for c in candidates_list:
            filepath = c.get("file")
            if filepath:
                subsystem = get_subsystem_prefix(filepath)
                subsystem_map[subsystem].append(c)

        # Stage 2: Subsystem Selection
        full_subsystems = [
            sub
            for sub, files in subsystem_map.items()
            if len(files) >= args.batch_size
        ]
        if full_subsystems:
            selected_subsystem = random.choice(full_subsystems)
        else:
            min_count = 3
            good_subsystems = [
                sub
                for sub, files in subsystem_map.items()
                if len(files) >= min_count
            ]
            if good_subsystems:
                selected_subsystem = random.choice(good_subsystems)
            else:
                selected_subsystem = random.choice(list(subsystem_map.keys()))

        # Stage 3: Targeted Deep-Fill if batch is not full yet
        current_subsystem_candidates = subsystem_map[selected_subsystem]
        if len(current_subsystem_candidates) < args.batch_size:
            # Filter remaining unvisited directories for selected_subsystem
            # NOTE: Always use get_subsystem_prefix(files[0]) to avoid directory-path parsing asymmetry
            remaining_subsystem_dirs = [
                (d, files)
                for d, files in dirs_with_files
                if d not in visited_dirs
                and files
                and get_subsystem_prefix(files[0]) == selected_subsystem
            ]

            stage2_scanned = 0
            for directory, files_in_dir in remaining_subsystem_dirs:
                if len(current_subsystem_candidates) >= args.batch_size:
                    break
                if (
                    stage2_scanned >= 500
                ):  # Safety budget to avoid runaway latency
                    break

                results = scan_dir(directory, files_in_dir)
                stage2_scanned += len(files_in_dir)
                if results:
                    candidates_list.extend(results)
                    current_subsystem_candidates.extend(results)
                    if len(current_subsystem_candidates) >= args.batch_size:
                        break

        handle_grouped_mode(
            candidates_list, args.batch_size, selected_subsystem
        )
    else:
        handle_atomic_mode(plugin, args.count, get_repo_root())


if __name__ == "__main__":
    main()
