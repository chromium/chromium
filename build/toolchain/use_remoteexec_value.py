#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to decide use_remoteexec value.

It is called by rbe.gni via exec_script,
or used in depot_tools' autoninja or siso wrapper.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.path.pardir))
import gn_helpers

def use_remoteexec_value(output_dir):
  """Returns use_remoteexec value."""
  # TODO(crbug.com/341167943): Use remoteexec by default.
  return gn_helpers.ReadArgsGN(output_dir).get("use_remoteexec", False)

if __name__ == "__main__":
  # exec_script runs in output directory.
  print(str(use_remoteexec_value(".")).lower())
