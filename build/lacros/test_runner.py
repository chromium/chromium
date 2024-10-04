#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script facilitates running tests for lacros on Linux.

  In order to run lacros tests on Linux, please first follow bit.ly/3juQVNJ
  to setup build directory with the lacros-chrome-on-linux build configuration,
  and corresponding test targets are built successfully.

Example usages

  ./build/lacros/test_runner.py test out/lacros/url_unittests
  ./build/lacros/test_runner.py test out/lacros/browser_tests

  The commands above run url_unittests and browser_tests respectively, and more
  specifically, url_unitests is executed directly while browser_tests is
  executed with the latest version of prebuilt ash-chrome, and the behavior is
  controlled by |_TARGETS_REQUIRE_ASH_CHROME|, and it's worth noting that the
  list is maintained manually, so if you see something is wrong, please upload a
  CL to fix it.

  ./build/lacros/test_runner.py test out/lacros/browser_tests \\
      --gtest_filter=BrowserTest.Title

  The above command only runs 'BrowserTest.Title', and any argument accepted by
  the underlying test binary can be specified in the command.

  ./build/lacros/test_runner.py test out/lacros/browser_tests \\
    --ash-chrome-version=120.0.6099.0

  The above command runs tests with a given version of ash-chrome, which is
  useful to reproduce test failures. A list of prebuilt versions can
  be found at:
  https://chrome-infra-packages.appspot.com/p/chromium/testing/linux-ash-chromium/x86_64/ash.zip
  Click on any instance, you should see the version number for that instance.
  Also, there are refs, which points to the instance for that channel. It should
  be close the prod version but no guarantee.
  For legacy refs, like legacy119, it point to the latest version for that
  milestone.

  ./testing/xvfb.py ./build/lacros/test_runner.py test out/lacros/browser_tests

  The above command starts ash-chrome with xvfb instead of an X11 window, and
  it's useful when running tests without a display attached, such as sshing.

  For version skew testing when passing --ash-chrome-path-override, the runner
  will try to find the ash major version and Lacros major version. If ash is
  newer(major version larger), the runner will not run any tests and just
  returns success.

Interactively debugging tests

  Any of the previous examples accept the switches
    --gdb
    --lldb
  to run the tests in the corresponding debugger.
