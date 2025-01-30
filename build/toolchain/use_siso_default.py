#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to decide use_siso default value.

`use_siso_default` is called by siso.gni via exec_script, and
`use_siso_default_and_suggest_siso` is called by autoninja in depot_tools.
"""
# TODO(crbug.com/379584977): move this to depot_tools once `use_siso`
# is not used for build graph.

import os
import shutil
import sys

_SISO_SUGGESTION = """Please run 'gn clean {output_dir}' when convenient to upgrade this output directory to Siso (Chromium’s Ninja replacement). If you run into any issues, please file a bug via go/siso-bug and switch back temporarily by setting the GN arg 'use_siso = false'"""


def _is_google_corp_machine():
  """This assumes that corp machine has gcert binary in known location."""
  return shutil.which("gcert") is not None


def _siso_supported(output_dir):
  """Returns whether siso is supported for the current scenario."""
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


def use_siso_default(output_dir, suggest_siso=False):
  """Returns use_siso default value."""
  if not _siso_supported(output_dir):
      return False

  # This output directory is already using Siso.
  if os.path.exists(os.path.join(output_dir, ".siso_deps")):
    return True

  # This output directory is still using Ninja.
  if os.path.exists(os.path.join(output_dir, ".ninja_deps")):
    if suggest_siso:
        print(_SISO_SUGGESTION.format(output_dir=output_dir), file=sys.stderr)
    return False

  return True

def use_siso_default_and_suggest_siso(output_dir):
  """Returns use_siso default value and suggests to use siso."""
  return use_siso_default(output_dir, suggest_siso = True)


if __name__ == "__main__":
  # exec_script runs in output directory.
  print(str(use_siso_default(".")).lower())
