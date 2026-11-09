#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unified CLI runner for Chromium Code Health Hub cleanups."""

import argparse
import collections
import importlib
import importlib.util
import json
import os
import posixpath
import random
import subprocess
import sys

# Add the scripts directory to path to allow loading plugins
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import hub_utils  # pylint: disable=wrong-import-position

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
    """Walks the repo, filters by skip dirs/extensions, shuffles directories.

    Directories (and files within them) are shuffled to maintain random
    sampling.
    """
    skip_dirs = GLOBAL_SKIP_DIRS.copy()
    if plugin_skip_dirs:
        skip_dirs.update(plugin_skip_dirs)

    abs_search_root = os.path.abspath(search_root)
    raw_files = []

    try:
        # Use git ls-files for speed
        result = subprocess.run(
            ['git', '-c', 'core.quotePath=false', 'ls-files'],
            stdout=subprocess.PIPE,
            text=True,
            check=True,
            cwd=abs_search_root,
        )
        for line in result.stdout.splitlines():
            raw_files.append(os.path.join(abs_search_root, line.strip()))
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

        # Normalize to a path relative to abs_search_root (independent of CWD).
        f = os.path.normpath(os.path.relpath(f, abs_search_root))

        # Filter by skip directories
        path_parts = f.split(os.sep)
        if any(part in skip_dirs for part in path_parts):
            continue

        directory = os.path.dirname(f)
        dir_to_files[directory].append(f)

    # Shuffle directories to maintain random sampling.
    directories = list(dir_to_files.keys())
    random.shuffle(directories)

    # Return list of (directory, files) tuples, keeping directories contiguous
    ordered_dirs_with_files = []
    for directory in directories:
        files_in_dir = dir_to_files[directory]
        random.shuffle(files_in_dir)
        ordered_dirs_with_files.append((directory, files_in_dir))

    return ordered_dirs_with_files


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


def _pick_subsystem(subsystem_map, batch_size):
    """Picks a subsystem, preferring full batches, then 3+ candidates.

    One is chosen at random to prevent collisions between users.
    """
    min_count = 3
    full_subsystems = [
        sub for sub, files in subsystem_map.items() if len(files) >= batch_size
    ]
    good_subsystems = [
        sub for sub, files in subsystem_map.items() if len(files) >= min_count
    ]
    if full_subsystems:
        tier = full_subsystems
    elif good_subsystems:
        tier = good_subsystems
    else:
        tier = list(subsystem_map.keys())

    return random.choice(tier)


def handle_grouped_mode(
    candidates,
    batch_size: int,
    selected_subsystem: str = None,
    exclude=None,
    json_output=False,
):
    """Groups file-level candidates by subsystem and prints a batch."""
    exclude_set = set(exclude or [])
    subsystem_map = collections.defaultdict(list)
    for c in candidates:
        filepath = c.get("file")
        if filepath:
            subsystem = get_subsystem_prefix(filepath)
            # Exclude by candidate name, file path, or containing subsystem.
            if exclude_set & {c.get("name"), filepath, subsystem}:
                continue
            subsystem_map[subsystem].append(c)

    if not subsystem_map:
        if json_output:
            print(
                json.dumps(
                    {
                        "mode": "grouped",
                        "subsystem": selected_subsystem,
                        "count": 0,
                        "candidates": [],
                        "metadata": {},
                    },
                    indent=2,
                )
            )
        else:
            print("No candidates found.")
        return

    if not selected_subsystem or selected_subsystem not in subsystem_map:
        # Also reselect when the requested subsystem was fully excluded.
        selected_subsystem = _pick_subsystem(subsystem_map, batch_size)

    batch_candidates = subsystem_map[selected_subsystem]
    random.shuffle(batch_candidates)
    batch = batch_candidates[:batch_size]

    metadata_keys = set()
    for c in batch:
        metadata_keys.update(
            k for k in c.keys() if k not in ("file", "subsystem")
        )

    if json_output:
        metadata = {}
        for key in sorted(metadata_keys):
            values = set()
            for c in batch:
                val = c.get(key)
                if isinstance(val, (list, set)):
                    values.update(val)
                elif val:
                    values.add(str(val))
            if values:
                metadata[key] = sorted(values)
        print(
            json.dumps(
                {
                    "mode": "grouped",
                    "subsystem": selected_subsystem,
                    "count": len(batch),
                    "candidates": batch,
                    "metadata": metadata,
                },
                indent=2,
            )
        )
        return

    print("Candidate Batch Found:")
    print(f"Subsystem: {selected_subsystem}")
    print(f"File Count: {len(batch)}")
    print("Files:")
    for c in batch:
        print(f"  - {c['file']}")

    if metadata_keys:
        print("\nBatch Metadata:")
        for key in sorted(metadata_keys):
            values = set()
            for c in batch:
                val = c.get(key)
                if isinstance(val, (list, set)):
                    values.update(val)
                elif val:
                    values.add(str(val))
            if values:
                print(f"Unique {key.capitalize()}:")
                for val in sorted(values)[:10]:
                    print(f"  - {val}")
                if len(values) > 10:
                    print(f"  - ... and {len(values) - 10} more")