"""

import argparse
import json
import os
import logging
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import zipfile

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir))
sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'depot_tools'))


# The cipd path for prebuilt ash chrome.
_ASH_CIPD_PATH = 'chromium/testing/linux-ash-chromium/x86_64/ash.zip'


# Directory to cache downloaded ash-chrome versions to avoid re-downloading.
_PREBUILT_ASH_CHROME_DIR = os.path.join(os.path.dirname(__file__),
                                        'prebuilt_ash_chrome')

# File path to the asan symbolizer executable.
_ASAN_SYMBOLIZER_PATH = os.path.join(_SRC_ROOT, 'tools', 'valgrind', 'asan',
                                     'asan_symbolize.py')

# Number of seconds to wait for ash-chrome to start.
ASH_CHROME_TIMEOUT_SECONDS = (
    300 if os.environ.get('ASH_WRAPPER', None) else 25)

# List of targets that require ash-chrome as a Wayland server in order to run.
_TARGETS_REQUIRE_ASH_CHROME = [
    'app_shell_unittests',
    'aura_unittests',
    'browser_tests',
    'components_unittests',
    'compositor_unittests',
    'content_unittests',
    'dbus_unittests',
    'extensions_unittests',
    'media_unittests',
    'message_center_unittests',
    'snapshot_unittests',
    'sync_integration_tests',
    'unit_tests',
    'views_unittests',
    'wm_unittests',

    # regex patterns.
    '.*_browsertests',
    '.*interactive_ui_tests'
]

# List of targets that require ash-chrome to support crosapi mojo APIs.
_TARGETS_REQUIRE_MOJO_CROSAPI = [
    # TODO(jamescook): Add 'browser_tests' after multiple crosapi connections
    # are allowed. For now we only enable crosapi in targets that run tests
    # serially.
    'interactive_ui_tests',
]

# Default test filter file for each target. These filter files will be
# used by default if no other filter file get specified.
_DEFAULT_FILTER_FILES_MAPPING = {
    'browser_tests': 'linux-lacros.browser_tests.filter',
    'components_unittests': 'linux-lacros.components_unittests.filter',
    'content_browsertests': 'linux-lacros.content_browsertests.filter',
    'interactive_ui_tests': 'linux-lacros.interactive_ui_tests.filter',
    'sync_integration_tests': 'linux-lacros.sync_integration_tests.filter',
    'unit_tests': 'linux-lacros.unit_tests.filter',
}


def _GetAshChromeDirPath(version):
  """Returns a path to the dir storing the downloaded version of ash-chrome."""
  return os.path.join(_PREBUILT_ASH_CHROME_DIR, version)


def _remove_unused_ash_chrome_versions(version_to_skip):
  """Removes unused ash-chrome versions to save disk space.

  Currently, when an ash-chrome zip is downloaded and unpacked, the atime/mtime
  of the dir and the files are NOW instead of the time when they were built, but
  there is no garanteen it will always be the behavior in the future, so avoid
  removing the current version just in case.

  Args:
    version_to_skip (str): the version to skip removing regardless of its age.
  """
  days = 7
  expiration_duration = 60 * 60 * 24 * days

  for f in os.listdir(_PREBUILT_ASH_CHROME_DIR):
    if f == version_to_skip:
      continue

    p = os.path.join(_PREBUILT_ASH_CHROME_DIR, f)
    if os.path.isfile(p):
      # The prebuilt ash-chrome dir is NOT supposed to contain any files, remove
      # them to keep the directory clean.
      os.remove(p)
      continue
    chrome_path = os.path.join(p, 'test_ash_chrome')
    if not os.path.exists(chrome_path):
      chrome_path = p
    age = time.time() - os.path.getatime(chrome_path)
    if age > expiration_duration:
      logging.info(
          'Removing ash-chrome: "%s" as it hasn\'t been used in the '
          'past %d days', p, days)
      shutil.rmtree(p)


def _GetLatestVersionOfAshChrome():
  '''Get the latest ash chrome version.

  Get the package version info with canary ref.

  Returns:
    A string with the chrome version.

  Raises:
    RuntimeError: if we can not get the version.
  '''
  cp = subprocess.run(
      ['cipd', 'describe', _ASH_CIPD_PATH, '-version', 'canary'],
      capture_output=True)
  assert (cp.returncode == 0)
  groups = re.search(r'version:(?P<version>[\d\.]+)', str(cp.stdout))
  if not groups:
    raise RuntimeError('Can not find the version. Error message: %s' %
                       cp.stdout)
  return groups.group('version')


def _DownloadAshChromeFromCipd(path, version):
  '''Download the ash chrome with the requested version.

  Args:
    path: string for the downloaded ash chrome folder.
    version: string for the ash chrome version.

  Returns:
    A string representing the path for the downloaded ash chrome.
  '''
  with tempfile.TemporaryDirectory() as temp_dir:
    ensure_file_path = os.path.join(temp_dir, 'ensure_file.txt')
    f = open(ensure_file_path, 'w+')
    f.write(_ASH_CIPD_PATH + ' version:' + version)
    f.close()
    subprocess.run(
        ['cipd', 'ensure', '-ensure-file', ensure_file_path, '-root', path])


def _DoubleCheckDownloadedAshChrome(path, version):
  '''Check the downloaded ash is the expected version.

  Double check by running the chrome binary with --version.

  Args:
    path: string for the downloaded ash chrome folder.
    version: string for the expected ash chrome version.

  Raises:
    RuntimeError if no test_ash_chrome binary can be found.
  '''
  test_ash_chrome = os.path.join(path, 'test_ash_chrome')
  if not os.path.exists(test_ash_chrome):
    raise RuntimeError('Can not find test_ash_chrome binary under %s' % path)
  cp = subprocess.run([test_ash_chrome, '--version'], capture_output=True)
  assert (cp.returncode == 0)
  if str(cp.stdout).find(version) == -1:
    logging.warning(
        'The downloaded ash chrome version is %s, but the '
        'expected ash chrome is %s. There is a version mismatch. Please '
        'file a bug to OS>Lacros so someone can take a look.' %
        (cp.stdout, version))


def _DownloadAshChromeIfNecessary(version):
  """Download a given version of ash-chrome if not already exists.

  Args:
    version: A string representing the version, such as "793554".

  Raises:
      RuntimeError: If failed to download the specified version, for example,
          if the version is not present on gcs.
  """

  def IsAshChromeDirValid(ash_chrome_dir):
    # This function assumes that once 'chrome' is present, other dependencies
    # will be present as well, it's not always true, for example, if the test
    # runner process gets killed in the middle of unzipping (~2 seconds), but
    # it's unlikely for the assumption to break in practice.
    return os.path.isdir(ash_chrome_dir) and os.path.isfile(
        os.path.join(ash_chrome_dir, 'test_ash_chrome'))

  ash_chrome_dir = _GetAshChromeDirPath(version)
  if IsAshChromeDirValid(ash_chrome_dir):
    return

  shutil.rmtree(ash_chrome_dir, ignore_errors=True)
  os.makedirs(ash_chrome_dir)
  _DownloadAshChromeFromCipd(ash_chrome_dir, version)
  _DoubleCheckDownloadedAshChrome(ash_chrome_dir, version)
  _remove_unused_ash_chrome_versions(version)


def _WaitForAshChromeToStart(tmp_xdg_dir, lacros_mojo_socket_file,
                             enable_mojo_crosapi, ash_ready_file):
  """Waits for Ash-Chrome to be up and running and returns a boolean indicator.

  Determine whether ash-chrome is up and running by checking whether two files
  (lock file + socket) have been created in the |XDG_RUNTIME_DIR| and the lacros
  mojo socket file has been created if enabling the mojo "crosapi" interface.
  TODO(crbug.com/40707216): Figure out a more reliable hook to determine the
  status of ash-chrome, likely through mojo connection.

  Args:
    tmp_xdg_dir (str): Path to the XDG_RUNTIME_DIR.
    lacros_mojo_socket_file (str): Path to the lacros mojo socket file.
    enable_mojo_crosapi (bool): Whether to bootstrap the crosapi mojo interface
        between ash and the lacros test binary.
    ash_ready_file (str): Path to a non-existing file. After ash is ready for
        testing, the file will be created.

  Returns:
    A boolean indicating whether Ash-chrome is up and running.
  """

  def IsAshChromeReady(tmp_xdg_dir, lacros_mojo_socket_file,
                       enable_mojo_crosapi, ash_ready_file):
    # There should be 2 wayland files.
    if len(os.listdir(tmp_xdg_dir)) < 2:
      return False
    if enable_mojo_crosapi and not os.path.exists(lacros_mojo_socket_file):
      return False
    return os.path.exists(ash_ready_file)

  time_counter = 0
  while not IsAshChromeReady(tmp_xdg_dir, lacros_mojo_socket_file,
                             enable_mojo_crosapi, ash_ready_file):
    time.sleep(0.5)
    time_counter += 0.5
    if time_counter > ASH_CHROME_TIMEOUT_SECONDS:
      break

  return IsAshChromeReady(tmp_xdg_dir, lacros_mojo_socket_file,
                          enable_mojo_crosapi, ash_ready_file)


def _ExtractAshMajorVersion(file_path):
  """Extract major version from file_path.

  File path like this:
  ../../lacros_version_skew_tests_v94.0.4588.0/test_ash_chrome

  Returns:
    int representing the major version. Or 0 if it can't extract
        major version.
  """
  m = re.search(
      'lacros_version_skew_tests_v(?P<version>[0-9]+).[0-9]+.[0-9]+.[0-9]+/',
      file_path)
  if (m and 'version' in m.groupdict().keys()):
    return int(m.group('version'))
  logging.warning('Can not find the ash version in %s.' % file_path)
  # Returns ash major version as 0, so we can still run tests.
  # This is likely happen because user is running in local environments.
  return 0


def _FindLacrosMajorVersionFromMetadata():
  # This handles the logic on bots. When running on bots,
  # we don't copy source files to test machines. So we build a
  # metadata.json file which contains version information.
  if not os.path.exists('metadata.json'):
    logging.error('Can not determine current version.')
    # Returns 0 so it can't run any tests.
    return 0
  version = ''
  with open('metadata.json', 'r') as file:
    content = json.load(file)
    version = content['content']['version']
  return int(version[:version.find('.')])


def _FindLacrosMajorVersion():
  """Returns the major version in the current checkout.

  It would try to read src/chrome/VERSION. If it's not available,
  then try to read metadata.json.

  Returns:
    int representing the major version. Or 0 if it fails to
    determine the version.
  """
  version_file = os.path.abspath(
      os.path.join(os.path.abspath(os.path.dirname(__file__)),
                   '../../chrome/VERSION'))
  # This is mostly happens for local development where
  # src/chrome/VERSION exists.
  if os.path.exists(version_file):
    lines = open(version_file, 'r').readlines()
    return int(lines[0][lines[0].find('=') + 1:-1])
  return _FindLacrosMajorVersionFromMetadata()


def _ParseSummaryOutput(forward_args):
  """Find the summary output file path.

  Args:
    forward_args (list): Args to be forwarded to the test command.

  Returns:
    None if not found, or str representing the output file path.
  """
  logging.warning(forward_args)
  for arg in forward_args:
    if arg.startswith('--test-launcher-summary-output='):
      return arg[len('--test-launcher-summary-output='):]
  return None


def _IsRunningOnBots(forward_args):
  """Detects if the script is running on bots or not.

  Args:
    forward_args (list): Args to be forwarded to the test command.

  Returns:
    True if the script is running on bots. Otherwise returns False.
  """
  return '--test-launcher-bot-mode' in forward_args


def _KillNicely(proc, timeout_secs=2, first_wait_secs=0):
  """Kills a subprocess nicely.

  Args:
    proc: The subprocess to kill.
    timeout_secs: The timeout to wait in seconds.
    first_wait_secs: The grace period before sending first SIGTERM in seconds.
  """
  if not proc:
    return

  if first_wait_secs:
    try:
      proc.wait(first_wait_secs)
      return
    except subprocess.TimeoutExpired:
      pass

  if proc.poll() is None:
    proc.terminate()
    try:
      proc.wait(timeout_secs)
    except subprocess.TimeoutExpired:
      proc.kill()
      proc.wait()


def _ClearDir(dirpath):
  """Deletes everything within the directory.

  Args:
    dirpath: The path of the directory.
  """
  for e in os.scandir(dirpath):
    if e.is_dir():
      shutil.rmtree(e.path, ignore_errors=True)
    elif e.is_file():
      os.remove(e.path)


def _LaunchDebugger(args, forward_args, test_env):
  """Launches the requested debugger.

  This is used to wrap the test invocation in a debugger. It returns the
  created Popen class of the debugger process.

  Args:
      args (dict): Args for this script.
      forward_args (list): Args to be forwarded to the test command.
      test_env (dict): Computed environment variables for the test.
  """
  logging.info('Starting debugger.')

  # Redirect fatal signals to "ignore." When running an interactive debugger,
  # these signals should go only to the debugger so the user can break back out
  # of the debugged test process into the debugger UI without killing this
  # parent script.
  for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, signal.SIG_IGN)

  # Force the tests into single-process-test mode for debugging unless manually
  # specified. Otherwise the tests will run in a child process that the debugger
  # won't be attached to and the debugger won't do anything.
  if not ("--single-process" in forward_args
          or "--single-process-tests" in forward_args):
    forward_args += ["--single-process-tests"]

    # Adding --single-process-tests can cause some tests to fail when they're
    # run in the same process. Forcing the user to specify a filter will prevent
    # a later error.
    if not [i for i in forward_args if i.startswith("--gtest_filter")]:
      logging.error("""Interactive debugging requested without --gtest_filter

