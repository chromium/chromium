#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an server to offload non-critical-path GN targets."""

import argparse
import dataclasses
import json
import logging
import os
import socket
import subprocess
import sys
import threading
import time

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils

# TODO(wnwen): Add type annotations.


@dataclasses.dataclass
class Task:
  """Class to represent a single build task."""
  name: str
  stamp_path: str
  proc: subprocess.Popen
  terminated: bool = False

  def is_running(self):
    return self.proc.poll() is None


def _log(msg):
  # Subtract one from the total number of threads so we don't count the main
  #     thread.
  num = threading.active_count() - 1
  # \r to return the carriage to the beginning of line.
  # \033[K to replace the normal \n to erase until the end of the line.
  # Avoid the default line ending so the next \r overwrites the same line just
  #     like ninja's output.
  # TODO(wnwen): When there is just one thread left and it finishes, the last
  #              output is "[1 thread] FINISHED //...". It may be better to show
  #              "ALL DONE" or something to that effect.
  print(f'\r[{num} thread{"" if num == 1 else "s"}] {msg}\033[K', end='')


def _run_when_completed(task):
  _log(f'RUNNING {task.name}')
  stdout, _ = task.proc.communicate()

  # Avoid printing anything since the task is now outdated.
  if task.terminated:
    return

  _log(f'FINISHED {task.name}')
  if stdout:
    # An extra new line is needed since _log does not end with a new line.
    print(f'\nFAILED: {task.name}')
    print(' '.join(task.proc.args))
    print(stdout.decode('utf-8'))
    # Force ninja to always re-run failed tasks.
    try:
      os.unlink(task.stamp_path)
    except FileNotFoundError:
      pass
  # TODO(wnwen): Reset timestamp for stamp file to when the original request was
  #              sent since otherwise if a file is edited while the task is
  #              running, then the target would appear newer than the edit.


def _init_task(*, name, cwd, cmd, stamp_file):
  _log(f'STARTING {name}')
  # The environment variable forces the script to actually run in order to avoid
  # infinite recursion.
  env = os.environ.copy()
  env[server_utils.BUILD_SERVER_ENV_VARIABLE] = '1'
  # Use os.nice(19) to ensure the lowest priority (idle) for these analysis
  # tasks since we want to avoid slowing down the actual build.
  proc = subprocess.Popen(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,  # Interleave outputs to match running locally.
      cwd=cwd,
      env=env,
      preexec_fn=lambda: os.nice(19),
  )
  task = Task(name=name, stamp_path=os.path.join(cwd, stamp_file), proc=proc)
  # Set daemon=True so that one Ctrl-C terminates the server.
  # TODO(wnwen): Handle Ctrl-C and updating stamp files before exiting.
  io_thread = threading.Thread(target=_run_when_completed,
                               args=(task, ),
                               daemon=True)
  io_thread.start()
  return task


def _listen_for_request_data(sock):
  while True:
    conn, _ = sock.accept()
    received = []
    with conn:
      while True:
        data = conn.recv(4096)
        if not data:
          break
        received.append(data)
    if received:
      yield json.loads(b''.join(received))


def _terminate_task(task):
  task.terminated = True
  task.proc.terminate()
  task.proc.wait()
  _log(f'TERMINATED {task.name}')


def _process_requests(sock):
  tasks = {}
  # TODO(wnwen): Record and update start_time whenever we go from 0 to 1 active
  #              threads so logging can display a reasonable time, e.g.
  #              "[2 threads : 32.553s ] RUNNING //..."
  for data in _listen_for_request_data(sock):
    key = (data['name'], data['cwd'])
    task = tasks.get(key)
    if task and task.is_running():
      _terminate_task(task)
    tasks[key] = _init_task(**data)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  args = parser.parse_args()
  with socket.socket(socket.AF_UNIX) as sock:
    sock.bind(server_utils.SOCKET_ADDRESS)
    sock.listen()
    _process_requests(sock)


if __name__ == '__main__':
  sys.exit(main())
