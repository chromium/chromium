#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an server to offload non-critical-path GN targets."""

import argparse
import dataclasses
import json
import os
import shutil
import socket
import subprocess
import sys
import threading
from typing import Dict, List, Optional, Tuple

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils


class Logger:
  """Class to store global state for logging."""
  num_processes: int = 0
  completed_tasks: int = 0
  total_tasks: int = 0

  @classmethod
  def _plural(cls, word: str, num: int, suffix: str = 's'):
    if num == 1:
      return word
    return word + suffix

  @classmethod
  def _prefix(cls):
    # Ninja's prefix is: [205 processes, 6/734 @ 6.5/s : 0.922s ]
    # Time taken and task completion rate are not important for the build server
    # since it is always running in the background and uses idle priority for
    # its tasks.
    processes_str = cls._plural('process', cls.num_processes, suffix='es')
    return (f'{cls.num_processes} {processes_str}, '
            f'{cls.completed_tasks}/{cls.total_tasks}')

  @classmethod
  def log(cls, msg: str, *, end: str = ''):
    # Shrink the message (leaving a 2-char prefix and use the rest of the room
    # for the suffix) according to terminal size so it is always one line.
    width = shutil.get_terminal_size().columns
    prefix = f'[{cls._prefix()}] '
    max_msg_width = width - len(prefix)
    if len(msg) > max_msg_width:
      length_to_show = max_msg_width - 5  # Account for ellipsis and header.
      msg = f'{msg[:2]}...{msg[-length_to_show:]}'
    # \r to return the carriage to the beginning of line.
    # \033[K to replace the normal \n to erase until the end of the line.
    # Avoid the default line ending so the next \r overwrites the same line just
    #     like ninja's output.
    print(f'\r{prefix}{msg}\033[K', end=end, flush=True)


@dataclasses.dataclass
class Task:
  """Class to represent a single build task."""
  name: str
  cwd: str
  cmd: List[str]
  stamp_file: str
  _proc: Optional[subprocess.Popen] = None
  _thread: Optional[threading.Thread] = None
  _terminated: bool = False
  _return_code: Optional[int] = None

  @property
  def key(self):
    return (self.cwd, self.name)

  def start(self):
    assert self._proc is None
    Logger.num_processes += 1
    Logger.log(f'STARTING {self.name}')
    # The environment variable forces the script to actually run in order to
    # avoid infinite recursion.
    env = os.environ.copy()
    env[server_utils.BUILD_SERVER_ENV_VARIABLE] = '1'
    # Use os.nice(19) to ensure the lowest priority (idle) for these analysis
    # tasks since we want to avoid slowing down the actual build.
    # TODO(wnwen): Also use ionice to reduce resource consumption. Possibly use
    #              cgroups to make these processes use even fewer resources than
    #              idle priority.
    self._proc = subprocess.Popen(
        self.cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=self.cwd,
        env=env,
        text=True,
        preexec_fn=lambda: os.nice(19),
    )
    # Avoid daemon=True to allow threads to finish running cleanup on Ctrl-C.
    self._thread = threading.Thread(target=self._complete_when_process_finishes)
    self._thread.start()

  def terminate(self):
    if self._terminated:
      return
    self._terminated = True
    if self._proc:
      self._proc.terminate()
      self._proc.wait()
    if self._thread:
      self._thread.join()

  def _complete_when_process_finishes(self):
    assert self._proc
    # We know Popen.communicate will return a str and not a byte since it is
    # constructed with text=True.
    stdout: str = self._proc.communicate()[0]
    self._return_code = self._proc.returncode
    self._proc = None
    self._complete(stdout)

  def _complete(self, stdout: str):
    assert self._proc is None
    Logger.completed_tasks += 1
    Logger.num_processes -= 1
    failed = False
    if self._terminated:
      Logger.log(f'TERMINATED {self.name}')
      # Ignore stdout as it is now outdated.
      failed = True
    else:
      Logger.log(f'FINISHED {self.name}')
      if stdout or self._return_code != 0:
        failed = True
        # An extra new line is needed since _log does not end with a new line.
        print(f'\nFAILED: {self.name} Return code: {self._return_code}')
        print(' '.join(self.cmd))
        print(stdout)

    if failed:
      # Force ninja to consider failed targets as dirty.
      try:
        os.unlink(os.path.join(self.cwd, self.stamp_file))
      except FileNotFoundError:
        pass
    else:
      # Ninja will rebuild targets when their inputs change even if their stamp
      # file has a later modified time. Thus we do not need to worry about the
      # script being run by the build server updating the mtime incorrectly.
      pass


def _listen_for_request_data(sock: socket.socket):
  while True:
    conn = sock.accept()[0]
    received = []
    with conn:
      while True:
        data = conn.recv(4096)
        if not data:
          break
        received.append(data)
    if received:
      yield json.loads(b''.join(received))


def _process_requests(sock: socket.socket):
  # Since dicts in python can contain anything, explicitly type tasks to help
  # make static type checking more useful.
  tasks: Dict[Tuple[str, str], Task] = {}
  try:
    for data in _listen_for_request_data(sock):
      task = Task(name=data['name'],
                  cwd=data['cwd'],
                  cmd=data['cmd'],
                  stamp_file=data['stamp_file'])
      Logger.total_tasks += 1
      existing_task = tasks.get(task.key)
      if existing_task:
        existing_task.terminate()
      tasks[task.key] = task
      # TODO(wnwen): Rather than start it right away, add this task to a running
      #              queue and run either a limited number of processes (10) or
      #              even just 1 until the server load is very low (or ninja has
      #              finished).
      task.start()
  except KeyboardInterrupt:
    Logger.log('STOPPING SERVER...', end='\n')
    # Gracefully exit by terminating all running tasks and allowing their io
    # watcher threads to finish and run cleanup on their own.
    for task in tasks.values():
      task.terminate()
    Logger.log('STOPPED', end='\n')


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.parse_args()
  with socket.socket(socket.AF_UNIX) as sock:
    sock.bind(server_utils.SOCKET_ADDRESS)
    sock.listen()
    _process_requests(sock)


if __name__ == '__main__':
  sys.exit(main())
