#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for serving a TUF repository."""

import argparse
import contextlib
import sys

from typing import Iterator, Optional

from common import REPO_ALIAS, register_device_args, run_ffx_command

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


def run_serve_cmd(cmd: str, args: argparse.Namespace) -> None:
    """Helper for running serve commands."""

    if cmd == 'start':
        _start_serving(args.repo, args.repo_name, args.target_id)
    else:
        _stop_serving(args.repo_name, args.target_id)


@contextlib.contextmanager
def serve_repository(args: argparse.Namespace) -> Iterator[None]:
    """Context manager for serving a repository."""
    run_serve_cmd('start', args)
    try:
        yield None
    finally:
        run_serve_cmd('stop', args)


def main():
    """Stand-alone function for serving a repository."""

    parser = argparse.ArgumentParser()
    parser.add_argument('cmd',
                        choices=['start', 'stop'],
                        help='Choose to start|stop repository serving.')
    register_device_args(parser)
    register_serve_args(parser)
    args = parser.parse_args()
    if args.cmd == 'start' and not args.repo:
        raise ValueError('Directory the repository is serving from needs '
                         'to be specified.')
    run_serve_cmd(args.cmd, args)


if __name__ == '__main__':
    sys.exit(main())
