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

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
import use_siso_default

# instead of finding depot_tools in PATH,
# just use pinned third_party/depot_tools.
sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), "..", "..", "third_party",
                 "depot_tools"))
import gn_helper


def _gn_bool(value):
  if value == "true":
    return True
  if value == "false":
    return False
  raise Exception("invalid bool value %s" % value)


def use_remoteexec_value(output_dir):
  """Returns use_remoteexec value."""
  use_remoteexec = None
  for k, v in gn_helper.args(output_dir):
    if k == "use_remoteexec":
      use_remoteexec = _gn_bool(v)
  # If args.gn has use_remoteexec, use it.
  if use_remoteexec is not None:
    return use_remoteexec

  # TODO(crbug.com/341167943): Use remoteexec by default.
  return False


if __name__ == "__main__":
  # exec_script runs in output directory.
  print(str(use_remoteexec_value(".")).lower())
