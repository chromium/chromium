# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find Java inline FQN violations."""
# pylint: disable=line-too-long

import re
import subprocess
import functools

# Configuration for main runner
MODE = "grouped"
FILE_EXTENSIONS = [".java"]

FQN_REGEX = re.compile(
    r"\b(org\.chromium|android|com\.google|androidx|java|javax)"
    r"(?:\.[a-zA-Z0-9_]+)+\b")
URL_REGEX = re.compile(r"https?://\S+")


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
                cleaned_line = URL_REGEX.sub("", line)
                for match in FQN_REGEX.finditer(cleaned_line):
                    fqn = match.group(0)
                    if ".R." not in fqn and len(fqn.split(".")) >= 3:
                        fqns.add(fqn)
    except Exception:
        pass
    return fqns


@functools.lru_cache(maxsize=None)
def is_valid_import(fqn, search_root):
    parts = fqn.split(".")
    for i in range(len(parts), 1, -1):
        test_fqn = ".".join(parts[:i])
        regex_fqn = test_fqn.replace(".", r"\.")
        try:
            res = subprocess.run([
                "git", "grep", "-q", "-E", f"import (static )?{regex_fqn}[;.]",
                "--", ":/*.java"
            ],
                                 cwd=search_root,
                                 capture_output=True,
                                 check=False)
            if res.returncode == 0:
                return True
        except Exception:
            return True
    return False


def check_file(file_path, search_root):
    """Checks a single file for valid inline FQN violations."""
    fqns = get_file_fqns(file_path)
    if not fqns:
        return None

    valid_fqns = set()
    banned_fqns = set()
    for fqn in fqns:
        if is_valid_import(fqn, search_root):
            valid_fqns.add(fqn)
        else:
            banned_fqns.add(fqn)

    if valid_fqns:
        return {"valid": list(valid_fqns), "banned": list(banned_fqns)}
    return None


if __name__ == "__main__":
    import sys
    print("ERROR: This script is a plugin and cannot be run directly.",
          file=sys.stderr)
    print("Please run the central hub runner instead:", file=sys.stderr)
    print(
        f"  python3 agents/projects/code-health/hub/scripts/candidate_finder.py find --plugin {__file__}",
        file=sys.stderr)
    sys.exit(1)
