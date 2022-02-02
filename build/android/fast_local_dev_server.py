#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an server to offload non-critical-path GN targets."""

from __future__ import annotations

import argparse
import json
import os
import queue
import shutil
import socket
import subprocess
import sys
import threading
from typing import Callable, Dict, List, Optional, Tuple

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils


def log(msg: str, *, end: str = ''):
  # Shrink the message (leaving a 2-char prefix and use the rest of the room
  # for the suffix) according to terminal size so it is always one line.
  width = shutil.get_terminal_size().columns
  prefix = f'[{TaskStats.prefix()}] '
  max_msg_width = width - len(prefix)
  if len(msg) > max_msg_width:
    length_to_show = max_msg_width - 5  # Account for ellipsis and header.
    msg = f'{msg[:2]}...{msg[-length_to_show:]}'
  # \r to return the carriage to the beginning of line.
  # \033[K to replace the normal \n to erase until the end of the line.
  # Avoid the default line ending so the next \r overwrites the same line just
  #     like ninja's output.
  print(f'\r{prefix}{msg}\033[K', end=end, flush=True)


class TaskStats:
  """Class to keep track of aggregate stats for all tasks across threads."""
  _num_processes = 0
  _completed_tasks = 0
  _total_tasks = 0
  _lock = threading.Lock()

  @classmethod
  def no_running_processes(cls):
    return cls._num_processes == 0

  @classmethod
  def add_task(cls):
    # Only the main thread calls this, so there is no need for locking.
    cls._total_tasks += 1

  @classmethod
  def add_process(cls):
    with cls._lock:
      cls._num_processes += 1

  @classmethod
  def remove_process(cls):
    with cls._lock:
      cls._num_processes -= 1

  @classmethod
  def complete_task(cls):
    with cls._lock:
      cls._completed_tasks += 1

  @classmethod
  def prefix(cls):
    # Ninja's prefix is: [205 processes, 6/734 @ 6.5/s : 0.922s ]
    # Time taken and task completion rate are not important for the build server
    # since it is always running in the background and uses idle priority for
    # its tasks.
    with cls._lock:
      word = 'process' if cls._num_processes == 1 else 'processes'
      return (f'{cls._num_processes} {word}, '
              f'{cls._completed_tasks}/{cls._total_tasks}')


class TaskManager:
  """Class to encapsulate a threadsafe queue and handle deactivating it."""

  def __init__(self):
    self._queue: queue.SimpleQueue[Task] = queue.SimpleQueue()
    self._deactivated = False

  def add_task(self, task: Task):
    assert not self._deactivated
    TaskStats.add_task()
    self._queue.put(task)
    log(f'QUEUED {task.name}')
    self._maybe_start_tasks()

  def deactivate(self):
    self._deactivated = True
    while not self._queue.empty():
      try:
        task = self._queue.get_nowait()
      except queue.Empty:
        return
      task.terminate()

  @staticmethod
  def _num_running_processes():
    with open('/proc/stat') as f:
      for line in f:
        if line.startswith('procs_running'):
          return int(line.rstrip().split()[1])
    assert False, 'Could not read /proc/stat'

  def _maybe_start_tasks(self):
    if self._deactivated:
      return
    # Include load avg so that a small dip in the number of currently running
    # processes will not cause new tasks to be started while the overall load is
    # heavy.
    cur_load = max(self._num_running_processes(), os.getloadavg()[0])
    num_started = 0
    # Always start a task if we don't have any running, so that all tasks are
    # eventually finished. Try starting up tasks when the overall load is light.
    # Limit to at most 2 new tasks to prevent ramping up too fast. There is a
    # chance where multiple threads call _maybe_start_tasks and each gets to
    # spawn up to 2 new tasks, but since the only downside is some build tasks
    # get worked on earlier rather than later, it is not worth mitigating.
    while num_started < 2 and (TaskStats.no_running_processes()
                               or num_started + cur_load < os.cpu_count()):
      try:
        next_task = self._queue.get_nowait()
      except queue.Empty:
        return
      num_started += next_task.start(self._maybe_start_tasks)


