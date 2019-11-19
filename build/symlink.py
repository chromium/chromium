#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

description = """
Make a symlink and optionally touch a file (to handle dependencies).
"""
usage = "%prog [options] source[ source ...] linkname"
epilog = """
A sym link to source is created at linkname. If multiple sources are specfied,
then linkname is assumed to be a directory, and will contain all the links to
the sources (basenames identical to their source).

On Windows, this will use hard links (mklink /H) to avoid requiring elevation.
This means that if the original is deleted and replaced, the link will still
have the old contents. This is not expected to interfere with the Chromium
build.
"""

import errno
import optparse
import os.path
import shutil
import subprocess
import sys


def Main(argv):
  parser = optparse.OptionParser(usage=usage, description=description,
                                 epilog=epilog)
  parser.add_option('-f', '--force', action='store_true')
  parser.add_option('--touch')

  options, args = parser.parse_args(argv[1:])
  if len(args) < 2:
    parser.error('at least two arguments required.')

  target = args[-1]
  sources = args[:-1]
  for s in sources:
    t = os.path.join(target, os.path.basename(s))
    if len(sources) == 1 and not os.path.isdir(target):
      t = target
    t = os.path.expanduser(t)
    if os.path.realpath(t) == os.path.realpath(s):
      continue
    try:
      # N.B. Python 2.x does not have os.symlink for Windows.
      #   Python 3 has os.symlink for Windows, but requires either the admin-
      #   granted privilege SeCreateSymbolicLinkPrivilege or, as of Windows 10
      #   1703, that Developer Mode be enabled. Hard links and junctions do not
      #   require any extra privileges to create.
      if os.name == 'nt':
        # mklink does not tolerate /-delimited path names.
        t = t.replace('/', '\\')
        s = s.replace('/', '\\')
        # N.B. This tool only handles file hardlinks, not directory junctions.
        subprocess.check_output(['cmd.exe', '/c', 'mklink', '/H', t, s],
                                stderr=subprocess.STDOUT)
      else:
        os.symlink(s, t)
    except OSError as e:
      if e.errno == errno.EEXIST and options.force:
        if os.path.isdir(t):
          shutil.rmtree(t, ignore_errors=True)
        else:
          os.remove(t)
        os.symlink(s, t)
      else:
        raise
    except subprocess.CalledProcessError as e:
      # Since subprocess.check_output does not return an easily checked error
      # number, in the 'force' case always assume it is 'file already exists'
      # and retry.
      if options.force:
        if os.path.isdir(t):
          shutil.rmtree(t, ignore_errors=True)
        else:
          os.remove(t)
        subprocess.check_output(e.cmd, stderr=subprocess.STDOUT)
      else:
        raise


  if options.touch:
    with open(options.touch, 'w') as f:
      pass


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
