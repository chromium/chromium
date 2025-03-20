#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import datetime
import pathlib
import unittest
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

import fast_local_dev_server as server

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils


class RegexTest(unittest.TestCase):

  def testBuildIdRegex(self):
    self.assertRegex(server.FIRST_LOG_LINE.format(build_id='abc', outdir='PWD'),
                     server.BUILD_ID_RE)


def sendMessage(message):
  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    sock.settimeout(1)
    sock.connect(server_utils.SOCKET_ADDRESS)
    server_utils.SendMessage(sock, message)


def pollServer():
  try:
    sendMessage({'message_type': server_utils.POLL_HEARTBEAT})
    return True
  except ConnectionRefusedError:
    return False


def shouldSkip():
  if os.environ.get('ALLOW_EXTERNAL_BUILD_SERVER', None):
    return False
  return pollServer()


def callServer(args, check=True):
  return subprocess.run([str(server_utils.SERVER_SCRIPT.absolute())] + args,
                        cwd=pathlib.Path(__file__).parent,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        check=check,
                        text=True,
                        timeout=3)


@contextlib.contextmanager
def blockingFifo(fifo_path='/tmp/.fast_local_dev_server_test.fifo'):
  fifo_path = pathlib.Path(fifo_path)
  try:
    if not fifo_path.exists():
      os.mkfifo(fifo_path)
    yield fifo_path
  finally:
    # Write to the fifo nonblocking to unblock other end.
    try:
      pipe = os.open(fifo_path, os.O_WRONLY | os.O_NONBLOCK)
      os.write(pipe, b'')
      os.close(pipe)
    except OSError:
      # Can't open non-blocking an unconnected pipe for writing.
      pass
    fifo_path.unlink(missing_ok=True)


