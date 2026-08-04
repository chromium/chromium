# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plugin to find Java anonymous class candidates to replace with lambdas."""
# pylint: disable=line-too-long

import os
import re
import sys

# Add general hub utilities to path
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "../../hub/scripts"))
from hub_utils import (
    strip_comments,
    strip_strings,
)

# Configuration for main runner
MODE = "grouped"
FILE_EXTENSIONS = ['.java']
SKIP_DIRS = {"javatests", "junit", "third_party", "out", "build", ".git"}

# Pattern: Prefer Lambdas over Anonymous Classes
# Anonymous classes for single-method (SAM) interfaces add significant ClassDef and metadata
# overhead in DEX. Converting them to lambdas allows R8 to group/merge them to save binary size.
ANON_SAM_RE = re.compile(
    r'\bnew\s+(?:[a-zA-Z0-9_.]+\.)?(Runnable|Callback|OnClickListener|OnTouchListener|OnHoverListener|OnGenericMotionListener|OnContextClickListener|BooleanSupplier|Supplier|Callable|OnLayoutChangeListener|NightModeStateProvider\.Observer|PhotoPickerDelegate|ChildCrashedCallback)\b[^(\n]*\([^)]*\)\s*\{'
)


def check_file(file_path, search_root):
    """Checks a single file and returns metadata if it has anonymous class candidates."""
    if any(
            file_path.endswith(suffix)
            for suffix in ['Test.java', 'UnitTest.java', 'RenderTest.java']):
        return None

    if os.path.isabs(file_path):
        full_path = file_path
    elif os.path.exists(file_path):
        full_path = file_path
    else:
        full_path = os.path.join(search_root, file_path)

    try:
        with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
            raw_content = f.read()

            # Prepare sanitized contents
            syntax_content = strip_strings(strip_comments(raw_content))

            if ANON_SAM_RE.search(syntax_content):
                return {
                    "issues":
                    "replace-anonymous-runnables-candidates: anonymous-classes"
                }
    except Exception:
        pass
    return None


if __name__ == "__main__":
    print("ERROR: This script is a plugin and cannot be run directly.",
          file=sys.stderr)
    print("Please run the central hub runner instead:", file=sys.stderr)
    print(
        f"  python3 agents/projects/code-health/hub/scripts/candidate_finder.py find --plugin {__file__}",
        file=sys.stderr)
    sys.exit(1)
