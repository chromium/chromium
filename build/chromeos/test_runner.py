#!/usr/bin/env vpython3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import json
import logging
import os
import re
import shutil
import signal
import socket
import sys
import tempfile

# The following non-std imports are fetched via vpython. See the list at
# //.vpython3
import dateutil.parser  # pylint: disable=import-error
import jsonlines  # pylint: disable=import-error
import psutil  # pylint: disable=import-error

CHROMIUM_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

# Use the android test-runner's gtest results support library for generating
# output json ourselves.
sys.path.insert(0, os.path.join(CHROMIUM_SRC_PATH, 'build', 'android'))
from pylib.base import base_test_result  # pylint: disable=import-error
from pylib.results import json_results  # pylint: disable=import-error

sys.path.insert(0, os.path.join(CHROMIUM_SRC_PATH, 'build', 'util'))
# TODO(crbug.com/40259280): Re-enable the 'no-name-in-module' check.
from lib.results import result_sink  # pylint: disable=import-error,no-name-in-module

import subprocess  # pylint: disable=import-error,wrong-import-order

DEFAULT_CROS_CACHE = os.path.abspath(
    os.path.join(CHROMIUM_SRC_PATH, 'build', 'cros_cache'))
CHROMITE_PATH = os.path.abspath(
    os.path.join(CHROMIUM_SRC_PATH, 'third_party', 'chromite'))
CROS_RUN_TEST_PATH = os.path.abspath(
    os.path.join(CHROMITE_PATH, 'bin', 'cros_run_test'))

LACROS_LAUNCHER_SCRIPT_PATH = os.path.abspath(
    os.path.join(CHROMIUM_SRC_PATH, 'build', 'lacros',
                 'mojo_connection_lacros_launcher.py'))

# This is a special hostname that resolves to a different DUT in the lab
# depending on which lab machine you're on.
LAB_DUT_HOSTNAME = 'variable_chromeos_device_hostname'

SYSTEM_LOG_LOCATIONS = [
    '/home/chronos/crash/',
    '/var/log/chrome/',
    '/var/log/messages',
    '/var/log/ui/',
    '/var/log/lacros/',
]

TAST_DEBUG_DOC = 'https://bit.ly/2LgvIXz'


class TestFormatError(Exception):
  pass


