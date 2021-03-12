# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for checking if we're running Python 2 or 3."""

from __future__ import print_function

import subprocess
import sys

print("true" if sys.version_info.major == 2 else "false")
