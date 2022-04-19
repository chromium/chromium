#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sets the app container ACLs on directory."""

import os
import argparse
import logging
import platform
import subprocess
import sys

logging.basicConfig(level=logging.INFO)


# ACL might be explicitly set or inherited.
CORRECT_ACL_VARIANTS = [
    'APPLICATION PACKAGE AUTHORITY' \
    '\\ALL RESTRICTED APPLICATION PACKAGES:(OI)(CI)(RX)', \
    'APPLICATION PACKAGE AUTHORITY' \
    '\\ALL RESTRICTED APPLICATION PACKAGES:(I)(OI)(CI)(RX)'
]


def set_lpac_acls(acl_dir):
  """Sets LPAC ACLs on a directory. Windows 10 only."""
  if platform.release() != '10':
    return
  try:
    existing_acls = subprocess.check_output(['icacls', acl_dir],
                                            stderr=subprocess.STDOUT,
                                            universal_newlines=True)
  except subprocess.CalledProcessError as e:
    logging.error('Failed to retrieve existing ACLs for directory %s', acl_dir)
    logging.error('Command output: %s', e.output)
    sys.exit(e.returncode)
  acls_correct = False
  for acl in CORRECT_ACL_VARIANTS:
    if acl in existing_acls:
      acls_correct = True
  if not acls_correct:
    try:
      existing_acls = subprocess.check_output(
          ['icacls', acl_dir, '/grant', '*S-1-15-2-2:(OI)(CI)(RX)'],
          stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      logging.error(
          'Failed to retrieve existing ACLs for directory %s', acl_dir)
      logging.error('Command output: %s', e.output)
      sys.exit(e.returncode)


def main():
  parser = argparse.ArgumentParser(
      description='Sets App Container ACL on a directory.')
  parser.add_argument('--stamp',
                      required=False,
                      help='Touch this stamp file on success.')
  parser.add_argument('--dir', required=True, help='Set ACL on this directory.')
  #  parser.add_argument('--fail', required=True, help='Argument to fail.')
  args = parser.parse_args()

  set_lpac_acls(os.path.abspath(args.dir))
  if args.stamp:
    open(args.stamp, 'w').close()  # Update mtime on stamp file.


if __name__ == '__main__':
  main()