class RemoteTest:

  # This is a basic shell script that can be appended to in order to invoke the
  # test on the device.
  BASIC_SHELL_SCRIPT = [
      '#!/bin/sh',

      # /home and /tmp are mounted with "noexec" in the device, but some of our
      # tools and tests use those dirs as a workspace (eg: vpython downloads
      # python binaries to ~/.vpython-root and /tmp/vpython_bootstrap).
      # /usr/local/tmp doesn't have this restriction, so change the location of
      # the home and temp dirs for the duration of the test.
      'export HOME=/usr/local/tmp',
      'export TMPDIR=/usr/local/tmp',
  ]

  def __init__(self, args, unknown_args):
    self._additional_args = unknown_args
    self._path_to_outdir = args.path_to_outdir
    self._test_launcher_summary_output = args.test_launcher_summary_output
    self._logs_dir = args.logs_dir
    self._use_vm = args.use_vm
    self._rdb_client = result_sink.TryInitClient()

    self._retries = 0
    self._timeout = None
    self._test_launcher_shard_index = args.test_launcher_shard_index
    self._test_launcher_total_shards = args.test_launcher_total_shards

    # The location on disk of a shell script that can be optionally used to
    # invoke the test on the device. If it's not set, we assume self._test_cmd
    # contains the test invocation.
    self._on_device_script = None

    self._test_cmd = [
        CROS_RUN_TEST_PATH,
        '--board',
        args.board,
        '--cache-dir',
        args.cros_cache,
    ]
    if args.use_vm:
      self._test_cmd += [
          '--start',
          # Don't persist any filesystem changes after the VM shutsdown.
          '--copy-on-write',
      ]
    else:
      if args.fetch_cros_hostname:
        self._test_cmd += ['--device', get_cros_hostname()]
      else:
        self._test_cmd += [
            '--device', args.device if args.device else LAB_DUT_HOSTNAME
        ]

    if args.logs_dir:
      for log in SYSTEM_LOG_LOCATIONS:
        self._test_cmd += ['--results-src', log]
      self._test_cmd += [
          '--results-dest-dir',
          os.path.join(args.logs_dir, 'system_logs')
      ]
    if args.flash:
      self._test_cmd += ['--flash']
      if args.public_image:
        self._test_cmd += ['--public-image']

    self._test_env = setup_env()

  @property
  def suite_name(self):
    raise NotImplementedError('Child classes need to define suite name.')

  @property
  def test_cmd(self):
    return self._test_cmd

  def write_test_script_to_disk(self, script_contents):
    # Since we're using an on_device_script to invoke the test, we'll need to
    # set cwd.
    self._test_cmd += [
        '--remote-cmd',
        '--cwd',
        os.path.relpath(self._path_to_outdir, CHROMIUM_SRC_PATH),
    ]
    logging.info('Running the following command on the device:')
    logging.info('\n%s', '\n'.join(script_contents))
    fd, tmp_path = tempfile.mkstemp(suffix='.sh', dir=self._path_to_outdir)
    os.fchmod(fd, 0o755)
    with os.fdopen(fd, 'w') as f:
      f.write('\n'.join(script_contents) + '\n')
    return tmp_path

  def write_runtime_files_to_disk(self, runtime_files):
    logging.info('Writing runtime files to disk.')
    fd, tmp_path = tempfile.mkstemp(suffix='.txt', dir=self._path_to_outdir)
    os.fchmod(fd, 0o755)
    with os.fdopen(fd, 'w') as f:
      f.write('\n'.join(runtime_files) + '\n')
    return tmp_path

  def run_test(self):
    # Traps SIGTERM and kills all child processes of cros_run_test when it's
    # caught. This will allow us to capture logs from the device if a test hangs
    # and gets timeout-killed by swarming. See also:
    # https://chromium.googlesource.com/infra/luci/luci-py/+/main/appengine/swarming/doc/Bot.md#graceful-termination_aka-the-sigterm-and-sigkill-dance
    test_proc = None

    def _kill_child_procs(trapped_signal, _):
      logging.warning('Received signal %d. Killing child processes of test.',
                      trapped_signal)
      if not test_proc or not test_proc.pid:
        # This shouldn't happen?
        logging.error('Test process not running.')
        return
      for child in psutil.Process(test_proc.pid).children():
        logging.warning('Killing process %s', child)
        child.kill()

    signal.signal(signal.SIGTERM, _kill_child_procs)

    for i in range(self._retries + 1):
      logging.info('########################################')
      logging.info('Test attempt #%d', i)
      logging.info('########################################')
      test_proc = subprocess.Popen(
          self._test_cmd,
          stdout=sys.stdout,
          stderr=sys.stderr,
          env=self._test_env)
      try:
        test_proc.wait(timeout=self._timeout)
      except subprocess.TimeoutExpired:  # pylint: disable=no-member
        logging.error('Test timed out. Sending SIGTERM.')
        # SIGTERM the proc and wait 10s for it to close.
        test_proc.terminate()
        try:
          test_proc.wait(timeout=10)
        except subprocess.TimeoutExpired:  # pylint: disable=no-member
          # If it hasn't closed in 10s, SIGKILL it.
          logging.error('Test did not exit in time. Sending SIGKILL.')
          test_proc.kill()
          test_proc.wait()
      logging.info('Test exitted with %d.', test_proc.returncode)
      if test_proc.returncode == 0:
        break

    self.post_run(test_proc.returncode)
    # Allow post_run to override test proc return code. (Useful when the host
    # side Tast bin returns 0 even for failed tests.)
    return test_proc.returncode

  def post_run(self, _):
    if self._on_device_script:
      os.remove(self._on_device_script)

  @staticmethod
  def get_artifacts(path):
    """Crawls a given directory for file artifacts to attach to a test.

    Args:
      path: Path to a directory to search for artifacts.
    Returns:
      A dict mapping name of the artifact to its absolute filepath.
    """
    artifacts = {}
    for dirpath, _, filenames in os.walk(path):
      for f in filenames:
        artifact_path = os.path.join(dirpath, f)
        artifact_id = os.path.relpath(artifact_path, path)
        # Some artifacts will have non-Latin characters in the filename, eg:
        # 'ui_tree_Chinese Pinyin-你好.txt'. ResultDB's API rejects such
        # characters as an artifact ID, so force the file name down into ascii.
        # For more info, see:
        # https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/artifact.proto;drc=3bff13b8037ca76ec19f9810033d914af7ec67cb;l=46
        artifact_id = artifact_id.encode('ascii', 'replace').decode()
        artifact_id = artifact_id.replace('\\', '?')
        artifacts[artifact_id] = {
            'filePath': artifact_path,
        }
    return artifacts