def _path_sort_key(path):
    """Case-insensitive path component key (ties broken case-sensitively).

    Comparing per-component tuples keeps 'dir/sub' next to 'dir' rather than
    letting 'dir-other' interleave between them, and sorts parents before
    their children.
    """
    parts = [p for p in str(path or "").replace("\\", "/").split("/") if p]
    return tuple((p.lower(), p) for p in parts)


def _atomic_sort_key(item):
    """Sort key ordering by directory, then file name, then name/id.

    All comparisons are case-insensitive, with ties broken case-sensitively.
    Non-dict items sort by their string form only.
    """
    if isinstance(item, dict):
        file_path = str(item.get("file") or "").replace("\\", "/")
        directory = posixpath.dirname(file_path)
        basename = posixpath.basename(file_path)
        ident = str(item.get("name") or item.get("id") or "")
        return (
            _path_sort_key(directory),
            (basename.lower(), basename),
            (ident.lower(), ident),
        )
    return ((), ("", ""), (str(item).lower(), str(item)))


def handle_atomic_mode(
    plugin,
    count: int,
    repo_root: str,
    exclude=None,
    json_output=False,
    sort: str = "random",
):
    """Handles resource-centric cleanups by sampling and printing them."""
    exclude_set = set(exclude or [])
    # Random mode consumes only a small pool for sampling; asc/desc consume
    # every candidate so they can be sorted strictly by directory path.
    candidates_list = []
    pool_limit = max(count * 3, 10)
    discovered = plugin.find_candidates(repo_root)
    for c in discovered:
        if isinstance(c, dict):
            # Exclude by name, id, or file path.
            excluded = exclude_set & {
                c.get("name"),
                c.get("id"),
                c.get("file"),
            }
        else:
            excluded = c in exclude_set
        if excluded:
            continue
        if isinstance(c, dict) and "related_histograms" in c:
            # Copy so the plugin's dict isn't mutated.
            c = dict(c)
            related = [
                r for r in c["related_histograms"] if r not in exclude_set
            ]
            if related:
                c["related_histograms"] = related
            else:
                del c["related_histograms"]
        candidates_list.append(c)
        if sort == "random":
            if len(candidates_list) >= pool_limit:
                break

    if not candidates_list:
        if json_output:
            print(
                json.dumps(
                    {"mode": "atomic", "count": 0, "candidates": []}, indent=2
                )
            )
        else:
            print("No candidates found.")
        return

    if sort == "random":
        sampled = random.sample(
            candidates_list, k=min(len(candidates_list), count)
        )
    else:
        # Sort by directory path regardless of any order the plugin yields.
        candidates_list.sort(key=_atomic_sort_key, reverse=(sort == "desc"))
        sampled = candidates_list[:count]

    if json_output:
        print(
            json.dumps(
                {
                    "mode": "atomic",
                    "count": len(sampled),
                    "candidates": sampled,
                },
                indent=2,
            )
        )
        return

    for h in sampled:
        print("Candidate Found:")
        for key, val in h.items():
            if key == "summary" and val:
                val = val[:150] + "..." if len(val) > 150 else val
            if isinstance(val, list):
                val = ", ".join(val) if val else "None"
            print(f"{key.capitalize()}: {val}")
        print("---")


