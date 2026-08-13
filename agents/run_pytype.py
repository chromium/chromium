#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on agent Python code."""

import pathlib
import sys

AGENTS_DIR = pathlib.Path(__file__).parent
CHROMIUM_SRC_DIR = AGENTS_DIR.parent

sys.path.append(str(CHROMIUM_SRC_DIR))

from testing.pytype_common import pytype_runner

from agents import presubmit_support


def main() -> int:
    extra_paths = [
        str(p) for p in presubmit_support.get_agents_python_path_entries()
    ]

    files_to_exclude = [
        # Exclude the shared repos since they are not part of Chromium.
        str(AGENTS_DIR / 'internal'),
        str(AGENTS_DIR / 'shared'),
        # Exclude the CIPD dependencies that might be present from running
        # prompt evals.
        str(AGENTS_DIR / 'testing' / 'cipd'),
        # WIP, paused, and currently unclear whether work will continue.
        str(AGENTS_DIR / 'infra' / 'review_rag_indexer'),
    ]

    return pytype_runner.run_pytype(
        test_name='agents_pytype',
        test_location='//agents/run_pytype.py',
        files_to_check=[str(AGENTS_DIR)],
        files_to_exclude=files_to_exclude,
        python_paths=extra_paths,
        cwd=str(AGENTS_DIR),
    )


if __name__ == '__main__':
    sys.exit(main())
