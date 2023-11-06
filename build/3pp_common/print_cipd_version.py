#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import re
import subprocess

_DIR_SOURCE_ROOT = str(pathlib.Path(__file__).absolute().parents[2])


def main():
    parser = argparse.ArgumentParser()
    # Hide args set by wrappers so that using --help with the wrappers does not
    # show them.
    parser.add_argument('--subdir', required=True, help=argparse.SUPPRESS)
    parser.add_argument('--cipd-package',
                        required=True,
                        help=argparse.SUPPRESS)
    parser.add_argument('--git-log-url', help=argparse.SUPPRESS)
    parser.add_argument('--cipd-instance',
                        help='Uses value from DEPS by default')
    args = parser.parse_args()

    if not args.cipd_instance:
        cmd = [
            'gclient', 'getdep', '-r', f'src/{args.subdir}:{args.cipd_package}'
        ]
        args.cipd_instance = subprocess.check_output(cmd,
                                                     cwd=_DIR_SOURCE_ROOT,
                                                     text=True)

    cmd = [
        'cipd', 'describe', args.cipd_package, '-version', args.cipd_instance
    ]
    print(' '.join(cmd))
    output = subprocess.check_output(cmd, text=True)
    print(output, end='')
    if args.git_log_url:
        git_hashes = re.findall(r'version:.*?@(\w+)', output)
        if not git_hashes:
            print('Could not find git hash from output.')
        else:
            # Multiple version tags exist when multiple versions have the same sha1.
            last_version = git_hashes[-1]
            print()
            print('Recent commits:', args.git_log_url.format(last_version))


if __name__ == '__main__':
    main()