class TastTest(RemoteTest):

  def __init__(self, args, unknown_args):
    super().__init__(args, unknown_args)

    self._suite_name = args.suite_name
    self._tast_vars = args.tast_vars
    self._tast_retries = args.tast_retries
    self._tests = args.tests
    # The CQ passes in '--gtest_filter' when specifying tests to skip. Store it
    # here and parse it later to integrate it into Tast executions.
    self._gtest_style_filter = args.gtest_filter
    self._attr_expr = args.attr_expr
    self._should_strip = args.strip_chrome
    self._deploy_lacros = args.deploy_lacros
    self._deploy_chrome = args.deploy_chrome

    if not self._logs_dir:
      # The host-side Tast bin returns 0 when tests fail, so we need to capture
      # and parse its json results to reliably determine if tests fail.
      raise TestFormatError(
          'When using the host-side Tast bin, "--logs-dir" must be passed in '
          'order to parse its results.')

    # If the first test filter is negative, it should be safe to assume all of
    # them are, so just test the first filter.
    if self._gtest_style_filter and self._gtest_style_filter[0] == '-':
      raise TestFormatError('Negative test filters not supported for Tast.')

  @property
  def suite_name(self):
    return self._suite_name

  def build_test_command(self):
    unsupported_args = [
        '--test-launcher-retry-limit',
        '--test-launcher-batch-limit',
        '--gtest_repeat',
    ]
    for unsupported_arg in unsupported_args:
      if any(arg.startswith(unsupported_arg) for arg in self._additional_args):
        logging.info(
            '%s not supported for Tast tests. The arg will be ignored.',
            unsupported_arg)
        self._additional_args = [
            arg for arg in self._additional_args
            if not arg.startswith(unsupported_arg)
        ]

    # Lacros deployment mounts itself by default.
    if self._deploy_lacros:
      self._test_cmd.extend([
          '--deploy-lacros', '--lacros-launcher-script',
          LACROS_LAUNCHER_SCRIPT_PATH
      ])
      if self._deploy_chrome:
        self._test_cmd.extend(['--deploy', '--mount'])
    else:
      self._test_cmd.extend(['--deploy', '--mount'])
    self._test_cmd += [
        '--build-dir',
        os.path.relpath(self._path_to_outdir, CHROMIUM_SRC_PATH)
    ] + self._additional_args

    # Capture tast's results in the logs dir as well.
    if self._logs_dir:
      self._test_cmd += [
          '--results-dir',
          self._logs_dir,
      ]
    self._test_cmd += [
        '--tast-total-shards=%d' % self._test_launcher_total_shards,
        '--tast-shard-index=%d' % self._test_launcher_shard_index,
    ]
    # If we're using a test filter, replace the contents of the Tast
    # conditional with a long list of "name:test" expressions, one for each
    # test in the filter.
    if self._gtest_style_filter:
      if self._attr_expr or self._tests:
        logging.warning(
            'Presence of --gtest_filter will cause the specified Tast expr'
            ' or test list to be ignored.')
      names = []
      for test in self._gtest_style_filter.split(':'):
        names.append('"name:%s"' % test)
      self._attr_expr = '(' + ' || '.join(names) + ')'

    if self._attr_expr:
      # Don't use shlex.quote() here. Something funky happens with the arg
      # as it gets passed down from cros_run_test to tast. (Tast picks up the
      # escaping single quotes and complains that the attribute expression
      # "must be within parentheses".)
      self._test_cmd.append('--tast=%s' % self._attr_expr)
    else:
      self._test_cmd.append('--tast')
      self._test_cmd.extend(self._tests)

    for v in self._tast_vars or []:
      self._test_cmd.extend(['--tast-var', v])

    if self._tast_retries:
      self._test_cmd.append('--tast-retries=%d' % self._tast_retries)

    # Mounting ash-chrome gives it enough disk space to not need stripping,
    # but only for one not instrumented with code coverage.
    # Lacros uses --nostrip by default, so there is no need to specify.
    if not self._deploy_lacros and not self._should_strip:
      self._test_cmd.append('--nostrip')

  def post_run(self, return_code):
    tast_results_path = os.path.join(self._logs_dir, 'streamed_results.jsonl')
    if not os.path.exists(tast_results_path):
      logging.error(
          'Tast results not found at %s. Falling back to generic result '
          'reporting.', tast_results_path)
      return super().post_run(return_code)

    # See the link below for the format of the results:
    # https://godoc.org/chromium.googlesource.com/chromiumos/platform/tast.git/src/chromiumos/cmd/tast/run#TestResult
    with jsonlines.open(tast_results_path) as reader:
      tast_results = collections.deque(reader)

    suite_results = base_test_result.TestRunResults()
    for test in tast_results:
      errors = test['errors']
      start, end = test['start'], test['end']
      # Use dateutil to parse the timestamps since datetime can't handle
      # nanosecond precision.
      duration = dateutil.parser.parse(end) - dateutil.parser.parse(start)
      # If the duration is negative, Tast has likely reported an incorrect
      # duration. See https://issuetracker.google.com/issues/187973541. Round
      # up to 0 in that case to avoid confusing RDB.
      duration_ms = max(duration.total_seconds() * 1000, 0)
      if bool(test['skipReason']):
        result = base_test_result.ResultType.SKIP
      elif errors:
        result = base_test_result.ResultType.FAIL
      else:
        result = base_test_result.ResultType.PASS
      primary_error_message = None
      error_log = ''
      if errors:
        # See the link below for the format of these errors:
        # https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast/src/chromiumos/tast/cmd/tast/internal/run/resultsjson/resultsjson.go
        primary_error_message = errors[0]['reason']
        for err in errors:
          error_log += err['stack'] + '\n'
      debug_link = ("If you're unsure why this test failed, consult the steps "
                    'outlined <a href="%s">here</a>.' % TAST_DEBUG_DOC)
      base_result = base_test_result.BaseTestResult(
          test['name'], result, duration=duration_ms, log=error_log)
      suite_results.AddResult(base_result)
      self._maybe_handle_perf_results(test['name'])

      if self._rdb_client:
        # Walk the contents of the test's "outDir" and atttach any file found
        # inside as an RDB 'artifact'. (This could include system logs, screen
        # shots, etc.)
        artifacts = self.get_artifacts(test['outDir'])
        html_artifact = debug_link
        if result == base_test_result.ResultType.SKIP:
          html_artifact = 'Test was skipped because: ' + test['skipReason']
        self._rdb_client.Post(
            test['name'],
            result,
            duration_ms,
            error_log,
            None,
            artifacts=artifacts,
            failure_reason=primary_error_message,
            html_artifact=html_artifact)

    if self._rdb_client and self._logs_dir:
      # Attach artifacts from the device that don't apply to a single test.
      artifacts = self.get_artifacts(
          os.path.join(self._logs_dir, 'system_logs'))
      artifacts.update(
          self.get_artifacts(os.path.join(self._logs_dir, 'crashes')))
      self._rdb_client.ReportInvocationLevelArtifacts(artifacts)

    if self._test_launcher_summary_output:
      with open(self._test_launcher_summary_output, 'w') as f:
        json.dump(json_results.GenerateResultsDict([suite_results]), f)

    if not suite_results.DidRunPass():
      return 1
    if return_code:
      logging.warning(
          'No failed tests found, but exit code of %d was returned from '
          'cros_run_test.', return_code)
      return return_code
    return 0

  def _maybe_handle_perf_results(self, test_name):
    """Prepares any perf results from |test_name| for process_perf_results.

    - process_perf_results looks for top level directories containing a
      perf_results.json file and a test_results.json file. The directory names
      are used as the benchmark names.
    - If a perf_results.json or results-chart.json file exists in the
      |test_name| results directory, a top level directory is created and the
      perf results file is copied to perf_results.json.
    - A trivial test_results.json file is also created to indicate that the test
      succeeded (this function would not be called otherwise).
    - When process_perf_results is run, it will find the expected files in the
      named directory and upload the benchmark results.
    """

    perf_results = os.path.join(self._logs_dir, 'tests', test_name,
                                'perf_results.json')
    # TODO(stevenjb): Remove check for crosbolt results-chart.json file.
    if not os.path.exists(perf_results):
      perf_results = os.path.join(self._logs_dir, 'tests', test_name,
                                  'results-chart.json')
    if os.path.exists(perf_results):
      benchmark_dir = os.path.join(self._logs_dir, test_name)
      if not os.path.isdir(benchmark_dir):
        os.makedirs(benchmark_dir)
      shutil.copyfile(perf_results,
                      os.path.join(benchmark_dir, 'perf_results.json'))
      # process_perf_results.py expects a test_results.json file.
      test_results = {'valid': True, 'failures': []}
      with open(os.path.join(benchmark_dir, 'test_results.json'), 'w') as out:
        json.dump(test_results, out)