def _load_exclude_file(path: str) -> list[str]:
    """Loads candidate identifiers to exclude from a text file or JSON ledger.

    A '.json' file must hold a dict. If it has 'uploaded' and/or 'skipped'
    dicts (grouped ledger), their keys are used; otherwise the top-level keys
    are used. Any other file is read as one identifier per line, ignoring
    blank lines and lines starting with '#'.

    Raises OSError if the file can't be read, and ValueError if a JSON file
    is malformed or isn't a dict.
    """
    if path.endswith(".json"):
        with open(path, encoding="utf-8") as f:
            try:
                data = json.load(f)
            except json.JSONDecodeError as e:
                raise ValueError(f"Invalid JSON in exclude file {path}: {e}")
        if not isinstance(data, dict):
            raise ValueError(
                f"Exclude file {path} must contain a JSON object, got "
                f"{type(data).__name__}."
            )
        if "uploaded" in data or "skipped" in data:
            entries = []
            for section in ("uploaded", "skipped"):
                value = data.get(section)
                if isinstance(value, dict):
                    entries.extend(value.keys())
            return entries
        return list(data.keys())

    entries = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                entries.append(line)
    return entries


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
    parser.add_argument(
        "--exclude",
        nargs="*",
        action="extend",
        default=[],
        help="Candidate names or identifiers to exclude from results",
    )
    parser.add_argument(
        "--exclude-file",
        action="append",
        default=[],
        help=(
            "File(s) containing candidate identifiers or a JSON ledger to "
            "exclude"
        ),
    )
    parser.add_argument(
        "--sort",
        choices=["random", "asc", "desc"],
        default="random",
        help=(
            "Candidate ordering. asc/desc (alphabetical by path) are only "
            "supported for atomic plugins"
        ),
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output raw candidate data as JSON without truncation",
    )

    args = parser.parse_args()

    for exclude_file in args.exclude_file:
        args.exclude.extend(_load_exclude_file(exclude_file))

    search_root = (
        args.search_root if args.search_root else hub_utils.find_repo_root()
    )

    plugin = load_plugin_from_path(args.plugin)
    mode = getattr(plugin, "MODE", "atomic")

    if mode == "grouped" and args.sort != "random":
        parser.error("--sort asc/desc is only supported for atomic plugins.")

    if mode == "grouped":
        exclude_set = set(args.exclude or [])
        # Get files using the shared, optimized walker
        plugin_skip_dirs = getattr(plugin, "SKIP_DIRS", None)
        dirs_with_files = get_files(
            search_root,
            plugin.FILE_EXTENSIONS,
            plugin_skip_dirs,
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

        def is_excluded(filepath, name=None):
            # Exclude by candidate name, file path, or containing subsystem.
            return bool(
                exclude_set & {name, filepath, get_subsystem_prefix(filepath)}
            )

        def scan_dir(directory, files_in_dir):
            nonlocal scanned
            new_candidates = []
            if (
                files_in_dir
                and get_subsystem_prefix(files_in_dir[0]) in exclude_set
            ):
                # The whole subsystem is excluded, so skip scanning it.
                visited_dirs.add(directory)
                return new_candidates
            if has_check_directory:
                results = plugin.check_directory(
                    directory, files_in_dir, abs_search_root
                )
                if results:
                    for r in results:
                        f = r.get("file")
                        if f and f not in seen_files:
                            if is_excluded(f, r.get("name")):
                                continue
                            seen_files.add(f)
                            new_candidates.append(r)
            else:
                for f in files_in_dir:
                    if f in seen_files:
                        continue
                    if is_excluded(f):
                        continue
                    metadata = plugin.check_file(f, abs_search_root)
                    if metadata:
                        if is_excluded(f, metadata.get("name")):
                            continue
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
            # Delegate so --json emits valid empty JSON instead of plain text.
            handle_grouped_mode(
                candidates_list,
                args.batch_size,
                None,
                exclude=args.exclude,
                json_output=args.json,
            )
            return

        # Map candidates by subsystem
        subsystem_map = collections.defaultdict(list)
        for c in candidates_list:
            filepath = c.get("file")
            if filepath:
                subsystem = get_subsystem_prefix(filepath)
                subsystem_map[subsystem].append(c)

        # Stage 2: Subsystem Selection
        selected_subsystem = _pick_subsystem(subsystem_map, args.batch_size)

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
                and get_subsystem_prefix(files[0]) not in exclude_set
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
            candidates_list,
            args.batch_size,
            selected_subsystem,
            exclude=args.exclude,
            json_output=args.json,
        )
    else:
        handle_atomic_mode(
            plugin,
            args.count,
            search_root,
            exclude=args.exclude,
            json_output=args.json,
            sort=args.sort,
        )


if __name__ == "__main__":
    main()