# TODO(wnwen): Break this into Request (encapsulating what ninja sends) and Task
#              when a Request starts to be run. This would eliminate ambiguity
#              about when and whether _proc/_thread are initialized.
class Task:
  """Class to represent one task and operations on it."""

  def __init__(self, name: str, cwd: str, cmd: List[str], stamp_file: str):
    self.name = name
    self.cwd = cwd
    self.cmd = cmd
    self.stamp_file = stamp_file
    self._terminated = False
    self._lock = threading.Lock()
    self._proc: Optional[subprocess.Popen] = None
    self._thread: Optional[threading.Thread] = None
    self._return_code: Optional[int] = None

  @property
  def key(self):
    return (self.cwd, self.name)

  def start(self, on_complete_callback: Callable[[], None]) -> int:
    """Starts the task if it has not already been terminated.

    Returns the number of processes that have been started. This is called at
    most once when the task is popped off the task queue."""

    # The environment variable forces the script to actually run in order to
    # avoid infinite recursion.
    env = os.environ.copy()
    env[server_utils.BUILD_SERVER_ENV_VARIABLE] = '1'

    with self._lock:
      if self._terminated:
        return 0
      # Use os.nice(19) to ensure the lowest priority (idle) for these analysis
      # tasks since we want to avoid slowing down the actual build.
      # TODO(wnwen): Use ionice to reduce resource consumption.
      TaskStats.add_process()
      log(f'STARTING {self.name}')
      self._proc = subprocess.Popen(
          self.cmd,
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          cwd=self.cwd,
          env=env,
          text=True,
          preexec_fn=lambda: os.nice(19),
      )
      self._thread = threading.Thread(
          target=self._complete_when_process_finishes,
          args=(on_complete_callback, ))
      self._thread.start()
      return 1

  def terminate(self):
    """Can be called multiple times to cancel and ignore the task's output."""

    with self._lock:
      if self._terminated:
        return
      self._terminated = True
    # It is safe to access _proc and _thread outside of _lock since they are
    # only changed by self.start holding _lock when self._terminate is false.
    # Since we have just set self._terminate to true inside of _lock, we know
    # that neither _proc nor _thread will be changed from this point onwards.
    if self._proc:
      self._proc.terminate()
      self._proc.wait()
    # Ensure that self._complete is called either by the thread or by us.
    if self._thread:
      self._thread.join()
    else:
      self._complete()

  def _complete_when_process_finishes(self,
                                      on_complete_callback: Callable[[], None]):
    assert self._proc
    # We know Popen.communicate will return a str and not a byte since it is
    # constructed with text=True.
    stdout: str = self._proc.communicate()[0]
    self._return_code = self._proc.returncode
    TaskStats.remove_process()
    self._complete(stdout)
    on_complete_callback()

  def _complete(self, stdout: str = ''):
    """Update the user and ninja after the task has run or been terminated.

    This method should only be run once per task. Avoid modifying the task so
    that this method does not need locking."""

    TaskStats.complete_task()
    failed = False
    if self._terminated:
      log(f'TERMINATED {self.name}')
      # Ignore stdout as it is now outdated.
      failed = True
    else:
      log(f'FINISHED {self.name}')
      if stdout or self._return_code != 0:
        failed = True
        # An extra new line is needed since we want to preserve the previous
        # _log line. Use a single print so that it is threadsafe.
        # TODO(wnwen): Improve stdout display by parsing over it and moving the
        #              actual error to the bottom. Otherwise long command lines
        #              in the Traceback section obscure the actual error(s).
        print('\n' + '\n'.join([
            f'FAILED: {self.name}',
            f'Return code: {self._return_code}',
            ' '.join(self.cmd),
            stdout,
        ]))

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
  task_manager = TaskManager()
  try:
    for data in _listen_for_request_data(sock):
      task = Task(name=data['name'],
                  cwd=data['cwd'],
                  cmd=data['cmd'],
                  stamp_file=data['stamp_file'])
      existing_task = tasks.get(task.key)
      if existing_task:
        existing_task.terminate()
      tasks[task.key] = task
      task_manager.add_task(task)
  except KeyboardInterrupt:
    log('STOPPING SERVER...', end='\n')
    # Gracefully shut down the task manager, terminating all queued tasks.
    task_manager.deactivate()
    # Terminate all currently running tasks.
    for task in tasks.values():
      task.terminate()
    log('STOPPED', end='\n')


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.parse_args()
  with socket.socket(socket.AF_UNIX) as sock:
    sock.bind(server_utils.SOCKET_ADDRESS)
    sock.listen()
    _process_requests(sock)


if __name__ == '__main__':
  sys.exit(main())
