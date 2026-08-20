#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches and displays all previously attempted performance optimization CLs.

Queries Gerrit for CLs under:
- topic:chrome-perf-opt-rejected (failed/neutral attempts - DO NOT REPEAT)
- topic:chrome-perf-opt-accepted (significant winners - BUILD UPON)
"""

import shutil
import sys
from pathlib import Path

# Add depot_tools to path from source tree or system PATH
_SRC_ROOT = Path(__file__).resolve().parents[4]
_DEPOT_TOOLS_IN_SRC = _SRC_ROOT / 'third_party' / 'depot_tools'
if _DEPOT_TOOLS_IN_SRC.is_dir():
    sys.path.insert(0, str(_DEPOT_TOOLS_IN_SRC))

_GIT_CL_PATH = shutil.which('git-cl')
if _GIT_CL_PATH:
    _DEPOT_TOOLS_DIR = Path(_GIT_CL_PATH).resolve().parent
    if str(_DEPOT_TOOLS_DIR) not in sys.path:
        sys.path.insert(0, str(_DEPOT_TOOLS_DIR))

import gerrit_util  # pylint: disable=import-error

GERRIT_HOST = 'chromium-review.googlesource.com'


def fetch_cls_by_topic(topic: str, limit: int = 100):
    try:
        changes = gerrit_util.QueryChanges(
            GERRIT_HOST,
            [('topic', topic)],
            limit=limit,
            o_params=['CURRENT_REVISION', 'CURRENT_COMMIT', 'CURRENT_FILES'],
        )
        return changes or []
    except Exception as e:
        print(f"Error querying Gerrit for '{topic}': {e}", file=sys.stderr)
        return []


def main():
    print("=" * 80)
    print("CHROME PERFORMANCE OPTIMIZER - PREVIOUSLY ATTEMPTED CL TRACKER")
    print("=" * 80)

    # 1. Accepted Winning CLs
    accepted_changes = fetch_cls_by_topic('chrome-perf-opt-accepted')
    print(f"\n🏆 ACCEPTED WINNING OPTIMIZATIONS ({len(accepted_changes)} CLs):")
    print("These changes delivered verified statistically significant wins.")
    print("-" * 80)
    if not accepted_changes:
        print("  (None found)")
    for c in accepted_changes:
        num = c.get('_number')
        subj = c.get('subject', 'No Subject')
        status = c.get('status', '')
        print(f"  • CL {num} [{status}]: {subj}")
        print(f"    Link: https://crrev.com/c/{num}")

    # 2. Rejected / Neutral CLs
    rejected_changes = fetch_cls_by_topic('chrome-perf-opt-rejected')
    print(
        f"\n❌ REJECTED ATTEMPTS ({len(rejected_changes)} CLs) - DO NOT REPEAT:"
    )
    print("These optimizations were tested with 150 Pinpoint iterations and")
    print("showed NO statistically significant improvement ($p > 0.05$).")
    print("-" * 80)
    if not rejected_changes:
        print("  (None found)")
    for c in rejected_changes:
        num = c.get('_number')
        subj = c.get('subject', 'No Subject')
        updated = c.get('updated', '')[:10]
        print(f"  • CL {num} ({updated}): {subj}")
        print(f"    Link: https://crrev.com/c/{num}")

    print("\n" + "=" * 80)
    print("SUMMARY OF EXCLUDED PATTERNS:")
    print("  1. Do not repeat micro-optimizations that yielded < 0.1% change.")
    print(
        "  2. Focus on macro-architectural levers in optimization_patterns.md."
    )
    print("=" * 80 + "\n")


if __name__ == '__main__':
    main()
