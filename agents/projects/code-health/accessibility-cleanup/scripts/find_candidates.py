#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds Java UI classes that might contain accessibility anti-patterns."""

import json
import re
import subprocess

TARGET_DIRS = ["chrome/android", "components/browser_ui", "chrome/browser"]


def run_rg_search(query, word_match=True):
    cmd = ["rg", "--json", "-t", "java"]
    if word_match:
        cmd.append("-w")
    cmd.extend([query, *TARGET_DIRS])
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)

    matches = {}
    if result.returncode != 0:
        return matches

    for line in result.stdout.splitlines():
        try:
            data = json.loads(line)
            if data["type"] == "match":
                path = data["data"]["path"]["text"]
                line_num = data["data"]["line_number"]
                line_text = data["data"]["lines"]["text"]
                if not path.endswith(".java"):
                    continue
                if path not in matches:
                    matches[path] = []
                matches[path].append((line_num, line_text))
        except (json.JSONDecodeError, KeyError):
            continue
    return matches


def find_candidates():
    candidates = []

    # 1. Look for setContentDescription + state keywords (Rule 1)
    desc_matches = run_rg_search("setContentDescription")
    keywords_re = re.compile(
        r"\b(expand|collapse|expanded|collapsed|selected|unselected|"
        r"active|inactive|playing|paused|checked|unchecked)\b",
        re.IGNORECASE,
    )
    for path, matches in desc_matches.items():
        try:
            with open(path, "r", encoding="utf-8") as f:
                content = f.read()
            if keywords_re.search(content):
                interesting = []
                for line_num, line_text in matches:
                    is_dynamic = ("+" in line_text
                                  or "String.format" in line_text
                                  or "getString" in line_text)
                    if (keywords_re.search(line_text)
                            or "R.string.accessibility_" in line_text
                            or is_dynamic):
                        interesting.append((
                            line_num,
                            f"[Rule 1: ContentDesc State] {line_text.strip()}",
                        ))
                if interesting:
                    candidates.append((path, interesting))
        except IOError:
            continue

    # 2. Look for announceForAccessibility usages (Rule 2)
    announce_matches = run_rg_search("announceForAccessibility")
    for path, matches in announce_matches.items():
        interesting = []
        for line_num, line_text in matches:
            # Flags announceForAccessibility for manual inspection (should use
            # paneTitle/liveRegion)
            interesting.append((
                line_num,
                f"[Rule 2: announceForAccessibility] {line_text.strip()}",
            ))
        if interesting:
            candidates.append((path, interesting))

    # 3. Look for custom labels on standard actions (Rule 3)
    action_matches = run_rg_search("AccessibilityActionCompat")
    custom_label_re = re.compile(
        r"new\s+AccessibilityActionCompat\(\s*"
        r"(?:AccessibilityNodeInfoCompat\.)?"
        r"(?:AccessibilityActionCompat\.)?ACTION_(?:CLICK|LONG_CLICK)"
        r"\.getId\(\)\s*,\s*(?:R\.string\.|\"|getString|res\.getString)",
        re.DOTALL,
    )
    for path in action_matches.keys():
        try:
            with open(path, "r", encoding="utf-8") as f:
                content = f.read()
            interesting = []
            for match in custom_label_re.finditer(content):
                line_num = content.count('\n', 0, match.start()) + 1
                statement = content[match.start():match.end()]
                interesting.append((
                    line_num,
                    f"[Rule 3: Custom Action Label] {statement.strip()}",
                ))
            if interesting:
                candidates.append((path, interesting))
        except IOError:
            continue

    # 4. Look for assertive live regions (Rule 4)
    assertive_matches = run_rg_search("ACCESSIBILITY_LIVE_REGION_ASSERTIVE")
    for path, matches in assertive_matches.items():
        interesting = []
        for line_num, line_text in matches:
            interesting.append((
                line_num,
                f"[Rule 4: Assertive Live Region] {line_text.strip()}",
            ))
        if interesting:
            candidates.append((path, interesting))

    return candidates


def main():
    candidates = find_candidates()
    if not candidates:
        print("No candidates found.")
        return

    for path, matches in candidates:
        print(f"CANDIDATE: //{path}")
        print("MATCHES:")
        for line_num, text in matches:
            print(f"  Line {line_num}: {text}")
        print("-" * 40)


if __name__ == "__main__":
    main()
