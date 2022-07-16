# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains a helper function for deploying and executing a packaged
executable on a Target."""

from __future__ import print_function

import common
import hashlib
import logging
import multiprocessing
import os
import re
import select
import subprocess
import sys
import threading
import time
import uuid

from symbolizer import BuildIdsPaths, RunSymbolizer, SymbolizerFilter

FAR = common.GetHostToolPathFromPlatform('far')


def _GetComponentUri(package_name):
  return 'fuchsia-pkg://fuchsia.com/%s#meta/%s.cmx' % (package_name,
                                                       package_name)


class RunTestPackageArgs:
  """RunTestPackage() configuration arguments structure.

  code_coverage: If set, the test package will be run via 'runtests', and the
                 output will be saved to /tmp folder on the device.
  test_realm_label: Specifies the realm name that run-test-component should use.
      This must be specified if a filter file is to be set, or a results summary
      file fetched after the test suite has run.
  use_run_test_component: If True then the test package will be run hermetically
                          via 'run-test-component', rather than using 'run'.
  """

  def __init__(self):
    self.code_coverage = False
    self.test_realm_label = None
    self.use_run_test_component = False

  @staticmethod
  def FromCommonArgs(args):
    run_test_package_args = RunTestPackageArgs()
    run_test_package_args.code_coverage = args.code_coverage
    return run_test_package_args


def RunTestPackage(output_dir, target, package_paths, package_name,
                   package_args, args):
  """Installs the Fuchsia package at |package_path| on the target,
  executes it with |package_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_paths: The paths to the .far packages to be installed.
  package_name: The name of the primary package to run.
  package_args: The arguments which will be passed to the Fuchsia process.
  args: RunTestPackageArgs instance configuring how the package will be run.

  Returns the exit code of the remote package process."""

  with target.GetPkgRepo():
    start_time = time.time()
    target.InstallPackage(package_paths)
    logging.info('Test installed in {:.2f} seconds.'.format(time.time() -
                                                            start_time))

    # TODO(crbug.com/1156768): Deprecate runtests.
    if args.code_coverage:
      # runtests requires specifying an output directory and a double dash
      # before the argument list.
      command = ['runtests', '-o', '/tmp', _GetComponentUri(package_name)]
      if args.test_realm_label:
        command += ['--realm-label', args.test_realm_label]
      command += ['--']
    elif args.use_run_test_component:
      command = ['run-test-component']
      if args.test_realm_label:
        command += ['--realm-label=%s' % args.test_realm_label]
      command.append(_GetComponentUri(package_name))
      command.append('--')
    else:
      command = ['run', _GetComponentUri(package_name)]

    command.extend(package_args)

    process = target.RunCommandPiped(command,
                                     stdin=open(os.devnull, 'r'),
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT,
                                     text=True)

    # Print the test process' symbolized standard output.
    for next_line in SymbolizerFilter(process.stdout,
                                      BuildIdsPaths(package_paths)):
      print(next_line.rstrip())

    process.wait()
    if process.returncode == 0:
      logging.info('Process exited normally with status code 0.')
    else:
      # The test runner returns an error status code if *any* tests fail,
      # so we should proceed anyway.
      logging.warning('Process exited with status code %d.' %
                      process.returncode)

  return process.returncode
