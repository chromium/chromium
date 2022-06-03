# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script gets default goma_dir for depot_tools goma.

import os
import sys


def main():
  gomacc = 'gomacc'
  candidates = []
  if sys.platform in ['win32', 'cygwin']:
    gomacc = 'gomacc.exe'

  for path in os.environ.get('PATH', '').split(os.pathsep):
    # normpath() required to strip trailing slash when present.
    if os.path.basename(os.path.normpath(path)) == 'depot_tools':
      candidates.append(os.path.join(path, '.cipd_bin'))

  for d in candidates:
    if os.path.isfile(os.path.join(d, gomacc)):
      sys.stdout.write(d)
      return 0
  # mb analyze step set use_goma=true, but goma_dir="",
  # and bot doesn't have goma in default locataion above.
  # to mitigate this, just use initial depot_tools path
  # or default path as before (if depot_tools doesn't exist
  # in PATH).
  # TODO(ukai): crbug.com/1073276: fix mb analyze step and make it hard error?
  if sys.platform in ['win32', 'cygwin']:
    sys.stdout.write('C:\\src\\goma\\goma-win64')
  elif 'GOMA_DIR' in os.environ:
    sys.stdout.write(os.environ.get('GOMA_DIR'))
  else:
    sys.stdout.write(os.path.join(os.environ.get('HOME', ''), 'goma'))
  return 0


if __name__ == '__main__':
  sys.exit(main())