class GTestTest(RemoteTest):

  # The following list corresponds to paths that should not be copied over to
  # the device during tests. In other words, these files are only ever used on
  # the host.
  _FILE_IGNORELIST = [
      re.compile(r'.*build/android.*'),
      re.compile(r'.*build/chromeos.*'),
      re.compile(r'.*build/cros_cache.*'),
      # The following matches anything under //testing/ that isn't under
      # //testing/buildbot/filters/.
      re.compile(r'.*testing/(?!buildbot/filters).*'),
      re.compile(r'.*third_party/chromite.*'),
  ]

  def __init__(self, args, unknown_args):
    super().__init__(args, unknown_args)

    self._test_cmd = ['vpython3'] + self._test_cmd
    if not args.clean:
      self._test_cmd += ['--no-clean']

    self._test_exe = args.test_exe
    self._runtime_deps_path = args.runtime_deps_path
    self._vpython_dir = args.vpython_dir

    self._on_device_script = None
    self._env_vars = args.env_var
    self._stop_ui = args.stop_ui
    self._as_root = args.as_root
    self._trace_dir = args.trace_dir
    self._run_test_sudo_helper = args.run_test_sudo_helper
    self._set_selinux_label = args.set_selinux_label
    self._use_deployed_dbus_configs = args.use_deployed_dbus_configs

  @property
  def suite_name(self):
    return self._test_exe

  def build_test_command(self):
    # To keep things easy for us, ensure both types of output locations are
    # the same.
    if self._test_launcher_summary_output and self._logs_dir:
      json_out_dir = os.path.dirname(self._test_launcher_summary_output) or '.'
      if os.path.abspath(json_out_dir) != os.path.abspath(self._logs_dir):
        raise TestFormatError(
            '--test-launcher-summary-output and --logs-dir must point to '
            'the same directory.')

    if self._test_launcher_summary_output:
      result_dir, result_file = os.path.split(
          self._test_launcher_summary_output)
      # If args.test_launcher_summary_output is a file in cwd, result_dir will
      # be an empty string, so replace it with '.' when this is the case so
      # cros_run_test can correctly handle it.
      if not result_dir:
        result_dir = '.'
      device_result_file = '/tmp/%s' % result_file
      self._test_cmd += [
          '--results-src',
          device_result_file,
          '--results-dest-dir',
          result_dir,
      ]

    if self._trace_dir and self._logs_dir:
      trace_path = os.path.dirname(self._trace_dir) or '.'
      if os.path.abspath(trace_path) != os.path.abspath(self._logs_dir):
        raise TestFormatError(
            '--trace-dir and --logs-dir must point to the same directory.')

    if self._trace_dir:
      trace_path, trace_dirname = os.path.split(self._trace_dir)
      device_trace_dir = '/tmp/%s' % trace_dirname
      self._test_cmd += [
          '--results-src',
          device_trace_dir,
          '--results-dest-dir',
          trace_path,
      ]

    # Build the shell script that will be used on the device to invoke the test.
    # Stored here as a list of lines.
    device_test_script_contents = self.BASIC_SHELL_SCRIPT[:]
    for var_name, var_val in self._env_vars:
      device_test_script_contents += ['export %s=%s' % (var_name, var_val)]

    if self._vpython_dir:
      vpython_path = os.path.join(self._path_to_outdir, self._vpython_dir,
                                  'vpython3')
      cpython_path = os.path.join(self._path_to_outdir, self._vpython_dir,
                                  'bin', 'python3')
      if not os.path.exists(vpython_path) or not os.path.exists(cpython_path):
        raise TestFormatError(
            '--vpython-dir must point to a dir with both '
            'infra/3pp/tools/cpython3 and infra/tools/luci/vpython3 '
            'installed.')
      vpython_spec_path = os.path.relpath(
          os.path.join(CHROMIUM_SRC_PATH, '.vpython3'), self._path_to_outdir)
      # Initialize the vpython cache. This can take 10-20s, and some tests
      # can't afford to wait that long on the first invocation.
      device_test_script_contents.extend([
          'export PATH=$PWD/%s:$PWD/%s/bin/:$PATH' %
          (self._vpython_dir, self._vpython_dir),
          'vpython3 -vpython-spec %s -vpython-tool install' %
          (vpython_spec_path),
      ])

    test_invocation = ('LD_LIBRARY_PATH=./ ./%s --test-launcher-shard-index=%d '
                       '--test-launcher-total-shards=%d' %
                       (self._test_exe, self._test_launcher_shard_index,
                        self._test_launcher_total_shards))
    if self._test_launcher_summary_output:
      test_invocation += ' --test-launcher-summary-output=%s' % (
          device_result_file)

    if self._trace_dir:
      device_test_script_contents.extend([
          'rm -rf %s' % device_trace_dir,
          'sudo -E -u chronos -- /bin/bash -c "mkdir -p %s"' % device_trace_dir,
      ])
      test_invocation += ' --trace-dir=%s' % device_trace_dir

    if self._run_test_sudo_helper:
      device_test_script_contents.extend([
          'TEST_SUDO_HELPER_PATH=$(mktemp)',
          './test_sudo_helper.py --socket-path=${TEST_SUDO_HELPER_PATH} &',
          'TEST_SUDO_HELPER_PID=$!'
      ])
      test_invocation += (
          ' --test-sudo-helper-socket-path=${TEST_SUDO_HELPER_PATH}')

    # Append the selinux labels. The 'setfiles' command takes a file with each
    # line consisting of "<file-regex> <file-type> <new-label>", where '--' is
    # the type of a regular file.
    if self._set_selinux_label:
      for label_pair in self._set_selinux_label:
        filename, label = label_pair.split('=', 1)
        specfile = filename + '.specfile'
        device_test_script_contents.extend([
            'echo %s -- %s > %s' % (filename, label, specfile),
            'setfiles -F %s %s' % (specfile, filename),
        ])

    # Mount the deploy dbus config dir on top of chrome's dbus dir. Send SIGHUP
    # to dbus daemon to reload config from the newly mounted dir.
    if self._use_deployed_dbus_configs:
      device_test_script_contents.extend([
          'mount --bind ./dbus /opt/google/chrome/dbus',
          'kill -s HUP $(pgrep dbus)',
      ])

    if self._additional_args:
      test_invocation += ' %s' % ' '.join(self._additional_args)

    if self._stop_ui:
      device_test_script_contents += [
          'stop ui',
      ]
      # Send a user activity ping to powerd to ensure the display is on.
      device_test_script_contents += [
          'dbus-send --system --type=method_call'
          ' --dest=org.chromium.PowerManager /org/chromium/PowerManager'
          ' org.chromium.PowerManager.HandleUserActivity int32:0'
      ]
      # The UI service on the device owns the chronos user session, so shutting
      # it down as chronos kills the entire execution of the test. So we'll have
      # to run as root up until the test invocation.
      test_invocation = (
          'sudo -E -u chronos -- /bin/bash -c "%s"' % test_invocation)
      # And we'll need to chown everything since cros_run_test's "--as-chronos"
      # option normally does that for us.
      device_test_script_contents.append('chown -R chronos: ../..')
    elif not self._as_root:
      self._test_cmd += [
          # Some tests fail as root, so run as the less privileged user
          # 'chronos'.
          '--as-chronos',
      ]

    device_test_script_contents.append(test_invocation)
    device_test_script_contents.append('TEST_RETURN_CODE=$?')

    # (Re)start ui after all tests are done. This is for developer convenienve.
    # Without this, the device would remain in a black screen which looks like
    # powered off.
    if self._stop_ui:
      device_test_script_contents += [
          'start ui',
      ]

    # Stop the crosier helper.
    if self._run_test_sudo_helper:
      device_test_script_contents.extend([
          'pkill -P $TEST_SUDO_HELPER_PID',
          'kill $TEST_SUDO_HELPER_PID',
          'unlink ${TEST_SUDO_HELPER_PATH}',
      ])

    # Undo the dbus config mount and reload dbus config.
    if self._use_deployed_dbus_configs:
      device_test_script_contents.extend([
          'umount /opt/google/chrome/dbus',
          'kill -s HUP $(pgrep dbus)',
      ])

    # This command should always be the last bash commandline so infra can
    # correctly get the error code from test invocations.
    device_test_script_contents.append('exit $TEST_RETURN_CODE')

    self._on_device_script = self.write_test_script_to_disk(
        device_test_script_contents)

    runtime_files = [os.path.relpath(self._on_device_script)]
    runtime_files += self._read_runtime_files()
    if self._vpython_dir:
      # --vpython-dir is relative to the out dir, but --files-from expects paths
      # relative to src dir, so fix the path up a bit.
      runtime_files.append(
          os.path.relpath(
              os.path.abspath(
                  os.path.join(self._path_to_outdir, self._vpython_dir)),
              CHROMIUM_SRC_PATH))

    self._test_cmd.extend(
        ['--files-from',
         self.write_runtime_files_to_disk(runtime_files)])

    self._test_cmd += [
        '--',
        './' + os.path.relpath(self._on_device_script, self._path_to_outdir)
    ]

  def _read_runtime_files(self):
    if not self._runtime_deps_path:
      return []

    abs_runtime_deps_path = os.path.abspath(
        os.path.join(self._path_to_outdir, self._runtime_deps_path))
    with open(abs_runtime_deps_path) as runtime_deps_file:
      files = [l.strip() for l in runtime_deps_file if l]
    rel_file_paths = []
    for f in files:
      rel_file_path = os.path.relpath(
          os.path.abspath(os.path.join(self._path_to_outdir, f)))
      if not any(regex.match(rel_file_path) for regex in self._FILE_IGNORELIST):
        rel_file_paths.append(rel_file_path)
    return rel_file_paths

  def post_run(self, _):
    if self._on_device_script:
      os.remove(self._on_device_script)

    if self._test_launcher_summary_output and self._rdb_client:
      logging.error('Native ResultDB integration is not supported for GTests. '
                    'Upload results via result_adapter instead. '
                    'See crbug.com/1330441.')


