#!/usr/bin/env python
#
# Copyright 2021 The Chromium Authors. All rights reserved.
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
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  args, runtime_args = parser.parse_known_args()

  command_substituted = [
      chunk.replace('%args%', ' '.join(runtime_args))
      for chunk in shlex.split(args.command)
  ]

  with GetDeploymentTargetForArgs(args) as target:
    target.Start()
    target.StartSystemLog(args.package)

    # Extend the lifetime of |pkg_repo| beyond InstallPackage so that the
    # package can be instantiated after resolution.
    with target.GetPkgRepo() as pkg_repo:
      target.InstallPackage(args.package)
      process = target.RunFFXCommand(command_substituted)

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
