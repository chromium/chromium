#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches and displays all previously attempted performance optimization CLs.

Queries Gerrit for CLs under:
- topic:chrome-perf-opt-rejected (failed/neutral attempts - DO NOT REPEAT)
- topic:chrome-perf-opt-accepted (significant winners - BUILD UPON)
"""

import argparse
import json
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
    parser = argparse.ArgumentParser(
        description="Fetch and search tried optimization CLs from Gerrit."
    )
    parser.add_argument(
        '--accepted-only', action='store_true', help="Only fetch accepted CLs"
    )
    parser.add_argument(
        '--rejected-only', action='store_true', help="Only fetch rejected CLs"
    )
    parser.add_argument(
        '--search',
        type=str,
        default="",
        help="Search query to filter CL subjects, files, or commit messages",
    )
    parser.add_argument(
        '--json', action='store_true', help="Output as structured JSON"
    )
    args = parser.parse_args()

    accepted = (
        []
        if args.rejected_only
        else fetch_cls_by_topic('chrome-perf-opt-accepted')
    )
    rejected = (
        []
        if args.accepted_only
        else fetch_cls_by_topic('chrome-perf-opt-rejected')
    )

    def matches_search(change, query):
        if not query:
            return True
        query_lower = query.lower()
        subject = change.get('subject', '').lower()
        if query_lower in subject:
            return True
        # Check files in current revision if available
        rev_id = change.get('current_revision')
        if rev_id:
            files = (
                change.get('revisions', {})
                .get(rev_id, {})
                .get('files', {})
                .keys()
            )
            for f in files:
                if query_lower in f.lower():
                    return True
        return False

    if args.search:
        accepted = [c for c in accepted if matches_search(c, args.search)]
        rejected = [c for c in rejected if matches_search(c, args.search)]

    if args.json:
        result = {
            'accepted': [
                {
                    'number': c.get('_number'),
                    'subject': c.get('subject'),
                    'status': c.get('status'),
                    'updated': c.get('updated'),
                    'url': f"https://crrev.com/c/{c.get('_number')}",
                }
                for c in accepted
            ],
            'rejected': [
                {
                    'number': c.get('_number'),
                    'subject': c.get('subject'),
                    'status': c.get('status'),
                    'updated': c.get('updated'),
                    'url': f"https://crrev.com/c/{c.get('_number')}",
                }
                for c in rejected
            ],
        }
        print(json.dumps(result, indent=2))
        return

    print("=" * 80)
    query_str = f" [Search: '{args.search}']" if args.search else ""
    print(
        f"CHROME PERFORMANCE OPTIMIZER - TRIED CLs{query_str} (Accepted:"
        f" {len(accepted)}, Rejected: {len(rejected)})"
    )
    print("=" * 80)

    if not args.rejected_only:
        print(f"\n🏆 ACCEPTED WINNING OPTIMIZATIONS ({len(accepted)} CLs):")
        print("-" * 80)
        if not accepted:
            print("  (None found)")
        for c in accepted:
            num = c.get('_number')
            subj = c.get('subject', 'No Subject')
            print(f"  • CL {num}: {subj} (https://crrev.com/c/{num})")

    if not args.accepted_only:
        print(f"\n❌ REJECTED ATTEMPTS ({len(rejected)} CLs) - DO NOT REPEAT:")
        print("-" * 80)
        if not rejected:
            print("  (None found)")
        for c in rejected:
            num = c.get('_number')
            subj = c.get('subject', 'No Subject')
            updated = c.get('updated', '')[:10]
            print(
                f"  • CL {num} ({updated}): {subj} (https://crrev.com/c/{num})"
            )

    print("\n" + "=" * 80 + "\n")


if __name__ == '__main__':
    main()