def device_test(args, unknown_args):
  # cros_run_test has trouble with relative paths that go up directories,
  # so cd to src/, which should be the root of all data deps.
  os.chdir(CHROMIUM_SRC_PATH)

  # TODO: Remove the above when depot_tool's pylint is updated to include the
  # fix to https://github.com/PyCQA/pylint/issues/710.
  if args.test_type == 'tast':
    test = TastTest(args, unknown_args)
  else:
    test = GTestTest(args, unknown_args)

  test.build_test_command()
  logging.info('Running the following command on the device:')
  logging.info(' '.join(test.test_cmd))

  return test.run_test()


def host_cmd(args, cmd_args):
  if not cmd_args:
    raise TestFormatError('Must specify command to run on the host.')
  if args.deploy_chrome and not args.path_to_outdir:
    raise TestFormatError(
        '--path-to-outdir must be specified if --deploy-chrome is passed.')

  cros_run_test_cmd = [
      CROS_RUN_TEST_PATH,
      '--board',
      args.board,
      '--cache-dir',
      os.path.join(CHROMIUM_SRC_PATH, args.cros_cache),
  ]
  if args.use_vm:
    cros_run_test_cmd += [
        '--start',
        # Don't persist any filesystem changes after the VM shutsdown.
        '--copy-on-write',
    ]
  else:
    if args.fetch_cros_hostname:
      cros_run_test_cmd += ['--device', get_cros_hostname()]
    else:
      cros_run_test_cmd += [
          '--device', args.device if args.device else LAB_DUT_HOSTNAME
      ]
  if args.verbose:
    cros_run_test_cmd.append('--debug')
  if args.flash:
    cros_run_test_cmd.append('--flash')
    if args.public_image:
      cros_run_test_cmd += ['--public-image']

  if args.logs_dir:
    for log in SYSTEM_LOG_LOCATIONS:
      cros_run_test_cmd += ['--results-src', log]
    cros_run_test_cmd += [
        '--results-dest-dir',
        os.path.join(args.logs_dir, 'system_logs')
    ]

  test_env = setup_env()
  if args.deploy_chrome or args.deploy_lacros:
    if args.deploy_lacros:
      cros_run_test_cmd.extend([
          '--deploy-lacros', '--lacros-launcher-script',
          LACROS_LAUNCHER_SCRIPT_PATH
      ])
      if args.deploy_chrome:
        # Mounting ash-chrome gives it enough disk space to not need stripping
        # most of the time.
        cros_run_test_cmd.extend(['--deploy', '--mount'])
    else:
      # Mounting ash-chrome gives it enough disk space to not need stripping
      # most of the time.
      cros_run_test_cmd.extend(['--deploy', '--mount'])

    if not args.strip_chrome:
      cros_run_test_cmd.append('--nostrip')

    cros_run_test_cmd += [
        '--build-dir',
        os.path.join(CHROMIUM_SRC_PATH, args.path_to_outdir)
    ]

  cros_run_test_cmd += [
      '--host-cmd',
      '--',
  ] + cmd_args

  logging.info('Running the following command:')
  logging.info(' '.join(cros_run_test_cmd))

  return subprocess.call(
      cros_run_test_cmd, stdout=sys.stdout, stderr=sys.stderr, env=test_env)


