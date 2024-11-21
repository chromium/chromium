# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for serving a TUF repository."""

import argparse
import contextlib

from typing import Iterator, Optional

from common import REPO_ALIAS, run_ffx_command

_REPO_NAME = 'chromium-test-package-server'


def _stop_serving(repo_name: str, target: Optional[str]) -> None:
    """Stop serving a repository."""

    # Attempt to clean up.
    run_ffx_command(
        cmd=['target', 'repository', 'deregister', '-r', repo_name],
        target_id=target,
        check=False)
    run_ffx_command(cmd=['repository', 'remove', repo_name], check=False)
    run_ffx_command(cmd=['repository', 'server', 'stop'], check=False)


def _start_serving(repo_dir: str, repo_name: str,
                   target: Optional[str]) -> None:
    """Start serving a repository to a target device.

    Args:
        repo_dir: directory the repository is served from.
        repo_name: repository name.
        target: Fuchsia device the repository is served to.
    """

    run_ffx_command(cmd=('config', 'set', 'repository.server.mode', '\"ffx\"'))

    run_ffx_command(cmd=['repository', 'server', 'start'])
    run_ffx_command(
        cmd=['repository', 'add-from-pm', repo_dir, '-r', repo_name])
    run_ffx_command(cmd=[
        'target', 'repository', 'register', '-r', repo_name, '--alias',
        REPO_ALIAS
    ],
                    target_id=target)


def register_serve_args(arg_parser: argparse.ArgumentParser) -> None:
    """Register common arguments for repository serving."""

    serve_args = arg_parser.add_argument_group('serve',
                                               'repo serving arguments')
    serve_args.add_argument('--serve-repo',
                            dest='repo',
                            help='Directory the repository is served from.')
    serve_args.add_argument('--repo-name',
                            default=_REPO_NAME,
                            help='Name of the repository.')


@contextlib.contextmanager
def serve_repository(args: argparse.Namespace) -> Iterator[None]:
    """Context manager for serving a repository."""
    _start_serving(args.repo, args.repo_name, args.target_id)
    try:
        yield None
    finally:
        _stop_serving(args.repo_name, args.target_id)
