#!/usr/bin/env python3
# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to fetch reclient cfgs."""

import argparse
import os
import posixpath
import re
import subprocess
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))

def ClangRevision():
    sys.path.insert(0, os.path.join(THIS_DIR, '..', '..',
                                    'tools', 'clang', 'scripts'))
    import update
    sys.path.pop(0)
    return update.PACKAGE_VERSION

def NaclRevision():
    return subprocess.check_output(
        ['git', 'log', '-1', '--format=%H'],
        cwd=os.path.join(THIS_DIR, '..', '..', 'native_client'),
    ).decode('utf-8').strip()

def CipdInstall(pkg_name, ref, directory):
    print('install %s %s in %s' % (pkg_name, ref, directory))
    if not os.path.exists(directory):
      os.makedirs(directory, mode=0o755)
    if not os.path.exists(os.path.join(directory, '.cipd')):
      subprocess.check_call(['cipd', 'init', '-force'], cwd=directory)
    subprocess.check_call(
        ['cipd', 'install', pkg_name, ref],
        cwd=directory)

def RbeProjectFromEnv():
    instance = os.environ.get('RBE_instance')
    if not instance:
        return None
    m = re.fullmatch(r'projects/([-\w]+)/instances/[-\w]+', instance)
    if not m:
        return None
    return m.group(1)

def main():
    parser = argparse.ArgumentParser(description='fetch reclient cfgs')
    parser.add_argument('--rbe_project',
                        help='RBE instance project id',
                        default=RbeProjectFromEnv())
    parser.add_argument('--cipd_prefix',
                        help='cipd package name prefix',
                        default='infra_internal/rbe/reclient_cfgs')

    args = parser.parse_args()
    if not args.rbe_project:
        print('RBE project is not specified')
        return 1

    print('fetch reclient_cfgs for RBE project %s...' % args.rbe_project)

    cipd_prefix = posixpath.join(args.cipd_prefix, args.rbe_project)

    clang_revision = ClangRevision()
    if clang_revision:
        CipdInstall(posixpath.join(cipd_prefix, 'chromium-browser-clang'),
                    ref='revision/' + clang_revision,
                    directory=os.path.join(THIS_DIR, 'chromium-browser-clang'))
    else:
        print('failed to detect clang revision')

    nacl_revision = NaclRevision()
    if nacl_revision:
        CipdInstall(posixpath.join(cipd_prefix, 'nacl'),
                    ref='revision/' + nacl_revision,
                    directory=os.path.join(THIS_DIR, 'nacl'))
    else:
        print('failed to detect nacl revision')

    return 0

if __name__ == '__main__':
    sys.exit(main())