def get_cros_hostname_from_bot_id(bot_id):
  """Parse hostname from a chromeos-swarming bot id."""
  for prefix in ['cros-', 'crossk-']:
    if bot_id.startswith(prefix):
      return bot_id[len(prefix):]
  return bot_id


def get_cros_hostname():
  """Fetch bot_id from env var and parse hostname."""

  # In chromeos-swarming, we can extract hostname from bot ID, since
  # bot ID is formatted as "{prefix}{hostname}".
  bot_id = os.environ.get('SWARMING_BOT_ID')
  if bot_id:
    return get_cros_hostname_from_bot_id(bot_id)

  logging.warning(
      'Attempted to read from SWARMING_BOT_ID env var and it was'
      ' not defined. Will set %s as device instead.', LAB_DUT_HOSTNAME)
  return LAB_DUT_HOSTNAME


def setup_env():
  """Returns a copy of the current env with some needed vars added."""
  env = os.environ.copy()
  # Some chromite scripts expect chromite/bin to be on PATH.
  env['PATH'] = env['PATH'] + ':' + os.path.join(CHROMITE_PATH, 'bin')
  # deploy_chrome needs a set of GN args used to build chrome to determine if
  # certain libraries need to be pushed to the device. It looks for the args via
  # an env var. To trigger the default deploying behavior, give it a dummy set
  # of args.
  # TODO(crbug.com/40567963): Make the GN-dependent deps controllable via cmd
  # line args.
  if not env.get('GN_ARGS'):
    env['GN_ARGS'] = 'enable_nacl = true'
  if not env.get('USE'):
    env['USE'] = 'highdpi'
  return env