class ServerStartedTest(unittest.TestCase):
  build_id_counter = 0
  task_name_counter = 0

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._tty_path = None
    self._build_id = None

  def setUp(self):
    if shouldSkip():
      self.skipTest("Cannot run test when server already running.")
    self._process = subprocess.Popen(
        [server_utils.SERVER_SCRIPT.absolute(), '--exit-on-idle', '--quiet'],
        start_new_session=True,
        cwd=pathlib.Path(__file__).parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True)
    # pylint: disable=unused-variable
    for attempt in range(5):
      if pollServer():
        break
      time.sleep(0.05)

  def tearDown(self):
    self._process.terminate()
    stdout, _ = self._process.communicate()
    if stdout != '':
      self.fail(f'build server should be silent but it output:\n{stdout}')

  @contextlib.contextmanager
  def _register_build(self):
    with tempfile.NamedTemporaryFile() as f:
      build_id = f'BUILD_ID_{ServerStartedTest.build_id_counter}'
      os.environ['AUTONINJA_BUILD_ID'] = build_id
      os.environ['AUTONINJA_STDOUT_NAME'] = f.name
      ServerStartedTest.build_id_counter += 1
      build_proc = subprocess.Popen(
          [sys.executable, '-c', 'import time; time.sleep(100)'])
      callServer(
          ['--register-build', build_id, '--builder-pid',
           str(build_proc.pid)])
      self._tty_path = f.name
      self._build_id = build_id
      try:
        yield
      finally:
        self._tty_path = None
        self._build_id = None
        del os.environ['AUTONINJA_BUILD_ID']
        del os.environ['AUTONINJA_STDOUT_NAME']
        build_proc.kill()
        build_proc.wait()

  def sendTask(self, cmd, stamp_path=None):
    if stamp_path:
      _stamp_file = pathlib.Path(stamp_path)
    else:
      _stamp_file = pathlib.Path('/tmp/.test.stamp')
    _stamp_file.touch()

    name_prefix = f'{self._build_id}-{ServerStartedTest.task_name_counter}'
    sendMessage({
        'name': f'{name_prefix}: {" ".join(cmd)}',
        'message_type': server_utils.ADD_TASK,
        'cmd': cmd,
        # So that logfiles do not clutter cwd.
        'cwd': '/tmp/',
        'build_id': self._build_id,
        'stamp_file': _stamp_file.name,
    })
    ServerStartedTest.task_name_counter += 1

  def getTtyContents(self):
    return pathlib.Path(self._tty_path).read_text()

  def getBuildInfo(self):
    build_info = server.query_build_info(self._build_id)['builds'][0]
    pending_tasks = build_info['pending_tasks']
    completed_tasks = build_info['completed_tasks']
    return pending_tasks, completed_tasks

  def waitForTasksDone(self, timeout_seconds=3):
    timeout_duration = datetime.timedelta(seconds=timeout_seconds)
    start_time = datetime.datetime.now()
    while True:
      pending_tasks, completed_tasks = self.getBuildInfo()

      if completed_tasks > 0 and pending_tasks == 0:
        return

      current_time = datetime.datetime.now()
      duration = current_time - start_time
      if duration > timeout_duration:
        raise TimeoutError('Timed out waiting for pending tasks ' +
                           f'[{pending_tasks}/{pending_tasks+completed_tasks}]')
      time.sleep(0.1)

  def testRunsQuietTask(self):
    with self._register_build():
      self.sendTask(['true'])
      self.waitForTasksDone()
      self.assertEqual(self.getTtyContents(), '')

  def testRunsNoisyTask(self):
    with self._register_build():
      self.sendTask(['echo', 'some_output'])
      self.waitForTasksDone()
      tty_contents = self.getTtyContents()
      self.assertIn('some_output', tty_contents)

  def testStampFileDeletedOnFailedTask(self):
    with self._register_build():
      stamp_file = pathlib.Path('/tmp/.failed_task.stamp')
      self.sendTask(['echo', 'some_output'], stamp_path=stamp_file)
      self.waitForTasksDone()
      self.assertFalse(stamp_file.exists())

  def testStampFileNotDeletedOnSuccess(self):
    with self._register_build():
      stamp_file = pathlib.Path('/tmp/.successful_task.stamp')
      self.sendTask(['true'], stamp_path=stamp_file)
      self.waitForTasksDone()
      self.assertTrue(stamp_file.exists())

  def testWaitForBuildServerCall(self):
    with self._register_build():
      callServer(['--wait-for-build', self._build_id])
      self.assertEqual(self.getTtyContents(), '')

  def testWaitForIdleServerCall(self):
    with self._register_build():
      self.sendTask(['true'])
      proc_result = callServer(['--wait-for-idle'])
      self.assertIn('All', proc_result.stdout)
      self.assertEqual('', self.getTtyContents())

  def testCancelBuildServerCall(self):
    with self._register_build():
      callServer(['--cancel-build', self._build_id])
      self.assertEqual(self.getTtyContents(), '')

  def testBuildStatusServerCall(self):
    with self._register_build():
      proc_result = callServer(['--print-status', self._build_id])
      self.assertEqual(proc_result.stdout, '')

      proc_result = callServer(['--print-status-all'])
      self.assertIn(self._build_id, proc_result.stdout)

      self.sendTask(['true'])
      self.waitForTasksDone()

      proc_result = callServer(['--print-status', self._build_id])
      self.assertEqual('', proc_result.stdout)

      proc_result = callServer(['--print-status-all'])
      self.assertIn('has 1 registered build', proc_result.stdout)
      self.assertIn('[1/1]', proc_result.stdout)

      with blockingFifo() as fifo_path:
        # cat gets stuck until we open the other end of the fifo.
        self.sendTask(['cat', str(fifo_path)])
        proc_result = callServer(['--print-status', self._build_id])
        self.assertIn('is still 1 static analysis job', proc_result.stdout)
        self.assertIn('--wait-for-idle', proc_result.stdout)
        proc_result = callServer(['--print-status-all'])
        self.assertIn('[1/2]', proc_result.stdout)

      self.waitForTasksDone()
      callServer(['--cancel-build', self._build_id])
      self.waitForTasksDone()
      proc_result = callServer(['--print-status', self._build_id])
      self.assertEqual('', proc_result.stdout)

    proc_result = callServer(['--print-status-all'])
    self.assertIn('Siso finished', proc_result.stdout)

  def testServerCancelsRunningTasks(self):
    output_stamp = pathlib.Path('/tmp/.deleteme.stamp')
    with blockingFifo() as fifo_path:
      self.assertFalse(output_stamp.exists())
      # dd blocks on fifo so task never finishes inside with block.
      with self._register_build():
        self.sendTask(['dd', f'if={str(fifo_path)}', f'of={str(output_stamp)}'])
        callServer(['--cancel-build', self._build_id])
        self.waitForTasksDone()
    self.assertFalse(output_stamp.exists())

  def testKeyboardInterrupt(self):
    os.kill(self._process.pid, signal.SIGINT)
    self._process.wait(timeout=1)


class ServerNotStartedTest(unittest.TestCase):

  def setUp(self):
    if pollServer():
      self.skipTest("Cannot run test when server already running.")

  def testWaitForBuildServerCall(self):
    proc_result = callServer(['--wait-for-build', 'invalid-build-id'])
    self.assertIn('No server running', proc_result.stdout)

  def testBuildStatusServerCall(self):
    proc_result = callServer(['--print-status-all'])
    self.assertIn('No server running', proc_result.stdout)


if __name__ == '__main__':
  # Suppress logging messages.
  unittest.main(buffer=True)
