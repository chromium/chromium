#!/user/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys


def GetScriptName():
  return os.path.basename(os.path.abspath(sys.argv[0]))


def KCamelToShouty(s):
  """Convert |s| from kCamelCase or CamelCase to SHOUTY_CASE.

  kFooBar -> FOO_BAR
  FooBar -> FOO_BAR
  FooBAR9 -> FOO_BAR9
  FooBARBaz -> FOO_BAR_BAZ
  """
  if not re.match(r'^k?([A-Z][^A-Z]+|[A-Z0-9]+)+$', s):
    return s
  # Strip the leading k.
  s = re.sub(r'^k', '', s)
  # Add _ between title words and anything else.
  s = re.sub(r'([^_])([A-Z][^A-Z_0-9]+)', r'\1_\2', s)
  # Add _ between lower -> upper transitions.
  s = re.sub(r'([^A-Z_0-9])([A-Z])', r'\1_\2', s)
  return s.upper()
