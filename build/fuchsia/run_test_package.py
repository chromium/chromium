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
import uuid

from symbolizer import BuildIdsPaths, RunSymbolizer, SymbolizerFilter

FAR = common.GetHostToolPathFromPlatform('far')

# Amount of time to wait for the termination of the system log output thread.
_JOIN_TIMEOUT_SECS = 5


def _AttachKernelLogReader(target):
  """Attaches a kernel log reader as a long-running SSH task."""

  logging.info('Attaching kernel logger.')
  return target.RunCommandPiped(['dlog', '-f'],
                                stdin=open(os.devnull, 'r'),
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)


class SystemLogReader(object):
  """Collects and symbolizes Fuchsia system log to a file."""

  def __init__(self):
    self._listener_proc = None
    self._symbolizer_proc = None
    self._system_log = None

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Stops the system logging processes and closes the output file."""
    if self._symbolizer_proc:
      self._symbolizer_proc.kill()
    if self._listener_proc:
      self._listener_proc.kill()
    if self._system_log:
      self._system_log.close()

  def Start(self, target, package_paths, system_log_file):
    """Start a system log reader as a long-running SSH task."""
    logging.debug('Writing fuchsia system log to %s' % system_log_file)

    self._listener_proc = target.RunCommandPiped(['log_listener'],
                                                 stdout=subprocess.PIPE,
                                                 stderr=subprocess.STDOUT)

    self._system_log = open(system_log_file, 'w', buffering=1)
    self._symbolizer_proc = RunSymbolizer(self._listener_proc.stdout,
                                          self._system_log,
                                          BuildIdsPaths(package_paths))


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

    self._output_stream = os.fdopen(write_pipe, 'wb', 1)
    self._thread = threading.Thread(target=self._Run)
    self._thread.start()

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
          self._output_stream.write(line)
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
          self._output_stream.write(line)
        else:
          del streams_by_fd[fileno]


def _GetComponentUri(package_name):
  return 'fuchsia-pkg://fuchsia.com/%s#meta/%s.cmx' % (package_name,
                                                       package_name)


class RunTestPackageArgs:
  """RunTestPackage() configuration arguments structure.

  code_coverage: If set, the test package will be run via 'runtests', and the
                 output will be saved to /tmp folder on the device.
  system_logging: If set, connects a system log reader to the target.
  test_realm_label: Specifies the realm name that run-test-component should use.
      This must be specified if a filter file is to be set, or a results summary
      file fetched after the test suite has run.
  use_run_test_component: If True then the test package will be run hermetically
                          via 'run-test-component', rather than using 'run'.
  """

  def __init__(self):
    self.code_coverage = False
    self.system_logging = False
    self.test_realm_label = None
    self.use_run_test_component = False

  @staticmethod
  def FromCommonArgs(args):
    run_test_package_args = RunTestPackageArgs()
    run_test_package_args.code_coverage = args.code_coverage
    run_test_package_args.system_logging = args.include_system_logs
    return run_test_package_args


def _DrainStreamToStdout(stream, quit_event):
  """Outputs the contents of |stream| until |quit_event| is set."""

  while not quit_event.is_set():
    rlist, _, _ = select.select([stream], [], [], 0.1)
    if rlist:
      line = rlist[0].readline()
      if not line:
        return
      print(line.rstrip())


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

  system_logger = (_AttachKernelLogReader(target)
                   if args.system_logging else None)
  try:
    if system_logger:
      # Spin up a thread to asynchronously dump the system log to stdout
      # for easier diagnoses of early, pre-execution failures.
      log_output_quit_event = multiprocessing.Event()
      log_output_thread = threading.Thread(target=lambda: _DrainStreamToStdout(
          system_logger.stdout, log_output_quit_event))
      log_output_thread.daemon = True
      log_output_thread.start()

    with target.GetPkgRepo():
      target.InstallPackage(package_paths)

      if system_logger:
        log_output_quit_event.set()
        log_output_thread.join(timeout=_JOIN_TIMEOUT_SECS)

      logging.info('Running application.')

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
      else:
        command = ['run', _GetComponentUri(package_name)]

      command.extend(package_args)

      process = target.RunCommandPiped(command,
                                       stdin=open(os.devnull, 'r'),
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT)

      if system_logger:
        output_stream = MergedInputStream(
            [process.stdout, system_logger.stdout]).Start()
      else:
        output_stream = process.stdout

      # Run the log data through the symbolizer process.
      output_stream = SymbolizerFilter(output_stream,
                                       BuildIdsPaths(package_paths))

      for next_line in output_stream:
        # TODO(crbug/1198733): Switch to having stream encode to utf-8 directly
        # once we drop Python 2 support.
        print(next_line.encode('utf-8').rstrip())

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
