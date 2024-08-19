# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for managing Fuchsia repos via the ffx tool."""

import argparse

from typing import Iterable

from common import run_ffx_command


def publish_packages(packages: Iterable[str],
                     repo: str,
                     new_repo: bool = False) -> None:
    """Publish packages to a repo directory, initializing it if necessary."""
    if new_repo:
        run_ffx_command(cmd=['repository', 'create', repo])

    args = ['repository', 'publish']
    for package in packages:
        args += ['--package-archive', package]
    args += [repo]
    run_ffx_command(cmd=args)


def register_package_args(parser: argparse.ArgumentParser,
                          allow_temp_repo: bool = False) -> None:
    """Register common arguments for package publishing."""
    package_args = parser.add_argument_group(
        'package', 'Arguments for package publishing.')
    package_args.add_argument('--packages',
                              action='append',
                              help='Paths of the package archives to install')
    package_args.add_argument('--repo',
                              help='Directory packages will be published to.')
    package_args.add_argument('--purge-repo',
                              action='store_true',
                              help='If clear the content in the repo.')
    if allow_temp_repo:
        package_args.add_argument(
            '--no-repo-init',
            action='store_true',
            default=False,
            help='Do not initialize the package repository.')
