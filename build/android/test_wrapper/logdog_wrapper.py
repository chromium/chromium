#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for adding logdog streaming support to swarming tasks."""

import argparse
import contextlib
import json
import logging
import os
import shutil
import signal
import subprocess
import sys

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'common',
                             'py_utils'))

from devil.utils import signal_handler
from devil.utils import timeout_retry
from py_utils import tempfile_ext

OUTPUT = 'logdog'
COORDINATOR_HOST = 'luci-logdog.appspot.com'
LOGDOG_TERMINATION_TIMEOUT = 30


def CommandParser():
  # Parses the command line arguments being passed in
  parser = argparse.ArgumentParser(allow_abbrev=False)
  wrapped = parser.add_mutually_exclusive_group()
  wrapped.add_argument(
      '--target',
      help='The test target to be run. If neither target nor script are set,'
      ' any extra args passed to this script are assumed to be the'
      ' full test command to run.')
  wrapped.add_argument(
      '--script',
      help='The script target to be run. If neither target nor script are set,'
      ' any extra args passed to this script are assumed to be the'
      ' full test command to run.')
  parser.add_argument('--logdog-bin-cmd',
                      help='Location of the logdog butler binary. Will attempt '
                      'to find it on PATH if not specified. If not found, this '
                      'script will be a no-op and simply passthrough to the '
                      'test command.')
  return parser


def CreateStopTestsMethod(proc):
  def StopTests(signum, _frame):
    logging.error('Forwarding signal %s to test process', str(signum))
    proc.send_signal(signum)
  return StopTests


@contextlib.contextmanager
def NoLeakingProcesses(popen):
  try:
    yield popen
  finally:
    if popen is not None:
      try:
        if popen.poll() is None:
          popen.kill()
      except OSError:
        logging.warning('Failed to kill %s. Process may be leaked.',
                        str(popen.pid))


def GetProjectFromLuciContext():
  """Return the "project" from LUCI_CONTEXT.

  LUCI_CONTEXT contains a section "realm.name" whose value follows the format
  "<project>:<realm>". This method parses and return the "project" part.

  Fallback to "chromium" if realm name is None
  """
  project = 'chromium'
  ctx_path = os.environ.get('LUCI_CONTEXT')
  if ctx_path:
    try:
      with open(ctx_path) as f:
        luci_ctx = json.load(f)
        realm_name = luci_ctx.get('realm', {}).get('name')
        if realm_name:
          project = realm_name.split(':')[0]
    except (OSError, IOError, ValueError):
      pass
  return project


def main():
  parser = CommandParser()
  args, extra_cmd_args = parser.parse_known_args(sys.argv[1:])

  logging.basicConfig(level=logging.INFO)
  if args.target:
    test_cmd = [os.path.join('bin', 'run_%s' % args.target), '-v']
    test_cmd += extra_cmd_args
  elif args.script:
    test_cmd = [args.script]
    test_cmd += extra_cmd_args
  else:
    test_cmd = extra_cmd_args

  test_env = dict(os.environ)
  logdog_cmd = []
  logdog_butler_bin = args.logdog_bin_cmd
  if os.environ.get('SWARMING_TASK_ID'):
    logdog_butler_bin = logdog_butler_bin or shutil.which('logdog_butler')
    if not logdog_butler_bin or not os.path.exists(logdog_butler_bin):
      parser.error('Either --logdog-bin-cmd must be specified and valid or '
                   '"logdog_butler" must be on PATH if running on swarming.')

  with tempfile_ext.NamedTemporaryDirectory(
      prefix='tmp_android_logdog_wrapper') as temp_directory:
    if logdog_butler_bin:
      streamserver_uri = 'unix:%s' % os.path.join(temp_directory, 'butler.sock')
      prefix = os.path.join('android', 'swarming', 'logcats',
                            os.environ.get('SWARMING_TASK_ID'))
      project = GetProjectFromLuciContext()

      logdog_cmd = [
          logdog_butler_bin, '-project', project, '-output', OUTPUT, '-prefix',
          prefix, '-coordinator-host', COORDINATOR_HOST, 'serve',
          '-streamserver-uri', streamserver_uri
      ]
      test_env.update({
          'LOGDOG_STREAM_PROJECT': project,
          'LOGDOG_STREAM_PREFIX': prefix,
          'LOGDOG_STREAM_SERVER_PATH': streamserver_uri,
          'LOGDOG_COORDINATOR_HOST': COORDINATOR_HOST,
      })

    logdog_proc = None
    if logdog_cmd:
      logdog_proc = subprocess.Popen(logdog_cmd)

    with NoLeakingProcesses(logdog_proc):
      with NoLeakingProcesses(
          subprocess.Popen(test_cmd, env=test_env)) as test_proc:
        with signal_handler.SignalHandler(signal.SIGTERM,
                                          CreateStopTestsMethod(test_proc)):
          result = test_proc.wait()
          if logdog_proc:
            def logdog_stopped():
              return logdog_proc.poll() is not None

            logdog_proc.terminate()
            timeout_retry.WaitFor(logdog_stopped, wait_period=1,
                                  max_tries=LOGDOG_TERMINATION_TIMEOUT)

            # If logdog_proc hasn't finished by this point, allow
            # NoLeakingProcesses to kill it.


  return result


if __name__ == '__main__':
  sys.exit(main())
