# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Formats and lints the current change (Code Health lifecycle, Step 5).

Runs `git cl format`, then histogram `validate_format.py` if the change
touches histogram XML files.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import hub_utils

_HISTOGRAM_XML_PATTERNS = (
    re.compile(
        r'^tools/metrics/histograms/metadata/[^/]+/'
        r'(?:histograms|enums)\.xml$'
    ),
    re.compile(r'^tools/metrics/histograms/(?:enums|variants)\.xml$'),
)


def is_histogram_xml(path):
    normalized = path.replace(os.sep, '/')
    return any(p.match(normalized) for p in _HISTOGRAM_XML_PATTERNS)


def _changed_files(repo_root):
    """Returns files changed (incl. deleted) relative to origin/main."""
    rc, base_out = hub_utils.run_cmd(
        ['git', 'merge-base', 'origin/main', 'HEAD'], repo_root
    )
    base = base_out.strip() if rc == 0 and base_out.strip() else 'HEAD'
    rc, diff_out = hub_utils.run_cmd(
        [
            'git',
            '-c',
            'core.quotePath=false',
            'diff',
            '-z',
            '--name-only',
            base,
        ],
        repo_root,
    )
    return [p for p in diff_out.split('\0') if p] if rc == 0 else []


def _run_step(cmd, repo_root):
    """Runs |cmd|, relaying its output. Returns its return code."""
    rc, out = hub_utils.run_cmd(cmd, repo_root)
    if out.strip():
        print(out.rstrip(), file=sys.stdout if rc == 0 else sys.stderr)
    return rc


def format_and_lint(repo_root):
    """Returns 0, or the return code of the first failing step."""
    rc = _run_step(['git', 'cl', 'format'], repo_root)
    if rc != 0:
        return rc
    if any(is_histogram_xml(f) for f in _changed_files(repo_root)):
        return _run_step(
            ['vpython3', 'tools/metrics/histograms/validate_format.py'],
            repo_root,
        )
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args(argv)
    return format_and_lint(hub_utils.find_repo_root())


if __name__ == '__main__':
    sys.exit(main())
