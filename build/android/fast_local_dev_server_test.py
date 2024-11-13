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


class TasksTest(unittest.TestCase):

  def setUp(self):
    self._TTY_FILE = '/tmp/fast_local_dev_server_test_tty'
    try:
      sendMessage({'message_type': server_utils.POLL_HEARTBEAT})
      # TODO(mheikal): Support overriding the standard named pipe for
      # communicating with the server so that we can run an instance just for
      # this test even if a real one is running.
      self.skipTest("Cannot run test when server already running.")
    except ConnectionRefusedError:
      pass
    self._process = subprocess.Popen(
        [server_utils.SERVER_SCRIPT.absolute(), '--exit-on-idle', '--quiet'],
        start_new_session=True,
        cwd=pathlib.Path(__file__).parent)
    # pylint: disable=unused-variable
    for attempt in range(5):
      try:
        sendMessage({'message_type': server_utils.POLL_HEARTBEAT})
        break
      except ConnectionRefusedError:
        # Wait for server init.
        time.sleep(0.05)

  def tearDown(self):
    if os.path.exists(self._TTY_FILE):
      with open(self._TTY_FILE, 'rt') as tty:
        contents = tty.read()
        # TTY should only be written to if the server crashes which is probably
        # unexpected.
        if contents:
          self.fail('Found non-empty tty:\n' + repr(contents))
      os.unlink(self._TTY_FILE)
    self._process.terminate()
    self._process.wait()

  def sendTask(self, cmd):
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

  def getBuildInfo(self):
    build_info = server.query_build_info(self.id())
    pending_tasks = build_info['pending_tasks']
    completed_tasks = build_info['completed_tasks']
    pending_outputs = build_info['pending_outputs']
    return pending_tasks, completed_tasks, pending_outputs

  def waitForTasksDone(self, timeout_seconds=3):
    timeout_duration = datetime.timedelta(seconds=timeout_seconds)
    start_time = datetime.datetime.now()
    all_pending_outputs = []
    while True:
      pending_tasks, completed_tasks, pending_outputs = self.getBuildInfo()
      all_pending_outputs.extend(pending_outputs)

      if completed_tasks > 0 and pending_tasks == 0:
        return all_pending_outputs

      current_time = datetime.datetime.now()
      duration = current_time - start_time
      if duration > timeout_duration:
        raise TimeoutError()
      time.sleep(0.1)

  def testRunsQuietTask(self):
    self.sendTask(['true'])
    pending_outputs = self.waitForTasksDone()
    self.assertEqual(len(pending_outputs), 0)

  def testRunsNoisyTask(self):
    self.sendTask(['echo', 'some_output'])
    pending_outputs = self.waitForTasksDone()
    self.assertEqual(len(pending_outputs), 1)
    self.assertIn('some_output', pending_outputs[0])


if __name__ == '__main__':
  # Suppress logging messages.
  unittest.main(buffer=True)
