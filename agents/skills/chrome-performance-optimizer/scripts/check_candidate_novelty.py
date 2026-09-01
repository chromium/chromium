#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies novelty of candidate performance optimization branches.

Guards against duplicating previously accepted or rejected optimizations.
Queries Gerrit as the global, authoritative source of truth across all
machines and environments:
- topic:chrome-perf-opt-accepted (winning optimizations)
- topic:chrome-perf-opt-rejected (failed/neutral attempts)

Exits with:
- 0 if candidate is novel.
- 1 if duplicate / overlapping change is detected.
"""

import argparse
import base64
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

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
CACHE_DIR = Path(__file__).resolve().parent / '.patch_cache'


def run_cmd(cmd: List[str], cwd: Optional[Path] = None) -> Tuple[int, str, str]:
    proc = subprocess.run(
        cmd,
        cwd=cwd or _SRC_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


def normalize_code_line(line: str) -> Optional[str]:
    """Strips comments, whitespace, and trivial syntax tokens."""
    line = line.strip()
    if not line:
        return None
    # Skip comments
    if line.startswith('//') or line.startswith('/*') or line.startswith('*'):
        return None
    # Skip trivial syntax tokens
    if line in ('{', '}', '};', '(', ')', 'public:', 'private:', 'protected:'):
        return None
    # Remove inline comments
    if '//' in line:
        line = line.split('//', 1)[0].strip()
    return line if line else None


def extract_added_and_deleted_lines(
    diff_text: str,
) -> Tuple[Dict[str, Set[str]], Dict[str, Set[str]]]:
    """Parses a unified diff into added and deleted normalized lines per
    file."""
    added_per_file: Dict[str, Set[str]] = {}
    deleted_per_file: Dict[str, Set[str]] = {}
    current_file = None

    for line in diff_text.splitlines():
        if line.startswith('diff --git'):
            parts = line.split()
            if len(parts) >= 4:
                current_file = parts[3]
                if current_file.startswith('b/'):
                    current_file = current_file[2:]
                added_per_file.setdefault(current_file, set())
                deleted_per_file.setdefault(current_file, set())
        elif current_file:
            if line.startswith('+') and not line.startswith('+++'):
                norm = normalize_code_line(line[1:])
                if norm:
                    added_per_file[current_file].add(norm)
            elif line.startswith('-') and not line.startswith('---'):
                norm = normalize_code_line(line[1:])
                if norm:
                    deleted_per_file[current_file].add(norm)

    return added_per_file, deleted_per_file


def get_current_branch_diff(
    base_ref: str = 'origin/main',
) -> Tuple[str, List[str]]:
    """Returns the unified diff and list of touched files against base."""
    # Check committed changes against base
    code, out, _ = run_cmd(['git', 'diff', f'{base_ref}...HEAD'])
    if code != 0 or not out.strip():
        # Fallback to working directory diff against base
        code, out, _ = run_cmd(['git', 'diff', base_ref])
    if code != 0 or not out.strip():
        # Fallback to HEAD~1 against HEAD
        code, out, _ = run_cmd(['git', 'diff', 'HEAD~1...HEAD'])

    _, out_files, _ = run_cmd(
        ['git', 'diff', '--name-only', f'{base_ref}...HEAD']
    )
    files = [f.strip() for f in out_files.splitlines() if f.strip()]
    if not files:
        _, out_files, _ = run_cmd(['git', 'diff', '--name-only', base_ref])
        files = [f.strip() for f in out_files.splitlines() if f.strip()]

    return out, files


def get_commit_change_ids(base_ref: str = 'origin/main') -> Set[str]:
    """Extracts Change-Ids from commits in the branch relative to base."""
    code, out, _ = run_cmd(['git', 'log', f'{base_ref}..HEAD', '--format=%B'])
    if code != 0 or not out:
        return set()
    change_ids = set(re.findall(r'Change-Id:\s*(I[0-9a-fA-F]+)', out))
    return change_ids


def get_gerrit_patch(cl_number: int) -> Optional[str]:
    """Fetches unified patch for a Gerrit change, caching locally."""
    CACHE_DIR.mkdir(exist_ok=True)
    cache_file = CACHE_DIR / f'{cl_number}.patch'
    if cache_file.is_file():
        try:
            return cache_file.read_text(encoding='utf-8', errors='replace')
        except Exception:
            pass

    try:
        conn = gerrit_util.CreateHttpConn(
            GERRIT_HOST, f'/changes/{cl_number}/revisions/current/patch'
        )
        raw = gerrit_util.ReadHttpResponse(conn).read()
        patch_text = base64.b64decode(raw).decode('utf-8', errors='replace')
        cache_file.write_text(patch_text, encoding='utf-8')
        return patch_text
    except Exception:
        return None


def fetch_cls_by_topic(topic: str) -> List[Dict]:
    try:
        changes = gerrit_util.QueryChanges(
            GERRIT_HOST,
            [('topic', topic)],
            limit=100,
            o_params=['CURRENT_REVISION', 'CURRENT_COMMIT', 'CURRENT_FILES'],
        )
        return changes or []
    except Exception as e:
        print(
            f"Warning: Failed to fetch Gerrit topic '{topic}': {e}",
            file=sys.stderr,
        )
        return []


def check_gerrit_novelty(
    cand_added: Dict[str, Set[str]],
    cand_deleted: Dict[str, Set[str]],
    cand_files: List[str],
    cand_change_ids: Set[str],
    current_cl: Optional[int],
    threshold: float,
    verbose: bool = False,
) -> List[Dict]:
    """Checks candidate diff against Gerrit accepted and rejected CLs."""
    violations = []
    topics = [
        ('chrome-perf-opt-accepted', 'ACCEPTED WINNER'),
        ('chrome-perf-opt-rejected', 'REJECTED ATTEMPT'),
    ]

    for topic_name, topic_label in topics:
        cls = fetch_cls_by_topic(topic_name)
        for c in cls:
            cl_num = c.get('_number')
            if verbose:
                print(
                    f"Checking CL {cl_num} ({topic_label})...",
                    file=sys.stderr,
                )
            if cl_num == current_cl:
                continue

            # 1. Check if Change-Id matches
            gerrit_change_id = c.get('change_id')
            if gerrit_change_id and gerrit_change_id in cand_change_ids:
                violations.append(
                    {
                        'cl': cl_num,
                        'topic': topic_label,
                        'subject': c.get('subject'),
                        'file': 'All (Exact Change-Id match)',
                        'overlap_ratio': 1.0,
                        'matching_lines': 0,
                        'total_lines': 0,
                        'sample_matches': [
                            "Matches existing Gerrit Change-Id: "
                            f"{gerrit_change_id}"
                        ],
                    }
                )
                continue

            # 2. Check file overlap
            rev_id = c.get('current_revision')
            if not rev_id:
                continue
            rev_files = (
                c.get('revisions', {}).get(rev_id, {}).get('files', {}).keys()
            )
            common_files = set(cand_files).intersection(set(rev_files))
            if not common_files:
                continue

            # 3. Fetch patch and inspect code overlap
            patch = get_gerrit_patch(cl_num)
            if not patch:
                continue

            prev_added, prev_deleted = extract_added_and_deleted_lines(patch)

            for f in common_files:
                cand_lines = cand_added.get(f, set())
                prev_lines = prev_added.get(f, set())

                if cand_lines and prev_lines:
                    common = cand_lines.intersection(prev_lines)
                    overlap_ratio = len(common) / len(cand_lines)
                    if overlap_ratio >= threshold or (
                        len(cand_lines) >= 3 and len(common) >= 3
                    ):
                        violations.append(
                            {
                                'cl': cl_num,
                                'topic': topic_label,
                                'subject': c.get('subject'),
                                'file': f,
                                'overlap_ratio': overlap_ratio,
                                'matching_lines': len(common),
                                'total_lines': len(cand_lines),
                                'sample_matches': list(common)[:3],
                            }
                        )
                elif not cand_lines:
                    cand_del = cand_deleted.get(f, set())
                    prev_del = prev_deleted.get(f, set())
                    if cand_del and prev_del:
                        common_del = cand_del.intersection(prev_del)
                        if len(common_del) >= 3:
                            del_ratio = len(common_del) / len(cand_del)
                            if del_ratio >= threshold:
                                violations.append(
                                    {
                                        'cl': cl_num,
                                        'topic': topic_label,
                                        'subject': c.get('subject'),
                                        'file': f,
                                        'overlap_ratio': del_ratio,
                                        'matching_lines': len(common_del),
                                        'total_lines': len(cand_del),
                                        'sample_matches': list(common_del)[:3],
                                    }
                                )

    return violations


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Verify candidate novelty against authoritative Gerrit performance"
            " CLs."
        )
    )
    parser.add_argument(
        '--base',
        default='origin/main',
        help="Base ref to diff against (default: origin/main)",
    )
    parser.add_argument(
        '--threshold',
        type=float,
        default=0.25,
        help=(
            "Similarity overlap ratio threshold to trigger failure"
            " (default: 0.25)"
        ),
    )
    parser.add_argument(
        '--verbose', '-v', action='store_true', help="Verbose diagnostics"
    )
    args = parser.parse_args()

    print("=" * 80)
    print("CHROME PERFORMANCE OPTIMIZER — AUTHORITATIVE NOVELTY VERIFIER")
    print("=" * 80)

    # 1. Obtain candidate diff
    cand_diff, cand_files = get_current_branch_diff(args.base)
    if not cand_files or not cand_diff:
        print(
            f"Error: No changes found relative to {args.base}.", file=sys.stderr
        )
        sys.exit(1)

    print(f"Base ref: {args.base}")
    print(f"Candidate modified files ({len(cand_files)}):")
    for f in cand_files:
        print(f"  • {f}")

    cand_added, cand_deleted = extract_added_and_deleted_lines(cand_diff)
    total_added = sum(len(lines) for lines in cand_added.values())
    total_deleted = sum(len(lines) for lines in cand_deleted.values())
    print(
        f"Normalized candidate footprint: +{total_added} lines, "
        f"-{total_deleted} lines\n"
    )

    # Determine current CL if already uploaded
    current_cl = None
    code, issue_out, _ = run_cmd(['git', 'cl', 'issue'])
    if code == 0 and 'Issue number:' in issue_out:
        m = re.search(r'Issue number:\s*([0-9]+)', issue_out)
        if m:
            current_cl = int(m.group(1))

    cand_change_ids = get_commit_change_ids(args.base)

    # 2. Check authoritative Gerrit topics
    print(
        "Querying Gerrit for topic:chrome-perf-opt-accepted and"
        " topic:chrome-perf-opt-rejected..."
    )
    violations = check_gerrit_novelty(
        cand_added,
        cand_deleted,
        cand_files,
        cand_change_ids,
        current_cl,
        args.threshold,
        args.verbose,
    )

    if not violations:
        print("\n" + "=" * 80)
        print("✅ NOVELTY CHECK PASSED: Candidate is novel!")
        print(
            "   No overlapping changes found in authoritative Gerrit records."
        )
        print("=" * 80 + "\n")
        sys.exit(0)

    print("\n" + "=" * 80)
    print(
        "❌ NOVELTY CHECK FAILED: DUPLICATE / OVERLAPPING OPTIMIZATION DETECTED!"
    )
    print("=" * 80)
    print(
        "Under the Chrome Performance Optimizer rules, repeating,"
        " re-proposing,\n"
        "or re-uploading previously accepted or rejected changes is"
        " STRICTLY PROHIBITED.\n"
    )

    for v in violations:
        cl = v['cl']
        print(f"• Duplicates Gerrit CL {cl} [{v['topic']}]:")
        print(f"  Subject: {v['subject']}")
        print(f"  URL:     https://crrev.com/c/{cl}")
        print(f"  File:    {v['file']}")
        if v['total_lines'] > 0:
            print(
                f"  Overlap: {v['overlap_ratio']:.1%} "
                f"({v['matching_lines']}/{v['total_lines']} normalized lines "
                "match)"
            )
        if v['sample_matches']:
            print("  Sample matching lines:")
            for s in v['sample_matches']:
                print(f"    - {s}")
        print()

    print("=" * 80)
    print(
        "Action Required: Abandon this candidate and pursue a novel"
        " architectural hypothesis."
    )
    print("=" * 80 + "\n")
    sys.exit(1)


if __name__ == '__main__':
    main()
