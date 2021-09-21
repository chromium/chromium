#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Starts up a long running emulator for unit testing and developer use."""

import argparse
import common
import common_args
import logging
import os
import time
import subprocess

from aemu_target import AemuTarget
from fvdl_target import FvdlTarget


def main():
  parser = argparse.ArgumentParser(
      description='Launches a long-running emulator that can '
      'be re-used for multiple test runs.')
  AddLongRunningArgs(parser)
  FvdlTarget.RegisterArgs(parser)
  AemuTarget.RegisterArgs(parser)
  common_args.AddCommonArgs(parser)
  args = parser.parse_args()
  args.out_dir = None
  args.device = 'fvdl'
  common_args.ConfigureLogging(args)
  with common_args.GetDeploymentTargetForArgs(args) as fvdl_target:
    if fvdl_target._with_network:
      logging.info('If you haven\'t set up tuntap, you may be prompted '
                   'for your sudo password to set up tuntap.')
    fvdl_target.Start()
    logging.info('Emulator successfully started up! If you are running '
                 'multiple fuchsia devices, specify the port the ip address '
                 'via the --host flag.')
    if fvdl_target._with_network:
      logging.info('You can now use the "-d" flag when running '
                   'Chrome Fuchsia tests to target this emulator.')
    while fvdl_target._IsEmuStillRunning():
      time.sleep(10)
      pass


def AddLongRunningArgs(arg_parser):
  fvdl_args = arg_parser.add_argument_group('FVDL arguments')
  fvdl_args.add_argument('--target-cpu',
                         default=common_args.GetHostArchFromPlatform(),
                         help='Set target_cpu for the emulator. Defaults '
                         'to the same architecture as host cpu.')
  fvdl_args.add_argument('--without-network',
                         action='store_false',
                         dest='with_network',
                         default=True,
                         help='Run emulator without emulated nic via tun/tap.')


if __name__ == '__main__':
  main()