This script adds --single-process-tests to support interactive debugging but
some tests will fail in this mode unless run independently. To debug a test
specify a --gtest_filter=Foo.Bar to name the test you want to debug.
""")
      sys.exit(1)

  # This code attempts to source the debugger configuration file. Some
  # users will have this in their init but sourcing it more than once is
  # harmless and helps people that haven't configured it.
  if args.gdb:
    gdbinit_file = os.path.normpath(
        os.path.join(os.path.realpath(__file__), "../../../tools/gdb/gdbinit"))
    debugger_command = [
        'gdb', '--init-eval-command', 'source ' + gdbinit_file, '--args'
    ]
  else:
    lldbinit_dir = os.path.normpath(
        os.path.join(os.path.realpath(__file__), "../../../tools/lldb"))
    debugger_command = [
        'lldb', '-O',
        "script sys.path[:0] = ['%s']" % lldbinit_dir, '-O',
        'script import lldbinit', '--'
    ]
  debugger_command += [args.command] + forward_args
  return subprocess.Popen(debugger_command, env=test_env)


def _RunTestWithAshChrome(args, forward_args):
  """Runs tests with ash-chrome.

  Args:
    args (dict): Args for this script.
    forward_args (list): Args to be forwarded to the test command.
  """
  if args.ash_chrome_path_override:
    ash_chrome_file = args.ash_chrome_path_override
    ash_major_version = _ExtractAshMajorVersion(ash_chrome_file)
    lacros_major_version = _FindLacrosMajorVersion()
    if ash_major_version > lacros_major_version:
      logging.warning('''Not running any tests, because we do not \
support version skew testing for Lacros M%s against ash M%s''' %
                      (lacros_major_version, ash_major_version))
      # Create an empty output.json file so result adapter can read
      # the file. Or else result adapter will report no file found
      # and result infra failure.
      output_json = _ParseSummaryOutput(forward_args)
      if output_json:
        with open(output_json, 'w') as f:
          f.write("""{"all_tests":[],"disabled_tests":[],"global_tags":[],
"per_iteration_data":[],"test_locations":{}}""")
      # Although we don't run any tests, this is considered as success.
      return 0
    if not os.path.exists(ash_chrome_file):
      logging.error("""Can not find ash chrome at %s. Did you download \
