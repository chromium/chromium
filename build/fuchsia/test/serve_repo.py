# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for serving a TUF repository."""

import argparse
import contextlib
import json
import logging
from typing import Iterator, Optional

from common import run_ffx_command, REPO_ALIAS

_REPO_NAME = 'chromium-test-package-server'


def _stop_serving(repo_name: str, target: Optional[str]) -> None:
    """Stop serving a repository."""

    # Attempt to clean up.
    run_ffx_command(
        cmd=[
            'target', 'repository', 'deregister', '-r', repo_name
        ],
        target_id=target, check=False)

    run_ffx_command(
        cmd=[
            'repository', 'server', 'stop', repo_name
        ], check=False)


def _start_serving(repo_dir: str, repo_name: str,
                   target: Optional[str]) -> None:
    """Start serving a repository to a target device.

    Args:
        repo_dir: directory the repository is served from.
        repo_name: repository name.
        target: Fuchsia device the repository is served to.
    """

    cmd = [
        'repository', 'server', 'start', '--background',
        '--address', '[::]:0',
        '--repository', repo_name, '--repo-path', repo_dir, '--no-device'
    ]

    start_cmd = run_ffx_command(cmd=cmd, check=False)

    logging.warning('ffx repository server start returns %d: %s %s',
                          start_cmd.returncode,
                          start_cmd.stderr, start_cmd.stdout)

    _assert_server_running(repo_name)

    cmd = [
        'target', 'repository', 'register', '-r', repo_name, '--alias',
        REPO_ALIAS
    ]
    run_ffx_command(cmd=cmd,target_id=target)


def _assert_server_running(repo_name: str) -> None:
    """Raises RuntimeError if the repository server is not running."""

    list_cmd = run_ffx_command(
        cmd= ['--machine','json','repository', 'server',
             'list', '--name', repo_name],
        check=False,
        capture_output=True)
    try:
        response = json.loads(list_cmd.stdout.strip())
        if 'ok' in response and response['ok']['data']:
            if response['ok']['data'][0]['name'] != repo_name:
                raise RuntimeError(
                    'Repository server %s is not running. Output: %s stderr: %s'
                    % (repo_name, list_cmd.stdout, list_cmd.stderr))
            return
    except json.decoder.JSONDecodeError as error:
        # Log the json parsing error, but don't raise an exception since it
        # does not have the full context of the error.
        logging.error('Unexpected json string: %s, exception: %s, stderr: %s',
                list_cmd.stdout, error, list_cmd.stderr)
    raise RuntimeError(
        'Repository server %s is not running. Output: %s stderr: %s'
        % (repo_name, list_cmd.stdout, list_cmd.stderr))

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
