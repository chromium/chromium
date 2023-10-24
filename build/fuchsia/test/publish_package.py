#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for managing Fuchsia repos via the ffx tool."""

import argparse
import sys

from typing import Iterable

from common import make_clean_directory, read_package_paths, \
                   register_common_args, run_ffx_command


def publish_packages(packages: Iterable[str],
                     repo: str,
                     new_repo: bool = False) -> None:
    """Publish packages to a repo directory, initializing it if necessary."""
    if new_repo:
        run_ffx_command(cmd=['repository', 'create', repo], check=True)

    args = ['repository', 'publish']
    for package in packages:
        args += ['--package-archive', package]
    args += [repo]
    run_ffx_command(cmd=args, check=True)


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
    if args.purge_repo:
        make_clean_directory(args.repo)
    publish_packages(package_paths, args.repo, args.purge_repo)


if __name__ == '__main__':
    sys.exit(main())
