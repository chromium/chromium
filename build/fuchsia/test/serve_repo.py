#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for serving a TUF repository."""

import argparse
import json
import logging
import os
import sys

from common import REPO_ALIAS, run_ffx_command, run_ffx_target_command

# Contains information about the active ephemeral repository.
_REPO_CONFIG_FILE = os.path.join('/', 'tmp', 'fuchsia-repo-config')


def _configure_ffx_serving():
    """Configure ffx to allow serving a ffx-managed repository.

        Returns:
          True if configuration was updated, otherwise False.
        """
    config_updated = False
    repo_cmd = run_ffx_command(['config', 'get', 'ffx_repository'],
                               capture_output=True,
                               encoding='utf-8')
    if 'true' not in repo_cmd.stdout:
        run_ffx_command(['config', 'set', 'ffx_repository', 'true'])
        config_updated = True

    server_cmd = run_ffx_command(['config', 'get', 'repository.server.mode'],
                                 capture_output=True,
                                 encoding='utf-8')
    if 'ffx' not in server_cmd.stdout:
        run_ffx_command(['config', 'set', 'repository.server.mode', 'ffx'])
        config_updated = True
    return config_updated


def _stop_serving() -> None:
    """Stop serving a repository configured in _REPO_CONFIG_FILE."""
    if not os.path.exists(_REPO_CONFIG_FILE):
        logging.warning('Could not find repository configuration.')
        return

    with open(_REPO_CONFIG_FILE, 'r') as file:
        data = json.load(file)

    run_ffx_target_command(
        ['repository', 'deregister', '-r', data['repo_name']], data['target'])
    run_ffx_command(['repository', 'remove', data['repo_name']])
    run_ffx_command(['repository', 'server', 'stop'])
    os.remove(_REPO_CONFIG_FILE)


def _start_serving(repo_dir: str, repo_name: str, target: str) -> None:
    """Start serving a repository.
        Args:
          repo_dir: directory the repository is served from.
          repo_name: repository name.
          target: Fuchsia device the repository is served to.
        """

    if os.path.exists(_REPO_CONFIG_FILE):
        _stop_serving()

    # Check ffx configs, restart daemon if necessary.
    if _configure_ffx_serving():
        run_ffx_command(['doctor', '--restart-daemon'])

    data = {}
    data['repo_name'] = repo_name
    data['target'] = target
    with open(_REPO_CONFIG_FILE, 'w') as file:
        json.dump(data, file)
    run_ffx_command(['repository', 'server', 'start'])
    run_ffx_command(['repository', 'add-from-pm', repo_dir, '-r', repo_name])
    run_ffx_target_command(
        ['repository', 'register', '-r', repo_name, '--alias', REPO_ALIAS],
        target)


def register_serve_args(arg_parser: argparse.ArgumentParser) -> None:
    """Register common arguments for repository serving."""
    serve_args = arg_parser.add_argument_group('serve',
                                               'repo serving arguments')
    serve_args.add_argument('--serve-repo',
                            dest='repo',
                            help='Directory the repository is served from.')
    serve_args.add_argument('--repo-name',
                            default='test',
                            help='Name of the repository.')
    serve_args.add_argument('--serve-target',
                            dest='target',
                            help='Target the repository registers with.')


def run_serve_cmd(cmd: str, args: argparse.Namespace) -> None:
    """Helper for running serve commands."""
    if cmd == 'start':
        _start_serving(args.repo, args.repo_name, args.target)
    else:
        _stop_serving()


def main():
    """Stand-alone function for serving a repository."""
    parser = argparse.ArgumentParser()
    parser.add_argument('cmd',
                        choices=['start', 'stop'],
                        help='Choose to start|stop repository serving.')
    register_serve_args(parser)
    args = parser.parse_args()
    run_serve_cmd(args.cmd, args)


if __name__ == '__main__':
    sys.exit(main())
