#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Writes a dummy R.txt file from a resource zip."""

import argparse
import sys

from util import build_utils
from util import resources_parser


def main(args):
    parser = argparse.ArgumentParser(
        description='Create an R.txt from resources.'
    )
    parser.add_argument(
        '--resources-zip-path',
        required=True,
        help='Path to input resources zip.',
    )
    parser.add_argument(
        '--rtxt-path', required=True, help='Path to output R.txt file.'
    )
    options = parser.parse_args(build_utils.ExpandFileArgs(args))
    resources_parser.RTxtGenerator(
        res_zips=[options.resources_zip_path]
    ).WriteRTxtFile(options.rtxt_path)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
