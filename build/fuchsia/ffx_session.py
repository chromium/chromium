#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
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
import time

import common
import log_manager

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
from compatible_utils import parse_host_port

RUN_SUMMARY_SCHEMA = \
  'https://fuchsia.dev/schema/ffx_test/run_summary-8d1dd964.json'


def get_ffx_path():
  """Returns the full path to `ffx`."""
  return os.path.join(common.SDK_ROOT, 'tools',
                      common.GetHostArchFromPlatform(), 'ffx')


def format_host_port(host, port):
  """Formats a host name or IP address and port number into a host:port string.
  """
  # Wrap `host` in brackets if it looks like an IPv6 address
  return ('[%s]:%d' if ':' in host else '%s:%d') % (host, port)


class FfxRunner():
  """A helper to run `ffx` commands."""

  def __init__(self, log_manager):
    self._ffx = get_ffx_path()
    self._log_manager = log_manager

  def _get_daemon_status(self):
    """Determines daemon status via `ffx daemon socket`.

    Returns:
      dict of status of the socket. Status will have a key Running or
      NotRunning to indicate if the daemon is running.
    """
    status = json.loads(
        self.run_ffx(['--machine', 'json', 'daemon', 'socket'],
                     check=True,
                     suppress_repair=True))
    if status.get('pid') and status.get('pid', {}).get('status', {}):
      return status['pid']['status']
    return {'NotRunning': True}

  def _is_daemon_running(self):
    return 'Running' in self._get_daemon_status()

  def _wait_for_daemon(self, start=True, timeout_seconds=100):
    """Waits for daemon to reach desired state in a polling loop.

    Sleeps for 5s between polls.

    Args:
      start: bool. Indicates to wait for daemon to start up. If False,
        indicates waiting for daemon to die.
      timeout_seconds: int. Number of seconds to wait for the daemon to reach
        the desired status.
    Raises:
      TimeoutError: if the daemon does not reach the desired state in time.
    """
    wanted_status = 'start' if start else 'stop'
    sleep_period_seconds = 5
    attempts = int(timeout_seconds / sleep_period_seconds)
    for i in range(attempts):
      if self._is_daemon_running() == start:
        return
      if i != attempts:
        logging.info('Waiting for daemon to %s...', wanted_status)
        time.sleep(sleep_period_seconds)

    raise TimeoutError(f'Daemon did not {wanted_status} in time.')

  def _run_repair_command(self, output):
    """Scans `output` for a self-repair command to run and, if found, runs it.

    If logging is enabled, `ffx` is asked to emit its own logs to the log
    directory.

    Returns:
      True if a repair command was found and ran successfully. False otherwise.
    """
    # Check for a string along the lines of:
    # "Run `ffx doctor --restart-daemon` for further diagnostics."
    match = re.search('`ffx ([^`]+)`', output)
    if not match or len(match.groups()) != 1:
      return False  # No repair command found.
    args = match.groups()[0].split()
    # Tell ffx to include the configuration file without prompting in case
    # logging is enabled.
    with self.scoped_config('doctor.record_config', 'true'):
      # If the repair command is `ffx doctor` and logging is enabled, add the
      # options to emit ffx logs to the logging directory.
      if len(args) and args[0] == 'doctor' and \
         self._log_manager.IsLoggingEnabled():
        args.extend(
            ('--record', '--output-dir', self._log_manager.GetLogDirectory()))
      try:
        self.run_ffx(args, suppress_repair=True)
        self._wait_for_daemon(start=True)
      except subprocess.CalledProcessError as cpe:
        return False  # Repair failed.
      return True  # Repair succeeded.

  def run_ffx(self, args, check=True, suppress_repair=False):
    """Runs `ffx` with the given arguments, waiting for it to exit.

    If `ffx` exits with a non-zero exit code, the output is scanned for a
    recommended repair command (e.g., "Run `ffx doctor --restart-daemon` for
    further diagnostics."). If such a command is found, it is run and then the
    original command is retried. This behavior can be suppressed via the
    `suppress_repair` argument.

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
                                 stderr=subprocess.PIPE)
      stdout_data, stderr_data = process.communicate()
      stdout_data = stdout_data.decode('utf-8')
      stderr_data = stderr_data.decode('utf-8')
      if check and process.returncode != 0:
        # TODO(grt): Pass stdout and stderr as two args when p2 support is no
        # longer needed.
        raise subprocess.CalledProcessError(
            process.returncode, command, '\n'.join((stdout_data, stderr_data)))
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

    stripped_stdout = stdout_data.strip()
    stripped_stderr = stderr_data.strip()
    if log_file:
      if process.returncode != 0 or stripped_stderr:
        log_file.write('Process exited with code %d.' % process.returncode)
        if stripped_stderr:
          log_file.write(' Stderr:\n%s\n' % stripped_stderr)
        if stripped_stdout:
          log_file.write(' Stdout:\n%s\n' % stripped_stdout)
        if not stripped_stderr and not stripped_stdout:
          log_file.write('\n')
      elif stripped_stdout:
        log_file.write('%s\n' % stripped_stdout)
    logging.debug(
        'ffx command returned %d with %s%s', process.returncode,
        ('output "%s"' % stripped_stdout if stripped_stdout else 'no output'),
        (' and error "%s".' % stripped_stderr if stripped_stderr else '.'))
    return stdout_data

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

  def list_targets(self):
    """Returns the (possibly empty) list of targets known to ffx.

    Returns:
      The list of targets parsed from the JSON output of `ffx target list`.
    """
    json_targets = self.run_ffx(['target', 'list', '-f', 'json'])
    if not json_targets:
      return []
    try:
      return json.loads(json_targets)
    except ValueError:
      # TODO(grt): Change to json.JSONDecodeError once p3 is supported.
      return []

  def list_active_targets(self):
    """Gets the list of targets and filters down to the targets that are active.

    Returns:
      An iterator over active FfxTargets.
    """
    targets = [
        FfxTarget.from_target_list_json(self, json_target)
        for json_target in self.list_targets()
    ]
    return filter(lambda target: target.get_ssh_address(), targets)

  def remove_stale_targets(self, address):
    """Removes any targets from ffx that are listening at a given address.

    Args:
      address: A string representation of the target's ip address.
    """
    for target in self.list_targets():
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
    target_id = format_host_port(address, port)
    # -n allows `target add` to skip waiting for the device to come up,
    # as this can take longer than the default wait period.
    self.run_ffx(['target', 'add', '-n', target_id])
    try:
      yield FfxTarget.from_address(self, address, port)
    finally:
      self.run_ffx(['target', 'remove', target_id], check=False)

  def get_node_name(self, address, port):
    """Returns the node name for a target given its SSH address.

    Args:
      address: The address at which the target's SSH daemon is listening.
      port: The port number on which the daemon is listening.

    Returns:
      The target's node name.

    Raises:
      Exception: If the target cannot be found.
    """
    for target in self.list_targets():
      if target['nodename'] and address in target['addresses']:
        ssh_address = FfxTarget.from_target_list_json(target).get_ssh_address()
        if ssh_address and ssh_address[1] == port:
          return target['nodename']
    raise Exception('Failed to determine node name for target at %s' %
                    format_host_port(address, port))

  def daemon_stop(self):
    """Stops the ffx daemon."""
    self.run_ffx(['daemon', 'stop'], check=False, suppress_repair=True)
    # Daemon should stop at this point.
    self._wait_for_daemon(start=False)


class FfxTarget():
  """A helper to run `ffx` commands for a specific target."""

  @classmethod
  def from_address(cls, ffx_runner, address, port=None):
    """Args:
      ffx_runner: The runner to use to run ffx.
      address: The target's address.
      port: The target's port, defaults to None in which case it will target
            the first device at the specified address
    """
    return cls(ffx_runner, format_host_port(address, port) if port else address)

  @classmethod
  def from_node_name(cls, ffx_runner, node_name):
    """Args:
      ffx_runner: The runner to use to run ffx.
      node_name: The target's node name.
    """
    return cls(ffx_runner, node_name)

  @classmethod
  def from_target_list_json(cls, ffx_runner, json_target):
    """Args:
      ffx_runner: The runner to use to run ffx.
      json_target: the json dict as returned from `ffx list targets`
    """
    # Targets seen via `fx serve-remote` frequently have no name, so fall back
    # to using the first address.
    if json_target['nodename'].startswith('<unknown'):
      return cls.from_address(ffx_runner, json_target['addresses'][0])
    return cls.from_node_name(ffx_runner, json_target['nodename'])

  def __init__(self, ffx_runner, target_id):
    """Args:
      ffx_runner: The runner to use to run ffx.
      target_id: The target's node name or addr:port string.
    """
    self._ffx_runner = ffx_runner
    self._target_id = target_id
    self._target_args = ('--target', target_id)

  def format_runner_options(self):
    """Returns a string holding options suitable for use with the runner scripts
    to run tests on this target."""
    try:
      # First try extracting host:port from the target_id.
      return '-d --host %s --port %d' % parse_host_port(self._target_args[1])
    except ValueError:
      # Must be a simple node name.
      pass
    return '-d --node-name %s' % self._target_args[1]

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

  def get_ssh_address(self):
    """Returns the host and port of the target's SSH address

    Returns:
      A tuple of a host address string and a port number integer,
        or None if there was an exception
    """
    command = list(self._target_args)
    command.extend(('target', 'get-ssh-address'))
    try:
      return parse_host_port(self._ffx_runner.run_ffx(command))
    except:
      return None

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

  def __str__(self):
    return self._target_id

  def __repr__(self):
    return self._target_id


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
    self._own_output_dir = False
    self._output_dir = None
    self._run_summary = None
    self._suite_summary = None
    self._custom_artifact_directory = None
    self._debug_data_directory = None

  def __enter__(self):
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
        '--config', 'test.experimental_structured_output=false', 'test', 'run',
        '--output-directory', self._output_dir, component_uri, '--'
    ]
    command.extend(package_args)
    return ffx_target.open_ffx(command)

  def _parse_test_outputs(self):
    """Parses the output files generated by the test runner.

    The instance's `_custom_artifact_directory` member is set to the directory
    holding output files emitted by the test.

    This function is idempotent, and performs no work if it has already been
    called.
    """
    if self._run_summary:
      return  # The files have already been parsed.

    # Parse the main run_summary.json file.
    run_summary_path = os.path.join(self._output_dir, 'run_summary.json')
    try:
      with open(run_summary_path) as run_summary_file:
        self._run_summary = json.load(run_summary_file)
    except IOError as io_error:
      logging.error('Error reading run summary file: %s', str(io_error))
      return
    except ValueError as value_error:
      logging.error('Error parsing run summary file %s: %s', run_summary_path,
                    str(value_error))
      return

    assert self._run_summary['schema_id'] == RUN_SUMMARY_SCHEMA, \
      'Unsupported version found in %s' % run_summary_path

    run_artifact_dir = self._run_summary.get('data', {})['artifact_dir']
    for artifact_path, artifact in self._run_summary.get(
        'data', {})['artifacts'].items():
      if artifact['artifact_type'] == 'DEBUG':
        self._debug_data_directory = os.path.join(self._output_dir,
                                                  run_artifact_dir,
                                                  artifact_path)
        break

    # There should be precisely one suite for the test that ran.
    self._suite_summary = self._run_summary.get('data', {}).get('suites',
                                                                [{}])[0]

    # Get the top-level directory holding all artifacts for this suite.
    artifact_dir = self._suite_summary.get('artifact_dir')
    if not artifact_dir:
      logging.error('Failed to find suite\'s artifact_dir in %s',
                    run_summary_path)
      return

    # Get the path corresponding to artifacts
    for artifact_path, artifact in self._suite_summary['artifacts'].items():
      if artifact['artifact_type'] == 'CUSTOM':
        self._custom_artifact_directory = os.path.join(self._output_dir,
                                                       artifact_dir,
                                                       artifact_path)
        break

  def get_custom_artifact_directory(self):
    """Returns the full path to the directory holding custom artifacts emitted
    by the test, or None if the path cannot be determined.
    """
    self._parse_test_outputs()
    return self._custom_artifact_directory

  def get_debug_data_directory(self):
    """Returns the full path to the directory holding custom artifacts emitted
    by the test, or None if the path cannot be determined.
    """
    self._parse_test_outputs()
    return self._debug_data_directory


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
