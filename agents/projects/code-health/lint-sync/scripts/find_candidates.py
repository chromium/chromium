#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds enums in XML and source files that are missing LINT sync guards."""

import json
import re
import subprocess


def get_xml_candidates():
    """Fast search for all enum definitions in XML files."""
    cmd = ["rg", "--json", "<enum name=", "tools/metrics/histograms/"]
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
    """Check if the XML enum is missing a LINT.IfChange guard."""
    try:
        start = max(1, line_num - 3)
        cmd = ["sed", "-n", f"{start},{line_num-1}p", path]
        result = subprocess.run(cmd,
                                capture_output=True,
                                text=True,
                                check=False)
        return "LINT.IfChange" not in result.stdout
    except subprocess.SubprocessError:
        return False


def find_source_file(enum_name):
    """Search for common enum patterns in C++ or Java."""
    pattern = f"enum (class )?{enum_name}"
    cmd = ["rg", "-n", "-m", "1", pattern, "chrome", "components"]
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)

    if result.returncode == 0 and result.stdout:
        first_match = result.stdout.strip().split('\n')[0]
        parts = first_match.split(':')
        if len(parts) >= 2:
            path, line_num = parts[0], int(parts[1])
            # Check for guard in source
            start = max(1, line_num - 3)
            res = subprocess.run(["sed", "-n", f"{start},{line_num-1}p", path],
                                 capture_output=True,
                                 text=True,
                                 check=False)
            if "LINT.IfChange" not in res.stdout:
                return path, line_num
    return None, None


def main():
    """Main execution for finding unguarded enums."""
    candidates = get_xml_candidates()

    # Dictionary to keep track of (source_path, xml_path) pairs and their enums
    pairs = {}

    for enum_name, xml_path, xml_line in candidates:
        if is_xml_unguarded(xml_path, xml_line):
            source_path, _ = find_source_file(enum_name)
            if source_path:
                key = (source_path, xml_path)
                if key not in pairs:
                    pairs[key] = set()
                pairs[key].add(enum_name)

    # Now find ALL other enums shared between these two files for each pair
    for (source_path, xml_path), enums in pairs.items():
        try:
            with open(source_path, 'r', encoding='utf-8') as f:
                source_content = f.read()

            for other_name, other_xml_path, other_xml_line in candidates:
                if other_xml_path == xml_path and other_name not in enums:
                    is_in_source = (f"enum class {other_name}"
                                    in source_content
                                    or f"enum {other_name}" in source_content)
                    if is_in_source:
                        if is_xml_unguarded(other_xml_path, other_xml_line):
                            enums.add(other_name)

            if len(enums) >= 4:
                print(f"SOURCE: //{source_path}")
                print(f"XML: //{xml_path}")
                print("ENUMS:")
                for e in sorted(enums):
                    print(f"  - {e}")
                return
        except IOError:
            continue

    print("No candidates found.")


if __name__ == "__main__":
    main()
