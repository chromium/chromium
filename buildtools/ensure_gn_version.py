#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Ensure that CIPD fetched the right GN version.

Due to crbug.com/944367, using cipd in gclient to fetch GN binaries
may not always work right. This is a script that can be used as
a backup method to force-install GN at the right revision, just in case.

It should be used as a gclient hook alongside fetching GN via CIPD
until we have a proper fix in place.

TODO(crbug.com/944667): remove this script when it is no longer needed.
"""

from __future__ import print_function

import argparse
import errno
import io
import os
import re
import stat
import subprocess
import sys

try:
  import urllib2 as urllib
except ImportError:
  import urllib.request as urllib

import zipfile


BUILDTOOLS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.dirname(BUILDTOOLS_DIR)

def ChmodGnFile(path_to_exe):
  """Makes the gn binary executable for all and writable for the user."""
  os.chmod(path_to_exe,
           stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR |  # This is 0o755.
           stat.S_IRGRP | stat.S_IXGRP |
           stat.S_IROTH | stat.S_IXOTH)

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('version',
          help='CIPD "git_revision:XYZ" label for GN to sync to')

  args = parser.parse_args()

  if not args.version.startswith('git_revision:'):
    print('Unknown version format: %s' % args.version)
    return 2

  desired_revision = args.version[len('git_revision:'):]

  if sys.platform == 'darwin':
    platform, member, dest_dir = ('mac-amd64', 'gn', 'mac')
  elif sys.platform == 'win32':
    platform, member, dest_dir = ('windows-amd64', 'gn.exe', 'win')
  else:
    platform, member, dest_dir = ('linux-amd64', 'gn', 'linux64')

  path_to_exe = os.path.join(BUILDTOOLS_DIR, dest_dir, member)
  cmd = [path_to_exe, '--version']
  cmd_str = ' '.join(cmd)
  try:
    out = subprocess.check_output(cmd,
                                  stderr=subprocess.STDOUT,
                                  cwd=SRC_DIR).decode(errors='replace')
  except subprocess.CalledProcessError as e:
    print('`%s` returned %d:\n%s' % (cmd_str, e.returncode, e.output))
    return 1
  except OSError as e:
    if e.errno != errno.ENOENT:
      print('`%s` failed:\n%s' % (cmd_str, e.strerror))
      return 1

    # The tool doesn't exist, so redownload it.
    out = ''

  if out:
    current_revision_match = re.findall(r'\((.*)\)', out)
    if current_revision_match:
      current_revision = current_revision_match[0]
      if desired_revision.startswith(current_revision):
        # We're on the right version, so we're done.
        return 0

  print("`%s` returned '%s', which wasn't what we were expecting."
          % (cmd_str, out.strip()))
  print("Force-installing %s to update it." % desired_revision)

  url = 'https://chrome-infra-packages.appspot.com/dl/gn/gn/%s/+/%s' % (
      platform, args.version)
  try:
    zipdata = urllib.urlopen(url).read()
  except urllib.HTTPError as e:
    print('Failed to download the package from %s: %d %s' % (
        url, e.code, e.reason))
    return 1

  try:
    # Make the existing file writable so that we can overwrite it.
    ChmodGnFile(path_to_exe)
  except OSError as e:
    if e.errno != errno.ENOENT:
      print('Failed to make %s writable:\n%s\n' % (path_to_exe, e.strerror))
      return 1

  try:
    zf = zipfile.ZipFile(io.BytesIO(zipdata))
    zf.extract(member, os.path.join(BUILDTOOLS_DIR, dest_dir))
  except OSError as e:
    print('Failed to extract the binary:\n%s\n' % e.strerror)
    return 1
  except (zipfile.LargeZipFile, zipfile.BadZipfile) as e:
    print('Zip containing gn was corrupt:\n%s\n' % e)
    return 1

  try:
    ChmodGnFile(path_to_exe)
  except OSError as e:
    print('Failed to make %s executable:\n%s\n' % (path_to_exe, e.strerror))
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
