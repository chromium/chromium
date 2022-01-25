#!/usr/bin/env vpython
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A helper tool for running Fuchsia's `ffx`.
"""

# Enable use of the print() built-in function.
from __future__ import print_function

import argparse
import contextlib
import errno
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile

import common
import log_manager


def get_ffx_path():
  """Returns the full path to `ffx`."""
  return os.path.join(common.SDK_ROOT, 'tools',
                      common.GetHostArchFromPlatform(), 'ffx')


class FfxRunner():
  """A helper to run `ffx` commands."""

  def __init__(self, log_manager):
    self._ffx = get_ffx_path()
    self._log_manager = log_manager

  def _run_repair_command(self, output):
    """Scans `output` for a self-repair command to run and, if found, runs it.

    Returns:
      True if a repair command was found and ran successfully. False otherwise.
    """
    # Check for a string along the lines of:
    # "Run `ffx doctor --restart-daemon` for further diagnostics."
    match = re.search('`ffx ([^`]+)`', output)
    if not match or len(match.groups()) != 1:
      return False  # No repair command found.
    try:
      self.run_ffx(match.groups()[0].split(), suppress_repair=True)
    except subprocess.CalledProcessError as cpe:
      return False  # Repair failed.
    return True  # Repair succeeded.

  def run_ffx(self, args, check=True, suppress_repair=False):
    """Runs `ffx` with the given arguments, waiting for it to exit.
    Args:
      args: A sequence of arguments to ffx.
      check: If True, CalledProcessError is raised if ffx returns a non-zero
        exit code.
      suppress_repair: If True, do not attempt to find and run a repair command.
    Returns:
      A string containing combined stdout and stderr.
    Raises:
      CalledProcessError if `check` is true.
    """
    log_file = self._log_manager.Open('ffx_log') \
      if self._log_manager.IsLoggingEnabled() else None
    command = [self._ffx]
    command.extend(args)
    logging.debug(command)
    if log_file:
      print(command, file=log_file)
    repair_succeeded = False
    try:
      # TODO(grt): Switch to subprocess.run() with encoding='utf-8' when p3 is
      # supported.
      process = subprocess.Popen(command,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
      stdoutdata = process.communicate()[0].decode('utf-8')
      if check and process.returncode != 0:
        raise subprocess.CalledProcessError(process.returncode, command,
                                            stdoutdata)
    except subprocess.CalledProcessError as cpe:
      if log_file:
        log_file.write('Process exited with code %d. Output: %s\n' %
                       (cpe.returncode, cpe.output.strip()))
      # Let the exception fly unless a repair command is found and succeeds.
      if suppress_repair or not self._run_repair_command(cpe.output):
        raise
      repair_succeeded = True

    # If the original command failed but a repair command was found and
    # succeeded, try one more time with the original command.
    if repair_succeeded:
      return self.run_ffx(args, check, suppress_repair=True)

    stripped_stdout = stdoutdata.strip()
    if log_file:
      if process.returncode != 0:
        log_file.write('Process exited with code %d.' % process.returncode)
        if stripped_stdout:
          log_file.write(' Output:\n%s\n' % stripped_stdout)
        else:
          log_file.write('\n')
      elif stripped_stdout:
        log_file.write('%s\n' % stripped_stdout)
    logging.debug(
        'ffx command returned %d with %s', process.returncode,
        ('output %s' % stripped_stdout if stripped_stdout else 'no output'))
    return stdoutdata

  def open_ffx(self, args):
    """Runs `ffx` with some arguments.
    Args:
      args: A sequence of arguments to ffx.
    Returns:
      A subprocess.Popen object.
    """
    log_file = self._log_manager.Open('ffx_log') \
      if self._log_manager.IsLoggingEnabled() else None
    command = [self._ffx]
    command.extend(args)
    logging.debug(command)
    if log_file:
      print(command, file=log_file)
    try:
      # TODO(grt): Add encoding='utf-8' when p3 is supported.
      return subprocess.Popen(command,
                              stdin=open(os.devnull, 'r'),
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
    except:
      logging.exception('Failed to open ffx')
      if log_file:
        print('Exception caught while opening ffx: %s' % str(sys.exc_info[1]))
      raise

  @contextlib.contextmanager
  def scoped_config(self, name, value):
    """Temporarily overrides `ffx` configuration.

    Args:
      name: The name of the property to set.
      value: The value to associate with `name`.

    Returns:
      Yields nothing. Restores the previous value upon exit.
    """
    assert value is not None
    # Cache the current value.
    old_value = None
    try:
      old_value = self.run_ffx(['config', 'get', name]).strip()
    except subprocess.CalledProcessError as cpe:
      if cpe.returncode != 2:
        raise  # The failure was for something other than value not found.
    # Set the new value if it is different.
    if value != old_value:
      self.run_ffx(['config', 'set', name, value])
    try:
      yield None
    finally:
      if value == old_value:
        return  # There is no need to restore an old value.
      # Clear the new value.
      self.run_ffx(['config', 'remove', name])
      if old_value is None:
        return
      # Did removing the new value restore the original value on account of it
      # either being the default or being set in a different scope?
      if (self.run_ffx(['config', 'get', name],
                       check=False).strip() == old_value):
        return
      # If not, explicitly set the original value.
      self.run_ffx(['config', 'set', name, old_value])

  def stop_daemon(self):
    """Stops the ffx daemon.

    If an initial attempt to stop it via `ffx daemon stop` fails,
    `ffx doctor --restart-daemon` is used to force a restart.
    """
    try:
      self.run_ffx(['daemon', 'stop'])
      return
    except subprocess.CalledProcessError:
      pass
    logging.error('Failed to stop the damon. Attempting to restart it via ffx' +
                  ' doctor')
    self.run_ffx(['doctor', '--restart-daemon'], check=False)

  def remove_stale_targets(self, address):
    """Removes any targets from ffx that are listening at a given address.

    Args:
      address: A string representation of the target's ip address.
    """
    json_targets = self.run_ffx(['target', 'list', '-f', 'j'])
    if not json_targets:
      return
    try:
      targets = json.loads(json_targets)
    except ValueError:
      # TODO(grt): Change to json.JSONDecodeError once p3 is supported.
      logging.debug('No stale targets to remove')
      return
    for target in targets:
      if target['rcs_state'] == 'N' and address in target['addresses']:
        self.run_ffx(['target', 'remove', address])

  @contextlib.contextmanager
  def scoped_target_context(self, address, port):
    """Temporarily adds a new target.

    Args:
      address: The IP address at which the target is listening.
      port: The port number on which the target is listening.

    Yields:
      An FfxTarget for interacting with the target.
    """
    address_and_port = '%s:%d' % (address, port)
    self.run_ffx(['target', 'add', address_and_port])
    try:
      yield FfxTarget(self, address=address, port=port)
    finally:
      self.run_ffx(['target', 'remove', address_and_port], check=False)


class FfxTarget():
  """A helper to run `ffx` commands for a specific target."""

  def __init__(self, ffx_runner, address=None, port=None, node_name=None):
    """Args:
      ffx_runner: The runner to use to run ffx.
      address: The IP address at which the target is listening.
      port: The port number on which the target is listening.
      node_name: The target's node name.
    """
    # Both or neither address and port must be specified.
    assert (address is not None) == (port is not None)
    # Either node_name or address+port must be specified.
    assert (node_name is not None) != (address is not None)
    self._ffx_runner = ffx_runner
    self._address = address
    self._port = port
    self._node_name = node_name
    target_identifier = node_name if (node_name is not None) else \
      ('%s:%d' % (self._address, self._port))
    self._target_args = ('--target', target_identifier)

  def wait(self, timeout=None):
    """Waits for ffx to establish a connection with the target.

    Args:
      timeout: The number of seconds to wait (60 if not specified).
    """
    command = list(self._target_args)
    command.extend(('target', 'wait'))
    if timeout is not None:
      command.extend(('-t', '%d' % int(timeout)))
    self._ffx_runner.run_ffx(command)

  def open_ffx(self, command):
    """Runs `ffx` for the target with some arguments.
    Args:
      command: A command and its arguments to run on the target.
    Returns:
      A subprocess.Popen object.
    """
    args = list(self._target_args)
    args.extend(command)
    return self._ffx_runner.open_ffx(args)


# TODO(grt): Derive from contextlib.AbstractContextManager when p3 is supported.
class FfxSession():
  """A context manager that manages a session for running a test via `ffx`.

  Upon entry, an instance of this class configures `ffx` to retrieve files
  generated by a test and prepares a directory to hold these files either in a
  LogManager's log directory or in tmp. On exit, any previous configuration of
  `ffx` is restored and the temporary directory, if used, is deleted.

  The prepared directory is used when invoking `ffx test run`.
  """

  def __init__(self, log_manager):
    """Args:
      log_manager: A Target's LogManager.
    """
    self._log_manager = log_manager
    self._ffx = FfxRunner(log_manager)
    self._structured_output_config = None
    self._own_output_dir = False
    self._output_dir = None

  def __enter__(self):
    # Enable experimental structured output for ffx.
    self._structured_output_config = self._ffx.scoped_config(
        'test.experimental_structured_output', 'true')
    self._structured_output_config.__enter__()
    if self._log_manager.IsLoggingEnabled():
      # Use a subdir of the configured log directory to hold test outputs.
      self._output_dir = os.path.join(self._log_manager.GetLogDirectory(),
                                      'test_outputs')
      # TODO(grt): Use exist_ok=True when p3 is supported.
      try:
        os.makedirs(self._output_dir)
      except OSError as ose:
        if ose.errno != errno.EEXIST:
          raise
    else:
      # Create a temporary directory to hold test outputs.
      # TODO(grt): Switch to tempfile.TemporaryDirectory when p3 is supported.
      self._own_output_dir = True
      self._output_dir = tempfile.mkdtemp(prefix='ffx_session_tmp_')
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    # Restore the previous test.output_path setting.
    if self._own_output_dir:
      # Clean up the temporary output directory.
      shutil.rmtree(self._output_dir, ignore_errors=True)
      self._own_output_dir = False
    self._output_dir = None
    # Restore the previous experimental structured output setting.
    self._structured_output_config.__exit__(exc_type, exc_val, exc_tb)
    self._structured_output_config = None
    return False

  def get_output_dir(self):
    """Returns the temporary output directory for the session."""
    assert self._output_dir, 'FfxSession must be used in a context'
    return self._output_dir

  def test_run(self, ffx_target, component_uri, package_args):
    """Runs a test on a target.
    Args:
      ffx_target: The target on which to run the test.
      component_uri: The test component URI.
      package_args: Arguments to the test package.
    Returns:
      A subprocess.Popen object.
    """
    command = [
        'test', 'run', '--output-directory', self._output_dir, component_uri,
        '--'
    ]
    command.extend(package_args)
    return ffx_target.open_ffx(command)


def make_arg_parser():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--logs-dir', help='Directory to write logs to.')
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      default=False,
                      help='Enable debug logging')
  return parser


def main(args):
  args = make_arg_parser().parse_args(args)

  logging.basicConfig(format='%(asctime)s:%(levelname)s:%(name)s:%(message)s',
                      level=logging.DEBUG if args.verbose else logging.INFO)
  log_mgr = log_manager.LogManager(args.logs_dir)

  with FfxSession(log_mgr) as ffx_session:
    logging.info(ffx_session.get_output_dir())

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
