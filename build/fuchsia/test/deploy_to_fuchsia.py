#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script for deploying Chrome binaries to a Fuchsia checkout."""

import argparse
import os
import sys

from common import read_package_paths, register_common_args
from compatible_utils import install_symbols
from publish_package import publish_packages


def register_fuchsia_args(parser: argparse.ArgumentParser) -> None:
    """Register common arguments for deploying to Fuchsia."""

    fuchsia_args = parser.add_argument_group(
        'fuchsia', 'Arguments for working with Fuchsia checkout.')
    fuchsia_args.add_argument('--fuchsia-out-dir',
                              help='Path to output directory of a local '
                              'Fuchsia checkout.')


def main():
    """Stand-alone program for deploying to the output directory of a local
    Fuchsia checkout."""

    parser = argparse.ArgumentParser()
    parser.add_argument('package', help='The package to deploy to Fuchsia.')
    register_common_args(parser)
    register_fuchsia_args(parser)
    args = parser.parse_args()

    fuchsia_out_dir = os.path.expanduser(args.fuchsia_out_dir)
    package_paths = read_package_paths(args.out_dir, args.package)
    publish_packages(package_paths, os.path.join(fuchsia_out_dir,
                                                 'amber-files'))
    install_symbols(package_paths, fuchsia_out_dir)


if __name__ == '__main__':
    sys.exit(main())
