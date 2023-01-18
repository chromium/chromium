#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to fetch reclient cfgs."""

import argparse
import glob
import os
import posixpath
import re
import shutil
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
    nacl_dir = os.path.join(THIS_DIR, '..', '..', 'native_client')
    if not os.path.exists(nacl_dir):
      return None
    return subprocess.check_output(
        ['git', 'log', '-1', '--format=%H'],
        cwd= nacl_dir,
    ).decode('utf-8').strip()

def CipdEnsure(pkg_name, ref, directory):
    print('ensure %s %s in %s' % (pkg_name, ref, directory))
    ensure_file = """
$ParanoidMode CheckPresence
{pkg} {ref}
""".format(pkg=pkg_name, ref=ref).encode('utf-8')
    output = subprocess.check_output(
        ' '.join(['cipd', 'ensure', '-log-level=debug', '-root', directory,
                  '-ensure-file', '-']), shell=True, input=ensure_file)
    print(output)

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

    tool_revisions = {
        'chromium-browser-clang': ClangRevision(),
        'nacl': NaclRevision(),
        'python': '3.8.0',
    }
    for toolchain in tool_revisions:
      revision = tool_revisions[toolchain]
      if not revision:
        print('failed to detect %s revision' % toolchain)
        continue

      toolchain_root = os.path.join(THIS_DIR, toolchain)
      cipd_ref = 'revision/' + revision
      # 'cipd ensure' initializes the directory.
      CipdEnsure(posixpath.join(cipd_prefix, toolchain),
                  ref=cipd_ref,
                  directory=toolchain_root)
      # support legacy (win-cross-experiments) and new (win-cross)
      # TODO(crbug.com/1407557): drop -experiments support
      wcedir = os.path.join(THIS_DIR, 'win-cross', toolchain)
      if not os.path.exists(wcedir):
          os.makedirs(wcedir, mode=0o755)
      for win_cross_cfg_dir in ['win-cross','win-cross-experiments']:
          if os.path.exists(os.path.join(toolchain_root, win_cross_cfg_dir)):
              # copy in win-cross*/toolchain
              # as windows may not use symlinks.
              for cfg in glob.glob(os.path.join(toolchain_root,
                                                win_cross_cfg_dir,
                                                '*.cfg')):
                  fname = os.path.join(wcedir, os.path.basename(cfg))
                  if os.path.exists(fname):
                    os.chmod(fname, 0o777)
                    os.remove(fname)
                  print('Copy from %s to %s...' % (cfg, fname))
                  shutil.copy(cfg, fname)

    return 0

if __name__ == '__main__':
    sys.exit(main())
