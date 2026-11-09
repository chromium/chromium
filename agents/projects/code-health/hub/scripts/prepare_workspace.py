# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prepares the workspace (Code Health lifecycle, Step 1).

Optionally stashes local changes, switches to main, checks that main has no
unmerged local commits, pulls with rebase and runs `gclient sync -D`.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import hub_utils

STASH_MESSAGE = 'code-health-workspace-prep'


class StepError(Exception):
    """Raised when a step fails; carries the exit code and message."""

    def __init__(self, message, returncode=1):
        super().__init__(message)
        self.message = message
        self.returncode = returncode


def _run_checked(cmd, cwd):
    """Runs |cmd| and returns its output; raises StepError on failure."""
    rc, output = hub_utils.run_cmd(cmd, cwd)
    if rc != 0:
        raise StepError(
            f'Command failed: {" ".join(cmd)}\n{output}'.rstrip(), rc
        )
    return output


def prepare_workspace(repo_root, stash=False):
    """Puts |repo_root| on an up-to-date main; returns True if it stashed."""
    if not os.path.isdir(repo_root):
        raise StepError(f'Repository root not found: {repo_root}')

    stashed = False
    dirty = _run_checked(
        ['git', 'status', '--porcelain', '-uno', '--ignore-submodules'],
        repo_root,
    ).strip()
    if dirty:
        if not stash:
            raise StepError(
                'Uncommitted changes detected. Pass --stash to '
                'auto-stash or commit/stash manually.'
            )
        _run_checked(['git', 'stash', 'push', '-m', STASH_MESSAGE], repo_root)
        stashed = True

    _run_checked(['git', 'checkout', 'main'], repo_root)

    # Only check main. A previous feature branch is expected to have unmerged
    # commits, but local commits on main would be rewritten by the pull below.
    unmerged = _run_checked(
        ['git', 'log', 'origin/main..HEAD', '--oneline'], repo_root
    ).strip()
    if unmerged:
        raise StepError(
            f'Unmerged local commits detected on branch main\n{unmerged}'
        )

    _run_checked(['git', 'pull', 'origin', 'main', '--rebase'], repo_root)
    _run_checked(['gclient', 'sync', '-D'], repo_root)
    return stashed


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--stash',
        action='store_true',
        help='Auto-stash uncommitted changes if dirty.',
    )
    args = parser.parse_args(argv)
    try:
        stashed = prepare_workspace(
            hub_utils.find_repo_root(), stash=args.stash
        )
    except StepError as e:
        print(e.message, file=sys.stderr)
        return e.returncode
    print(
        'Workspace ready on main'
        + (' (stashed local changes).' if stashed else '.')
    )
    return 0


if __name__ == '__main__':
    sys.exit(main())
