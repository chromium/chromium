#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to decide use_siso default value.

It is called by siso.gni via exec_script,
or used in depot_tools' autoninja or siso wrapper.
"""
# TODO(crbug.com/379584977): move this to depot_tools once `use_siso`
# is not used for build graph.

import os
import shutil
import sys


def _is_google_corp_machine():
  """This assumes that corp machine has gcert binary in known location."""
  return shutil.which("gcert") is not None


def use_siso_default(output_dir):
  """Returns use_siso default value."""
  # This output directory is already using Siso.
  if os.path.exists(os.path.join(output_dir, ".siso_deps")):
    return True

  # This output directory is already using Ninja.
  if os.path.exists(os.path.join(output_dir, ".ninja_deps")):
    return False

  # If no .sisoenv, use Ninja.
  if not os.path.exists(
      os.path.join(os.path.dirname(__file__), "../config/siso/.sisoenv")):
    return False

  # If it's not chromium project, use Ninja.
  gclient_args_gni = os.path.join(os.path.dirname(__file__),
                                  "../config/gclient_args.gni")
  if not os.path.exists(gclient_args_gni):
    return False

  with open(gclient_args_gni) as f:
    if "build_with_chromium = true" not in f.read():
      return False

  # Use Siso by default for Googlers working on corp machine.
  if _is_google_corp_machine():
    return True

  # Otherwise, use Ninja, until we are ready to roll it out
  # on non-corp machines, too.
  # TODO(378078715): enable True by default.
  return False


if __name__ == "__main__":
  # exec_script runs in output directory.
  print(str(use_siso_default(".")).lower())
