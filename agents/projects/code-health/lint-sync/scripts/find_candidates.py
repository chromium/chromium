# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find enums missing LINT sync guards."""
# pylint: disable=line-too-long

import collections
import json
import os
import random
import re
import subprocess
import sys

# Configuration for main runner
MODE = "atomic"


def get_xml_candidates(repo_root):
    """Fast search for all enum definitions in XML files."""
    hist_path = os.path.join(repo_root, "tools", "metrics", "histograms")
    cmd = ["rg", "--json", "<enum name=", hist_path]
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)

    candidates = []
    if result.returncode == 0:
        for line in result.stdout.splitlines():
            try:
                data = json.loads(line)
                if data["type"] == "match":
                    path = data["data"]["path"]["text"]
                    line_num = data["data"]["line_number"]
                    match = re.search(r'<enum name="([^"]+)">',
                                      data["data"]["lines"]["text"])
                    if match:
                        candidates.append((match.group(1), path, line_num))
            except (json.JSONDecodeError, KeyError):
                continue
    return candidates


def is_xml_unguarded(path, line_num):
    """Check if the XML enum is missing a LINT.IfChange guard using native Python."""
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
            start = max(0, line_num - 4)
            context = "".join(lines[start:line_num - 1])
            return "LINT.IfChange" not in context
    except Exception:
        return False


def find_source_files_batch(enum_names, repo_root):
    """Finds source files for a list of enums in a single batch using ripgrep."""
    enum_to_source = {}
    if not enum_names:
        return enum_to_source

    chrome_path = os.path.join(repo_root, "chrome")
    components_path = os.path.join(repo_root, "components")

    # Chunk enum names to avoid excessively long regex patterns
    chunk_size = 500
    for i in range(0, len(enum_names), chunk_size):
        chunk = enum_names[i:i + chunk_size]
        escaped_names = [re.escape(name) for name in chunk]
        pattern = r"\benum\s+(class\s+)?(" + "|".join(escaped_names) + r")\b"

        cmd = [
            "rg", "-n", "-H", "--no-heading", pattern, chrome_path,
            components_path
        ]
        result = subprocess.run(cmd,
                                capture_output=True,
                                text=True,
                                check=False)

        if result.returncode == 0 and result.stdout:
            for line in result.stdout.splitlines():
                parts = line.strip().split(":")
                if len(parts) >= 3:
                    filepath = parts[0]
                    try:
                        line_num = int(parts[1])
                    except ValueError:
                        continue

                    line_content = ":".join(parts[2:])
                    for name in chunk:
                        if re.search(r"\b" + re.escape(name) + r"\b",
                                     line_content):
                            if name not in enum_to_source:
                                enum_to_source[name] = (filepath, line_num)
                            break
    return enum_to_source


def find_candidates(repo_root):
    """Generator yielding file pairs that need LINT sync guards."""
    candidates = get_xml_candidates(repo_root)

    # Pre-group XML candidates by their XML path to avoid quadratic search
    xml_to_candidates = collections.defaultdict(list)
    enum_names = []
    for name, path, line in candidates:
        xml_to_candidates[path].append((name, line))
        enum_names.append(name)

    # Perform batch source file lookup (single ripgrep call!)
    sys.stderr.write(
        f"Pre-indexing source enums (searching for {len(enum_names)} enums in C++/Java)...\n"
    )
    enum_to_source = find_source_files_batch(enum_names, repo_root)
    sys.stderr.write(f"Indexed {len(enum_to_source)} matching source enums.\n")

    # Shuffle candidates to start search at random locations
    shuffled_candidates = list(candidates)
    random.shuffle(shuffled_candidates)

    processed_pairs = set()

    for enum_name, xml_path, xml_line in shuffled_candidates:
        if is_xml_unguarded(xml_path, xml_line):
            source_info = enum_to_source.get(enum_name)
            if source_info:
                source_path, line_num = source_info

                # Verify source file enum is unguarded (native Python check)
                try:
                    with open(source_path,
                              "r",
                              encoding="utf-8",
                              errors="ignore") as f:
                        lines = f.readlines()
                        start = max(0, line_num - 4)
                        context = "".join(lines[start:line_num - 1])
                        if "LINT.IfChange" in context:
                            continue
                except Exception:
                    continue

                pair_key = (source_path, xml_path)
                if pair_key in processed_pairs:
                    continue
                processed_pairs.add(pair_key)

                # Bundle other enums shared by these two files
                enums = {enum_name}
                try:
                    with open(source_path, 'r', encoding='utf-8') as f:
                        source_content = f.read()

                    for other_name, other_line in xml_to_candidates[xml_path]:
                        if other_name not in enums:
                            is_in_source = (
                                f"enum class {other_name}" in source_content
                                or f"enum {other_name}" in source_content)
                            if is_in_source and is_xml_unguarded(
                                    xml_path, other_line):
                                enums.add(other_name)
                except IOError:
                    continue

                if len(enums) >= 4:
                    rel_source = os.path.relpath(source_path, os.getcwd())
                    rel_xml = os.path.relpath(xml_path, os.getcwd())
                    yield {
                        "file": rel_source,
                        "xml": rel_xml,
                        "enums": sorted(list(enums))
                    }


if __name__ == "__main__":
    print("ERROR: This script is a plugin and cannot be run directly.",
          file=sys.stderr)
    print("Please run the central hub runner instead:", file=sys.stderr)
    print(
        f"  python3 agents/projects/code-health/hub/scripts/candidate_finder.py find --plugin {__file__}",
        file=sys.stderr)
    sys.exit(1)
