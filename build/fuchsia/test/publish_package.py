#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for managing Fuchsia repos via the pm tool."""

import argparse
import os
import subprocess
import sys

from typing import Iterable

from common import SDK_TOOLS_DIR, read_package_paths, register_common_args

_pm_tool = os.path.join(SDK_TOOLS_DIR, 'pm')


def publish_packages(packages: Iterable[str],
                     repo: str,
                     new_repo: bool = False) -> None:
    """Publish packages to a repo directory, initializing it if necessary."""
    if new_repo:
        subprocess.run([_pm_tool, 'newrepo', '-repo', repo], check=True)
    for package in packages:
        subprocess.run([_pm_tool, 'publish', '-a', '-r', repo, '-f', package],
                       check=True)


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
    if allow_temp_repo:
        package_args.add_argument(
            '--no-repo-init',
            action='store_true',
            default=False,
            help='Do not initialize the package repository.')


def main():
    """Stand-alone function for publishing packages."""
    parser = argparse.ArgumentParser()
    register_package_args(parser)
    register_common_args(parser)
    args = parser.parse_args()
    if not args.repo:
        raise ValueError('Must specify directory to publish packages.')
    if not args.packages:
        raise ValueError('Must specify packages to publish.')
    if args.out_dir:
        package_paths = []
        for package in args.packages:
            package_paths.extend(read_package_paths(args.out_dir, package))
    else:
        package_paths = args.packages
    publish_packages(package_paths, args.repo)


if __name__ == '__main__':
    sys.exit(main())
