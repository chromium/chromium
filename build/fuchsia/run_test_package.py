# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains a helper function for deploying and executing a packaged
executable on a Target."""


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

from exit_on_sig_term import ExitOnSigTerm
from symbolizer import BuildIdsPaths, RunSymbolizer

FAR = common.GetHostToolPathFromPlatform('far')

# Amount of time to wait for the termination of the system log output thread.
_JOIN_TIMEOUT_SECS = 5


def _AttachKernelLogReader(target):
  """Attaches a kernel log reader as a long-running SSH task."""

  logging.info('Attaching kernel logger.')
  return target.RunCommandPiped(['log_listener',
                                 '--since_now',
                                 '--hide_metadata',
                                 '--tag',
                                 'klog',
                                ],
                                stdin=open(os.devnull, 'r'),
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)


class MergedInputStream(object):
  """Merges a number of input streams into a UTF-8 encoded UNIX pipe on a
  dedicated thread. Terminates when the file descriptor of the primary stream
  (the first in the sequence) is closed."""

  def __init__(self, streams):
    assert len(streams) > 0
    self._streams = streams
    self._output_stream = None
    self._thread = None

  def Start(self):
    """Returns a pipe to the merged output stream."""

    read_pipe, write_pipe = os.pipe()

    self._output_stream = os.fdopen(write_pipe, 'wb', 0)
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
  return 'fuchsia-pkg://fuchsia.com/%s#meta/%s.cm' % (package_name,
                                                      package_name)


def _DrainStreamToStdout(stream, quit_event):
  """Outputs the contents of |stream| until |quit_event| is set."""

  while not quit_event.is_set():
    rlist, _, _ = select.select([stream], [], [], 0.1)
    if rlist:
      line = rlist[0].readline()
      if not line:
        return
      print(line.rstrip())


def _SymbolizeStream(input_fd, ids_txt_files):
  """Returns a Popen object for a symbolizer process invocation.
  input_fd: The data to symbolize.
  ids_txt_files: A list of ids.txt files which contain symbol data."""

  return RunSymbolizer(input_fd, subprocess.PIPE, ids_txt_files)


def RunTestPackage(target, ffx_session, package_paths, package_name,
                   package_args):
  """Installs the Fuchsia package at |package_path| on the target,
  executes it with |package_args|, and symbolizes its output.

  target: The deployment Target object that will run the package.
  ffx_session: An FfxSession object.
  package_paths: The paths to the .far packages to be installed.
  package_name: The name of the primary package to run.
  package_args: The arguments which will be passed to the Fuchsia process.

  Returns the exit code of the remote package process."""

  assert ffx_session
  kernel_logger = _AttachKernelLogReader(target)
  try:
    # Spin up a thread to asynchronously dump the system log to stdout
    # for easier diagnoses of early, pre-execution failures.
    log_output_quit_event = multiprocessing.Event()
    log_output_thread = threading.Thread(target=lambda: _DrainStreamToStdout(
        kernel_logger.stdout, log_output_quit_event))
    log_output_thread.daemon = True
    log_output_thread.start()

    with ExitOnSigTerm(), target.GetPkgRepo():
      on_target = True
      start_time = time.time()
      target.InstallPackage(package_paths)
      logging.info('Test installed in {:.2f} seconds.'.format(time.time() -
                                                              start_time))

      log_output_quit_event.set()
      log_output_thread.join(timeout=_JOIN_TIMEOUT_SECS)

      logging.info('Running application.')

      component_uri = _GetComponentUri(package_name)
      process = ffx_session.test_run(target.GetFfxTarget(), component_uri,
                                     package_args)

      # Symbolize klog and systemlog as separate streams. The symbolizer
      # protocol is stateful, so comingled raw stack dumps can yield
      # unsymbolizable garbage data.
      ids_txt_paths = BuildIdsPaths(package_paths)
      with _SymbolizeStream(process.stdout, ids_txt_paths) as \
              symbolized_stdout, \
           _SymbolizeStream(kernel_logger.stdout, ids_txt_paths) as \
               symbolized_klog:
        output_stream = MergedInputStream([symbolized_stdout.stdout,
                                           symbolized_klog.stdout]).Start()
        for next_line in output_stream:
          print(next_line.rstrip())
        symbolized_stdout.wait()  # Should return instantly.
        symbolized_klog.kill()    # klog is never-ending and must be killed.

      process.wait()
      if process.returncode == 0:
        logging.info('Process exited normally with status code 0.')
      else:
        # The test runner returns an error status code if *any* tests fail,
        # so we should proceed anyway.
        logging.warning('Process exited with status code %d.' %
                        process.returncode)

  finally:
    logging.info('Terminating kernel log reader.')
    log_output_quit_event.set()
    log_output_thread.join()
    kernel_logger.kill()

  return process.returncode
