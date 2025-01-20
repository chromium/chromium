#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to decide use_reclient value.

It is called by rbe.gni via exec_script,
or used in depot_tools' autoninja or siso wrapper.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
import use_siso_default

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.path.pardir))
import gn_helpers

def use_reclient_value(output_dir):
  """Returns use_reclient value."""
  use_remoteexec = None
  use_reclient = None
  use_siso = use_siso_default.use_siso_default(output_dir)
  args = gn_helpers.ReadArgsGN(output_dir)
  use_remoteexec = args.get('use_remoteexec', False)
  use_reclient = args.get('use_reclient')
  use_siso = args.get('use_siso', use_siso)
  # If args.gn has use_reclient, use it.
  if use_reclient is not None:
    return use_reclient

  # If .reproxy_tmp dir exists, keep to use reclient.
  if os.path.exists(os.path.join(output_dir, ".reproxy_tmp")):
    return True

  if not use_remoteexec:
    return False
  # TODO(crbug.com/341167943): Use reclient if use_remoteexec=true and
  # use_siso=false.
  return True


if __name__ == "__main__":
  # exec_script runs in output directory.
  print(str(use_reclient_value(".")).lower())
