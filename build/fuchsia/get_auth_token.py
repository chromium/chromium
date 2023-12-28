#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Print the default service account's auth token to stdout."""

from __future__ import absolute_import
import os
import subprocess
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
from common import DIR_SRC_ROOT

sys.path.append(os.path.join(DIR_SRC_ROOT, 'build'))
import find_depot_tools


def main():
  luci_auth = os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'luci-auth')
  proc = subprocess.run([
      luci_auth, 'token', '-scopes',
      'https://www.googleapis.com/auth/devstorage.read_only'
  ],
                        encoding='utf-8')
  return proc.returncode


if __name__ == '__main__':
  sys.exit(main())