def add_common_args(*parsers):
  for parser in parsers:
    parser.add_argument('--verbose', '-v', action='store_true')
    parser.add_argument(
        '--board', type=str, required=True, help='Type of CrOS device.')
    parser.add_argument(
        '--deploy-chrome',
        action='store_true',
        help='Will deploy a locally built ash-chrome binary to the device '
        'before running the host-cmd.')
    parser.add_argument(
        '--deploy-lacros', action='store_true', help='Deploy a lacros-chrome.')
    parser.add_argument(
        '--cros-cache',
        type=str,
        default=DEFAULT_CROS_CACHE,
        help='Path to cros cache.')
    parser.add_argument(
        '--path-to-outdir',
        type=str,
        required=True,
        help='Path to output directory, all of whose contents will be '
        'deployed to the device.')
    parser.add_argument(
        '--runtime-deps-path',
        type=str,
        help='Runtime data dependency file from GN.')
    parser.add_argument(
        '--vpython-dir',
        type=str,
        help='Location on host of a directory containing a vpython binary to '
        'deploy to the device before the test starts. The location of '
        'this dir will be added onto PATH in the device. WARNING: The '
        'arch of the device might not match the arch of the host, so '
        'avoid using "${platform}" when downloading vpython via CIPD.')
    parser.add_argument(
        '--logs-dir',
        type=str,
        dest='logs_dir',
        help='Will copy everything under /var/log/ from the device after the '
        'test into the specified dir.')
    # Shard args are parsed here since we might also specify them via env vars.
    parser.add_argument(
        '--test-launcher-shard-index',
        type=int,
        default=os.environ.get('GTEST_SHARD_INDEX', 0),
        help='Index of the external shard to run.')
    parser.add_argument(
        '--test-launcher-total-shards',
        type=int,
        default=os.environ.get('GTEST_TOTAL_SHARDS', 1),
        help='Total number of external shards.')
    parser.add_argument(
        '--flash',
        action='store_true',
        help='Will flash the device to the current SDK version before running '
        'the test.')
    parser.add_argument(
        '--no-flash',
        action='store_false',
        dest='flash',
        help='Will not flash the device before running the test.')
    parser.add_argument(
        '--public-image',
        action='store_true',
        help='Will flash a public "full" image to the device.')
    parser.add_argument(
        '--magic-vm-cache',
        help='Path to the magic CrOS VM cache dir. See the comment above '
             '"magic_cros_vm_cache" in mixins.pyl for more info.')

    vm_or_device_group = parser.add_mutually_exclusive_group()
    vm_or_device_group.add_argument(
        '--use-vm',
        action='store_true',
        help='Will run the test in the VM instead of a device.')
    vm_or_device_group.add_argument(
        '--device',
        type=str,
        help='Hostname (or IP) of device to run the test on. This arg is not '
        'required if --use-vm is set.')
    vm_or_device_group.add_argument(
        '--fetch-cros-hostname',
        action='store_true',
        help='Will extract device hostname from the SWARMING_BOT_ID env var if '
        'running on ChromeOS Swarming.')

