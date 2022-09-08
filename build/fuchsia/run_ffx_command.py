#!/usr/bin/env python
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Deploys packages and runs an FFX command on a Fuchsia target."""

import argparse
import logging
import os
import pkg_repo
import shlex
import sys
import tempfile
import time

from common_args import AddCommonArgs, AddTargetSpecificArgs, \
                        ConfigureLogging, GetDeploymentTargetForArgs


def main():
  parser = argparse.ArgumentParser()

  logging.getLogger().setLevel(logging.INFO)
  parser.add_argument('--command',
                      required=True,
                      help='FFX command to run. Runtime arguments are handled '
                      'using the %%args%% placeholder.')
  parser.add_argument('child_args',
                      nargs='*',
                      help='Arguments for the command.')
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  args = parser.parse_args()

  # Prepare the arglist for "ffx". %args% is replaced with all positional
  # arguments given to the script.
  ffx_args = shlex.split(args.command)
  # replace %args% in the command with the given arguments.
  try:
    args_index = ffx_args.index('%args%')
    ffx_args[args_index:args_index + 1] = args.child_args
  except ValueError:
    # %args% is not present; use the command as-is.
    pass

  with GetDeploymentTargetForArgs(args) as target:
    target.Start()
    target.StartSystemLog(args.package)

    # Extend the lifetime of |pkg_repo| beyond InstallPackage so that the
    # package can be instantiated after resolution.
    with target.GetPkgRepo() as pkg_repo:
      target.InstallPackage(args.package)
      process = target.RunFFXCommand(ffx_args)

      # It's possible that components installed by this script may be
      # instantiated at arbitrary points in the future.
      # This script (specifically |pkg_repo|) must be kept alive until it
      # is explicitly terminated by the user, otherwise pkgsvr will
      # throw an error when launching components.
      logging.info('Command is now running. Press CTRL-C to exit.')
      try:
        while True:
          time.sleep(1)
      except KeyboardInterrupt:
        pass

  return 0


if __name__ == '__main__':
  sys.exit(main())
