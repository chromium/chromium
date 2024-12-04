#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import datetime
import json
import pathlib
import unittest
import os
import signal
import socket
import subprocess
import sys
import time
import uuid

import fast_local_dev_server as server

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils


class RegexTest(unittest.TestCase):

  def testBuildIdRegex(self):
    self.assertRegex(server.FIRST_LOG_LINE.format(build_id='abc'),
                     server.BUILD_ID_RE)


def sendMessage(message_dict):
  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    sock.settimeout(1)
    sock.connect(server_utils.SOCKET_ADDRESS)
    server_utils.SendMessage(sock, json.dumps(message_dict).encode('utf-8'))


def pollServer():
  try:
    sendMessage({'message_type': server_utils.POLL_HEARTBEAT})
    return True
  except ConnectionRefusedError:
    return False


def callServer(args, stdout=subprocess.DEVNULL):
  return subprocess.check_call([server_utils.SERVER_SCRIPT.absolute()] + args,
                               cwd=pathlib.Path(__file__).parent,
                               stdout=stdout)


class TasksTest(unittest.TestCase):

  def setUp(self):
    self._TTY_FILE = '/tmp/fast_local_dev_server_test_tty'
    if pollServer():
      # TODO(mheikal): Support overriding the standard named pipe for
      # communicating with the server so that we can run an instance just for
      # this test even if a real one is running.
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
    if os.path.exists(self._TTY_FILE):
      os.unlink(self._TTY_FILE)
    self._process.terminate()
    stdout, _ = self._process.communicate()
    if stdout != '':
      self.fail(f'build server should be silent but it output:\n{stdout}')

  def sendTask(self, cmd, stamp_path=None):
    if stamp_path:
      _stamp_file = pathlib.Path(stamp_path)
    else:
      _stamp_file = pathlib.Path('/tmp/.test.stamp')
    _stamp_file.touch()

    sendMessage({
        'name': f'test task {uuid.uuid4()}',
        'message_type': server_utils.ADD_TASK,
        'cmd': cmd,
        # So that logfiles do not clutter cwd.
        'cwd': '/tmp/',
        'tty': self._TTY_FILE,
        'build_id': self.id(),
        'experimental': True,
        'stamp_file': _stamp_file.name,
    })

  def getTtyContents(self):
    if os.path.exists(self._TTY_FILE):
      with open(self._TTY_FILE, 'rt') as tty:
        return tty.read()
    return ''

  def getBuildInfo(self):
    build_info = server.query_build_info(self.id())
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
        raise TimeoutError()
      time.sleep(0.1)

  def testRunsQuietTask(self):
    self.sendTask(['true'])
    self.waitForTasksDone()
    self.assertEqual(self.getTtyContents(), '')

  def testRunsNoisyTask(self):
    self.sendTask(['echo', 'some_output'])
    self.waitForTasksDone()
    tty_contents = self.getTtyContents()
    self.assertIn('some_output', tty_contents)

  def testStampFileDeletedOnFailedTask(self):
    stamp_file = pathlib.Path('/tmp/.failed_task.stamp')
    self.sendTask(['echo', 'some_output'], stamp_path=stamp_file)
    self.waitForTasksDone()
    self.assertFalse(stamp_file.exists())

  def testStampFileNotDeletedOnSuccess(self):
    stamp_file = pathlib.Path('/tmp/.successful_task.stamp')
    self.sendTask(['true'], stamp_path=stamp_file)
    self.waitForTasksDone()
    self.assertTrue(stamp_file.exists())

  def testRegisterBuilderMessage(self):
    sendMessage({
        'message_type': server_utils.REGISTER_BUILDER,
        'build_id': self.id(),
        'builder_pid': os.getpid(),
    })
    pollServer()
    self.assertEqual(self.getTtyContents(), '')

  def testRegisterBuilderServerCall(self):
    self.assertEqual(
        callServer(
            ['--register-build',
             self.id(), '--builder-pid',
             str(os.getpid())]), 0)
    self.assertEqual(self.getTtyContents(), '')

  def testWaitForBuildServerCall(self):
    self.assertEqual(callServer(['--wait-for-build', self.id()]), 0)
    self.assertEqual(self.getTtyContents(), '')

  def testCancelBuildServerCall(self):
    self.assertEqual(callServer(['--cancel-build', self.id()]), 0)
    self.assertEqual(self.getTtyContents(), '')

  def testKeyboardInterrupt(self):
    os.kill(self._process.pid, signal.SIGINT)
    self._process.wait(timeout=1)


if __name__ == '__main__':
  # Suppress logging messages.
  unittest.main(buffer=True)