def main():
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest='test_type')
  # Host-side test args.
  host_cmd_parser = subparsers.add_parser(
      'host-cmd',
      help='Runs a host-side test. Pass the host-side command to run after '
      '"--". If --use-vm is passed, hostname and port for the device '
      'will be 127.0.0.1:9222.')
  host_cmd_parser.set_defaults(func=host_cmd)
  host_cmd_parser.add_argument(
      '--strip-chrome',
      action='store_true',
      help='Strips symbols from ash-chrome or lacros-chrome before deploying '
      ' to the device.')

  gtest_parser = subparsers.add_parser(
      'gtest', help='Runs a device-side gtest.')
  gtest_parser.set_defaults(func=device_test)
  gtest_parser.add_argument(
      '--test-exe',
      type=str,
      required=True,
      help='Path to test executable to run inside the device.')

  # GTest args. Some are passed down to the test binary in the device. Others
  # are parsed here since they might need tweaking or special handling.
  gtest_parser.add_argument(
      '--test-launcher-summary-output',
      type=str,
      help='When set, will pass the same option down to the test and retrieve '
      'its result file at the specified location.')
  gtest_parser.add_argument(
      '--stop-ui',
      action='store_true',
      help='Will stop the UI service in the device before running the test. '
      'Also start the UI service after all tests are done.')
  gtest_parser.add_argument(
      '--as-root',
      action='store_true',
      help='Will run the test as root on the device. Runs as user=chronos '
      'otherwise. This is mutually exclusive with "--stop-ui" above due to '
      'setup issues.')
  gtest_parser.add_argument(
      '--trace-dir',
      type=str,
      help='When set, will pass down to the test to generate the trace and '
      'retrieve the trace files to the specified location.')
  gtest_parser.add_argument(
      '--env-var',
      nargs=2,
      action='append',
      default=[],
      help='Env var to set on the device for the duration of the test. '
      'Expected format is "--env-var SOME_VAR_NAME some_var_value". Specify '
      'multiple times for more than one var.')
  gtest_parser.add_argument(
      '--run-test-sudo-helper',
      action='store_true',
      help='When set, will run test_sudo_helper before the test and stop it '
      'after test finishes.')
  gtest_parser.add_argument(
      "--no-clean",
      action="store_false",
      dest="clean",
      default=True,
      help="Do not clean up the deployed files after running the test. "
      "Only supported for --remote-cmd tests")
  gtest_parser.add_argument(
      '--set-selinux-label',
      action='append',
      default=[],
      help='Set the selinux label for a file before running. The format is:\n'
      '  --set-selinux-label=<filename>=<label>\n'
      'So:\n'
      '  --set-selinux-label=my_test=u:r:cros_foo_label:s0\n'
      'You can specify it more than one time to set multiple files tags.')
  gtest_parser.add_argument(
      '--use-deployed-dbus-configs',
      action='store_true',
      help='When set, will bind mount deployed dbus config to chrome dbus dir '
      'and ask dbus daemon to reload config before running tests.')

  # Tast test args.
  # pylint: disable=line-too-long
  tast_test_parser = subparsers.add_parser(
      'tast',
      help='Runs a device-side set of Tast tests. For more details, see: '
      'https://chromium.googlesource.com/chromiumos/platform/tast/+/main/docs/running_tests.md'
  )
  tast_test_parser.set_defaults(func=device_test)
  tast_test_parser.add_argument(
      '--suite-name',
      type=str,
      required=True,
      help='Name to apply to the set of Tast tests to run. This has no effect '
      'on what is executed, but is used mainly for test results reporting '
      'and tracking (eg: flakiness dashboard).')
  tast_test_parser.add_argument(
      '--test-launcher-summary-output',
      type=str,
      help='Generates a simple GTest-style JSON result file for the test run.')
  tast_test_parser.add_argument(
      '--attr-expr',
      type=str,
      help='A boolean expression whose matching tests will run '
      '(eg: ("dep:chrome")).')
  tast_test_parser.add_argument(
      '--strip-chrome',
      action='store_true',
      help='Strips symbols from ash-chrome before deploying to the device.')
  tast_test_parser.add_argument(
      '--tast-var',
      action='append',
      dest='tast_vars',
      help='Runtime variables for Tast tests, and the format are expected to '
      'be "key=value" pairs.')
  tast_test_parser.add_argument(
      '--tast-retries',
      type=int,
      dest='tast_retries',
      help='Number of retries for failed Tast tests on the same DUT.')
  tast_test_parser.add_argument(
      '--test',
      '-t',
      action='append',
      dest='tests',
      help='A Tast test to run in the device (eg: "login.Chrome").')
  tast_test_parser.add_argument(
      '--gtest_filter',
      type=str,
      help="Similar to GTest's arg of the same name, this will filter out the "
      "specified tests from the Tast run. However, due to the nature of Tast's "
      'cmd-line API, this will overwrite the value(s) of "--test" above.')

  add_common_args(gtest_parser, tast_test_parser, host_cmd_parser)
  args, unknown_args = parser.parse_known_args()

  if args.test_type == 'gtest' and args.stop_ui and args.as_root:
    parser.error('Unable to run gtests with both --stop-ui and --as-root')

  # Re-add N-1 -v/--verbose flags to the args we'll pass to whatever we are
  # running. The assumption is that only one verbosity incrase would be meant
  # for this script since it's a boolean value instead of increasing verbosity
  # with more instances.
  verbose_flags = [a for a in sys.argv if a in ('-v', '--verbose')]
  if verbose_flags:
    unknown_args += verbose_flags[1:]

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARN)

  if not args.use_vm and not args.device and not args.fetch_cros_hostname:
    logging.warning(
        'The test runner is now assuming running in the lab environment, if '
        'this is unintentional, please re-invoke the test runner with the '
        '"--use-vm" arg if using a VM, otherwise use the "--device=<DUT>" arg '
        'to specify a DUT.')

    # If we're not running on a VM, but haven't specified a hostname, assume
    # we're on a lab bot and are trying to run a test on a lab DUT. See if the
    # magic lab DUT hostname resolves to anything. (It will in the lab and will
    # not on dev machines.)
    try:
      socket.getaddrinfo(LAB_DUT_HOSTNAME, None)
    except socket.gaierror:
      logging.error('The default lab DUT hostname of %s is unreachable.',
                    LAB_DUT_HOSTNAME)
      return 1

  if args.flash and args.public_image:
    # The flashing tools depend on being unauthenticated with GS when flashing
    # public images, so make sure the env var GS uses to locate its creds is
    # unset in that case.
    os.environ.pop('BOTO_CONFIG', None)

  if args.magic_vm_cache:
    full_vm_cache_path = os.path.join(CHROMIUM_SRC_PATH, args.magic_vm_cache)
    if os.path.exists(full_vm_cache_path):
      with open(os.path.join(full_vm_cache_path, 'swarming.txt'), 'w') as f:
        f.write('non-empty file to make swarming persist this cache')

  return args.func(args, unknown_args)


if __name__ == '__main__':
  sys.exit(main())
