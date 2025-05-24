# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for managing Fuchsia repos via the ffx tool."""

import argparse
import logging
import os.path

from typing import Iterable

from common import run_ffx_command


def ensure_repository(args: argparse.Namespace) -> bool:
    """Initializes the repository if necessary, returns true if the repository
    is being created."""
    assert not os.path.exists(args.repo) or os.path.isdir(args.repo), \
        'Need a directory for repo: ' + args.repo
    if args.no_repo_init and os.path.exists(args.repo):
        logging.warning(
            'You are using --no-repo-init, but the repo %s exists. Ensure it\'s'
            ' created via ffx repository create, or the following repository '
            'publish may fail.', args.repo)
        return False
    run_ffx_command(cmd=['repository', 'create', args.repo])
    return True


def publish_packages(packages: Iterable[str],
                     args: argparse.Namespace) -> None:
    """Publishes packages to a repo directory."""
    cmd = ['repository', 'publish']
    for package in packages:
        cmd += ['--package-archive', package]
    cmd += [args.repo]
    run_ffx_command(cmd=cmd)


def register_package_args(parser: argparse.ArgumentParser) -> None:
    """Registers common arguments for package publishing."""
    package_args = parser.add_argument_group(
        'package', 'Arguments for package publishing.')
    package_args.add_argument('--packages',
                              action='append',
                              help='Paths of the package archives to install')
    package_args.add_argument('--repo',
                              help='Directory packages will be published to.')
    package_args.add_argument('--no-repo-init',
                              action='store_true',
                              default=False,
                              help='Do not initialize the package repository.')