the ash from CIPD? If you don't plan to build your own ash, you need \
to download first. Example commandlines:
 $ cipd auth-login
 $ echo "chromium/testing/linux-ash-chromium/x86_64/ash.zip \
version:92.0.4515.130" > /tmp/ensure-file.txt
 $ cipd ensure -ensure-file /tmp/ensure-file.txt \
-root lacros_version_skew_tests_v92.0.4515.130
 Then you can use --ash-chrome-path-override=\
lacros_version_skew_tests_v92.0.4515.130/test_ash_chrome
""" % ash_chrome_file)
      return 1
  elif args.ash_chrome_path:
    ash_chrome_file = args.ash_chrome_path
  else:
    ash_chrome_version = (args.ash_chrome_version
                          or _GetLatestVersionOfAshChrome())
    _DownloadAshChromeIfNecessary(ash_chrome_version)
    logging.info('Ash-chrome version: %s', ash_chrome_version)

    ash_chrome_file = os.path.join(_GetAshChromeDirPath(ash_chrome_version),
                                   'test_ash_chrome')
  try:
    # Starts Ash-Chrome.
    tmp_xdg_dir_name = tempfile.mkdtemp()
    tmp_ash_data_dir_name = tempfile.mkdtemp()
    tmp_unique_ash_dir_name = tempfile.mkdtemp()

    # Please refer to below file for how mojo connection is set up in testing.
    # //chrome/browser/ash/crosapi/test_mojo_connection_manager.h
    lacros_mojo_socket_file = '%s/lacros.sock' % tmp_ash_data_dir_name
    lacros_mojo_socket_arg = ('--lacros-mojo-socket-for-testing=%s' %
                              lacros_mojo_socket_file)
    ash_ready_file = '%s/ash_ready.txt' % tmp_ash_data_dir_name
    enable_mojo_crosapi = any(t == os.path.basename(args.command)
                              for t in _TARGETS_REQUIRE_MOJO_CROSAPI)
    ash_wayland_socket_name = 'wayland-exo'

    ash_process = None
    ash_env = os.environ.copy()
    ash_env['XDG_RUNTIME_DIR'] = tmp_xdg_dir_name
    ash_cmd = [
        ash_chrome_file,
        '--user-data-dir=%s' % tmp_ash_data_dir_name,
        '--enable-wayland-server',
        '--no-startup-window',
        '--disable-input-event-activation-protection',
        '--disable-lacros-keep-alive',
        '--disable-login-lacros-opening',
        '--enable-field-trial-config',
        '--enable-logging=stderr',
        '--enable-features=LacrosSupport,LacrosPrimary,LacrosOnly',
        '--enable-lacros-for-testing',
        '--ash-ready-file-path=%s' % ash_ready_file,
        '--wayland-server-socket=%s' % ash_wayland_socket_name,
    ]
    if '--enable-pixel-output-in-tests' not in forward_args:
      ash_cmd.append('--disable-gl-drawing-for-tests')

    if enable_mojo_crosapi:
      ash_cmd.append(lacros_mojo_socket_arg)

    # Users can specify a wrapper for the ash binary to do things like
    # attaching debuggers. For example, this will open a new terminal window
    # and run GDB.
    #   $ export ASH_WRAPPER="gnome-terminal -- gdb --ex=r --args"
    ash_wrapper = os.environ.get('ASH_WRAPPER', None)
    if ash_wrapper:
      logging.info('Running ash with "ASH_WRAPPER": %s', ash_wrapper)
      ash_cmd = list(ash_wrapper.split()) + ash_cmd

    ash_process = None
    ash_process_has_started = False
    total_tries = 3
    num_tries = 0
    ash_start_time = None

    # Create a log file if the user wanted to have one.
    ash_log = None
    ash_log_path = None

    run_tests_in_debugger = args.gdb or args.lldb

    if args.ash_logging_path:
      ash_log_path = args.ash_logging_path
    # Put ash logs in a separate file on bots.
    # For asan builds, the ash log is not symbolized. In order to
    # read the stack strace, we don't redirect logs to another file.
    elif _IsRunningOnBots(forward_args) and not args.combine_ash_logs_on_bots:
      summary_file = _ParseSummaryOutput(forward_args)
      if summary_file:
        ash_log_path = os.path.join(os.path.dirname(summary_file),
                                    'ash_chrome.log')
    elif run_tests_in_debugger:
      # The debugger is unusable when all Ash logs are getting dumped to the
      # same terminal. Redirect to a log file if there isn't one specified.
      logging.info("Running in the debugger and --ash-logging-path is not " +
                   "specified, defaulting to the current directory.")
      ash_log_path = 'ash_chrome.log'

    if ash_log_path:
      ash_log = open(ash_log_path, 'a')
      logging.info('Writing ash-chrome logs to: %s', ash_log_path)

    ash_stdout = ash_log or None
    test_stdout = None

    # Setup asan symbolizer.
    ash_symbolize_process = None
    test_symbolize_process = None
    should_symbolize = False
    if args.asan_symbolize_output and os.path.exists(_ASAN_SYMBOLIZER_PATH):
      should_symbolize = True
      ash_symbolize_stdout = ash_stdout
      ash_stdout = subprocess.PIPE
      test_stdout = subprocess.PIPE

    while not ash_process_has_started and num_tries < total_tries:
      num_tries += 1
      ash_start_time = time.monotonic()
      logging.info('Starting ash-chrome: ' + ' '.join(ash_cmd))

      # Using preexec_fn=os.setpgrp here will detach the forked process from
      # this process group before exec-ing Ash. This prevents interactive
      # Control-C from being seen by Ash. Otherwise Control-C in a debugger
      # can kill Ash out from under the debugger. In non-debugger cases, this
      # script attempts to clean up the spawned processes nicely.
      ash_process = subprocess.Popen(ash_cmd,
                                     env=ash_env,
                                     preexec_fn=os.setpgrp,
                                     stdout=ash_stdout,
                                     stderr=subprocess.STDOUT)

      if should_symbolize:
        logging.info('Symbolizing ash logs with asan symbolizer.')
        ash_symbolize_process = subprocess.Popen([_ASAN_SYMBOLIZER_PATH],
                                                 stdin=ash_process.stdout,
                                                 preexec_fn=os.setpgrp,
                                                 stdout=ash_symbolize_stdout,
                                                 stderr=subprocess.STDOUT)
        # Allow ash_process to receive a SIGPIPE if symbolize process exits.
        ash_process.stdout.close()

      ash_process_has_started = _WaitForAshChromeToStart(
          tmp_xdg_dir_name, lacros_mojo_socket_file, enable_mojo_crosapi,
          ash_ready_file)
      if ash_process_has_started:
        break

      logging.warning('Starting ash-chrome timed out after %ds',
                      ASH_CHROME_TIMEOUT_SECONDS)
      logging.warning('Are you using test_ash_chrome?')
      logging.warning('Printing the output of "ps aux" for debugging:')
      subprocess.call(['ps', 'aux'])
      _KillNicely(ash_process)
      _KillNicely(ash_symbolize_process, first_wait_secs=1)

      # Clean up for retry.
      _ClearDir(tmp_xdg_dir_name)
      _ClearDir(tmp_ash_data_dir_name)

    if not ash_process_has_started:
      raise RuntimeError('Timed out waiting for ash-chrome to start')

    ash_elapsed_time = time.monotonic() - ash_start_time
    logging.info('Started ash-chrome in %.3fs on try %d.', ash_elapsed_time,
                 num_tries)

    # Starts tests.
    if enable_mojo_crosapi:
      forward_args.append(lacros_mojo_socket_arg)

    forward_args.append('--ash-chrome-path=' + ash_chrome_file)
    forward_args.append('--unique-ash-dir=' + tmp_unique_ash_dir_name)

    test_env = os.environ.copy()
    test_env['WAYLAND_DISPLAY'] = ash_wayland_socket_name
    test_env['EGL_PLATFORM'] = 'surfaceless'
    test_env['XDG_RUNTIME_DIR'] = tmp_xdg_dir_name

    if run_tests_in_debugger:
      test_process = _LaunchDebugger(args, forward_args, test_env)
    else:
      logging.info('Starting test process.')
      test_process = subprocess.Popen([args.command] + forward_args,
                                      env=test_env,
                                      stdout=test_stdout,
                                      stderr=subprocess.STDOUT)
      if should_symbolize:
        logging.info('Symbolizing test logs with asan symbolizer.')
        test_symbolize_process = subprocess.Popen([_ASAN_SYMBOLIZER_PATH],
                                                  stdin=test_process.stdout)
        # Allow test_process to receive a SIGPIPE if symbolize process exits.
        test_process.stdout.close()
    return test_process.wait()

  finally:
    _KillNicely(ash_process)
    # Give symbolizer processes time to finish writing with first_wait_secs.
    _KillNicely(ash_symbolize_process, first_wait_secs=1)
    _KillNicely(test_symbolize_process, first_wait_secs=1)

    shutil.rmtree(tmp_xdg_dir_name, ignore_errors=True)
    shutil.rmtree(tmp_ash_data_dir_name, ignore_errors=True)
    shutil.rmtree(tmp_unique_ash_dir_name, ignore_errors=True)


def _RunTestDirectly(args, forward_args):
  """Runs tests by invoking the test command directly.

  args (dict): Args for this script.
  forward_args (list): Args to be forwarded to the test command.
  """
  try:
    p = None
    p = subprocess.Popen([args.command] + forward_args)
    return p.wait()
  finally:
    _KillNicely(p)


def _HandleSignal(sig, _):
  """Handles received signals to make sure spawned test process are killed.

  sig (int): An integer representing the received signal, for example SIGTERM.
  """
  logging.warning('Received signal: %d, killing spawned processes', sig)

  # Don't do any cleanup here, instead, leave it to the finally blocks.
  # Assumption is based on https://docs.python.org/3/library/sys.html#sys.exit:
  # cleanup actions specified by finally clauses of try statements are honored.

  # https://tldp.org/LDP/abs/html/exitcodes.html:
  # Exit code 128+n -> Fatal error signal "n".
  sys.exit(128 + sig)


def _ExpandFilterFileIfNeeded(test_target, forward_args):
  if (test_target in _DEFAULT_FILTER_FILES_MAPPING.keys() and not any(
      [arg.startswith('--test-launcher-filter-file') for arg in forward_args])):
    file_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), '..', '..', 'testing',
                     'buildbot', 'filters',
                     _DEFAULT_FILTER_FILES_MAPPING[test_target]))
    forward_args.append(f'--test-launcher-filter-file={file_path}')


def _RunTest(args, forward_args):
  """Runs tests with given args.

  args (dict): Args for this script.
  forward_args (list): Args to be forwarded to the test command.

  Raises:
      RuntimeError: If the given test binary doesn't exist or the test runner
          doesn't know how to run it.
  """

  if not os.path.isfile(args.command):
    raise RuntimeError('Specified test command: "%s" doesn\'t exist' %
                       args.command)

  test_target = os.path.basename(args.command)
  _ExpandFilterFileIfNeeded(test_target, forward_args)

  # |_TARGETS_REQUIRE_ASH_CHROME| may not always be accurate as it is updated
  # with a best effort only, therefore, allow the invoker to override the
  # behavior with a specified ash-chrome version, which makes sure that
  # automated CI/CQ builders would always work correctly.
  requires_ash_chrome = any(
      re.match(t, test_target) for t in _TARGETS_REQUIRE_ASH_CHROME)
  if not requires_ash_chrome and not args.ash_chrome_version:
    return _RunTestDirectly(args, forward_args)

  return _RunTestWithAshChrome(args, forward_args)


def Main():
  for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, _HandleSignal)

  logging.basicConfig(level=logging.INFO)
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  subparsers = arg_parser.add_subparsers()

  test_parser = subparsers.add_parser('test', help='Run tests')
  test_parser.set_defaults(func=_RunTest)

  test_parser.add_argument(
      'command',
      help='A single command to invoke the tests, for example: '
      '"./url_unittests". Any argument unknown to this test runner script will '
      'be forwarded to the command, for example: "--gtest_filter=Suite.Test"')

  version_group = test_parser.add_mutually_exclusive_group()
  version_group.add_argument(
      '--ash-chrome-version',
      type=str,
      help='Version of an prebuilt ash-chrome to use for testing, for example: '
      '"120.0.6099.0", and the version corresponds to the commit position of '
      'commits on the main branch. If not specified, will use the latest '
      'version available')
  version_group.add_argument(
      '--ash-chrome-path',
      type=str,
      help='Path to an locally built ash-chrome to use for testing. '
      'In general you should build //chrome/test:test_ash_chrome.')

  debugger_group = test_parser.add_mutually_exclusive_group()
  debugger_group.add_argument('--gdb',
                              action='store_true',
                              help='Run the test in GDB.')
  debugger_group.add_argument('--lldb',
                              action='store_true',
                              help='Run the test in LLDB.')

  # This is for version skew testing. The current CI/CQ builder builds
  # an ash chrome and pass it using --ash-chrome-path. In order to use the same
  # builder for version skew testing, we use a new argument to override
  # the ash chrome.
  test_parser.add_argument(
      '--ash-chrome-path-override',
      type=str,
      help='The same as --ash-chrome-path. But this will override '
      '--ash-chrome-path or --ash-chrome-version if any of these '
      'arguments exist.')
  test_parser.add_argument(
      '--ash-logging-path',
      type=str,
      help='File & path to ash-chrome logging output while running Lacros '
      'browser tests. If not provided, no output will be generated.')
  test_parser.add_argument('--combine-ash-logs-on-bots',
                           action='store_true',
                           help='Whether to combine ash logs on bots.')
  test_parser.add_argument(
      '--asan-symbolize-output',
      action='store_true',
      help='Whether to run subprocess log outputs through the asan symbolizer.')

  args = arg_parser.parse_known_args()
  if not hasattr(args[0], "func"):
    # No command specified.
    print(__doc__)
    sys.exit(1)

  return args[0].func(args[0], args[1])


if __name__ == '__main__':
  sys.exit(Main())
