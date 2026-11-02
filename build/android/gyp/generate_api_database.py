#!/usr/bin/env python3
#
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import shutil
import tempfile

from util import build_utils


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--r8-path', required=True, help='Path to R8 jar.')
    parser.add_argument('--jar-path', required=True, help='Path to SDK jar.')
    parser.add_argument(
        '--xml-path', required=True, help='Path to api-versions.xml.'
    )
    parser.add_argument(
        '--output', required=True, help='Output api database file.'
    )
    options = parser.parse_args()

    with tempfile.NamedTemporaryFile(suffix='.ser') as temp:
        temp_output = temp.name
        cmd = build_utils.JavaCmd() + [
            '-cp',
            options.r8_path,
            'com.android.tools.r8.ApiDatabaseGenerator',
            '--xml',
            options.xml_path,
            '--jar',
            options.jar_path,
            '--output',
            temp_output,
            '--allow-future-versions',
        ]
        build_utils.CheckOutput(cmd)
        shutil.copy(temp_output, options.output)


if __name__ == '__main__':
    main()
