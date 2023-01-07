# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


def ParseVersion(lines):
  version_keys = ['MAJOR', 'MINOR']
  version_vals = {}
  for line in lines:
    key, val = line.strip().split('=', 1)
    if key in version_keys:
      if key in version_vals or not val.isdigit():
        return None
      version_vals[key] = int(val)

  if set(version_keys) != set(version_vals):
    # We didn't see all parts of the version.
    return None

  return collections.namedtuple('Version', ['major', 'minor'])(
      major=version_vals['MAJOR'], minor=version_vals['MINOR'])
