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
import time
import threading
import uuid

from symbolizer import SymbolizerFilter

FAR = common.GetHostToolPathFromPlatform('far')

# Amount of time to wait for the termination of the system log output thread.
_JOIN_TIMEOUT_SECS = 5


def _AttachKernelLogReader(target):
  """Attaches a kernel log reader as a long-running SSH task."""

  logging.info('Attaching kernel logger.')
  return target.RunCommandPiped(['dlog', '-f'], stdin=open(os.devnull, 'r'),
                                stdout=subprocess.PIPE)


class MergedInputStream(object):
  """Merges a number of input streams into a UNIX pipe on a dedicated thread.
  Terminates when the file descriptor of the primary stream (the first in
  the sequence) is closed."""

  def __init__(self, streams):
    assert len(streams) > 0
    self._streams = streams
    self._output_stream = None
    self._thread = None

  def Start(self):
    """Returns a pipe to the merged output stream."""

    read_pipe, write_pipe = os.pipe()

    # Disable buffering for the stream to make sure there is no delay in logs.
    self._output_stream = os.fdopen(write_pipe, 'w', 0)
    self._thread = threading.Thread(target=self._Run)
    self._thread.start();

    return os.fdopen(read_pipe, 'r')

  def _Run(self):
    streams_by_fd = {}
    primary_fd = self._streams[0].fileno()
    for s in self._streams:
      streams_by_fd[s.fileno()] = s

    # Set when the primary FD is closed. Input from other FDs will continue to
    # be processed until select() runs dry.
    flush = False

    # The lifetime of the MergedInputStream is bound to the lifetime of
    # |primary_fd|.
    while primary_fd:
      # When not flushing: block until data is read or an exception occurs.
      rlist, _, xlist = select.select(streams_by_fd, [], streams_by_fd)

      if len(rlist) == 0 and flush:
        break

      for fileno in xlist:
        del streams_by_fd[fileno]
        if fileno == primary_fd:
          primary_fd = None

      for fileno in rlist:
        line = streams_by_fd[fileno].readline()
        if line:
          self._output_stream.write(line + '\n')
        else:
          del streams_by_fd[fileno]
          if fileno == primary_fd:
            primary_fd = None

    # Flush the streams by executing nonblocking reads from the input file
    # descriptors until no more data is available,  or all the streams are
    # closed.
    while streams_by_fd:
      rlist, _, _ = select.select(streams_by_fd, [], [], 0)

      if not rlist:
        break

      for fileno in rlist:
        line = streams_by_fd[fileno].readline()
        if line:
          self._output_stream.write(line + '\n')
        else:
          del streams_by_fd[fileno]


def _GetComponentUri(package_name):
  return 'fuchsia-pkg://fuchsia.com/%s#meta/%s.cmx' % (package_name,
                                                       package_name)


class RunPackageArgs:
  """RunPackage() configuration arguments structure.

  symbolizer_config: A newline delimited list of source files contained
      in the package. Omitting this parameter will disable symbolization.
  system_logging: If set, connects a system log reader to the target.
  target_staging_path: Path to which package FARs will be staged, during
      installation. Defaults to staging into '/data'.
  """
  def __init__(self):
    self.symbolizer_config = None
    self.system_logging = False
    self.target_staging_path = '/data'

  @staticmethod
  def FromCommonArgs(args):
    run_package_args = RunPackageArgs()
    run_package_args.system_logging = args.include_system_logs
    run_package_args.target_staging_path = args.target_staging_path
    return run_package_args


def _DrainStreamToStdout(stream, quit_event):
  """Outputs the contents of |stream| until |quit_event| is set."""

  while not quit_event.is_set():
    rlist, _, _ = select.select([ stream ], [], [], 0.1)
    if rlist:
      line = rlist[0].readline()
      if not line:
        return
      print(line.rstrip())


def RunPackage(output_dir, target, package_paths, package_name,
               package_args, args):
  """Installs the Fuchsia package at |package_path| on the target,
  executes it with |package_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_paths: The paths to the .far packages to be installed.
  package_name: The name of the primary package to run.
  package_args: The arguments which will be passed to the Fuchsia process.
  args: Structure of arguments to configure how the package will be run.

  Returns the exit code of the remote package process."""

  system_logger = (
      _AttachKernelLogReader(target) if args.system_logging else None)
  try:
    if system_logger:
      # Spin up a thread to asynchronously dump the system log to stdout
      # for easier diagnoses of early, pre-execution failures.
      log_output_quit_event = multiprocessing.Event()
      log_output_thread = threading.Thread(
          target=lambda: _DrainStreamToStdout(system_logger.stdout,
                                              log_output_quit_event))
      log_output_thread.daemon = True
      log_output_thread.start()

    target.InstallPackage(package_paths)

    if system_logger:
      log_output_quit_event.set()
      log_output_thread.join(timeout=_JOIN_TIMEOUT_SECS)

    logging.info('Running application.')
    command = ['run', _GetComponentUri(package_name)] + package_args
    process = target.RunCommandPiped(command,
                                     stdin=open(os.devnull, 'r'),
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)

    if system_logger:
      output_stream = MergedInputStream([process.stdout,
                                         system_logger.stdout]).Start()
    else:
      output_stream = process.stdout

    # Run the log data through the symbolizer process.
    build_ids_paths = map(
        lambda package_path: os.path.join(
            os.path.dirname(package_path), 'ids.txt'),
        package_paths)
    output_stream = SymbolizerFilter(output_stream, build_ids_paths)

    for next_line in output_stream:
      print(next_line.rstrip())

    process.wait()
    if process.returncode == 0:
      logging.info('Process exited normally with status code 0.')
    else:
      # The test runner returns an error status code if *any* tests fail,
      # so we should proceed anyway.
      logging.warning('Process exited with status code %d.' %
                      process.returncode)

  finally:
    if system_logger:
      logging.info('Terminating kernel log reader.')
      log_output_quit_event.set()
      log_output_thread.join()
      system_logger.kill()

  return process.returncode
