#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an server to offload non-critical-path GN targets."""

from __future__ import annotations

import argparse
import collections
import contextlib
import datetime
import json
import os
import pathlib
import queue
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
from typing import Callable, Dict, List, Optional, Tuple, IO

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils

_SOCKET_TIMEOUT = 30  # seconds

_LOGFILES = {}
_LOGFILE_NAME = 'buildserver.log'
_MAX_LOGFILES = 6

FIRST_LOG_LINE = '#### Start of log for build_id = {build_id} ####\n'
BUILD_ID_RE = re.compile(r'^#### .*build_id = (?P<build_id>.+) ####')


def log(msg: str, *, end: str = '', quiet: bool = False, build_id: str = None):
  prefix = f'[{TaskStats.prefix()}] '
  # if message is specific to a build then also output to its logfile.
  if build_id:
    log_to_file(f'{prefix}{msg}', build_id=build_id)

  # No need to also output to the terminal if quiet.
  if quiet:
    return
  # Shrink the message (leaving a 2-char prefix and use the rest of the room
  # for the suffix) according to terminal size so it is always one line.
  width = shutil.get_terminal_size().columns
  max_msg_width = width - len(prefix)
  if len(msg) > max_msg_width:
    length_to_show = max_msg_width - 5  # Account for ellipsis and header.
    msg = f'{msg[:2]}...{msg[-length_to_show:]}'
  # \r to return the carriage to the beginning of line.
  # \033[K to replace the normal \n to erase until the end of the line.
  # Avoid the default line ending so the next \r overwrites the same line just
  #     like ninja's output.
  print(f'\r{prefix}{msg}\033[K', end=end, flush=True)


def log_to_file(message: str, build_id: str):
  logfile = _LOGFILES.get(build_id)
  print(message, file=logfile, flush=True)


def create_logfile(build_id, outdir):
  if logfile := _LOGFILES.get(build_id, None):
    return logfile

  outdir = pathlib.Path(outdir)
  latest_logfile = outdir / f'{_LOGFILE_NAME}.0'

  if latest_logfile.exists():
    with latest_logfile.open('rt') as f:
      first_line = f.readline()
      if log_build_id := BUILD_ID_RE.search(first_line):
        # If the newest logfile on disk is referencing the same build we are
        # currently processing, we probably crashed previously and we should
        # pick up where we left off in the same logfile.
        if log_build_id.group('build_id') == build_id:
          _LOGFILES[build_id] = latest_logfile.open('at')
          return _LOGFILES[build_id]

  # Do the logfile name shift.
  filenames = os.listdir(outdir)
  logfiles = {f for f in filenames if f.startswith(_LOGFILE_NAME)}
  for idx in reversed(range(_MAX_LOGFILES)):
    current_name = f'{_LOGFILE_NAME}.{idx}'
    next_name = f'{_LOGFILE_NAME}.{idx+1}'
    if current_name in logfiles:
      shutil.move(os.path.join(outdir, current_name),
                  os.path.join(outdir, next_name))

  # Create a new 0th logfile.
  logfile = latest_logfile.open('wt')
  _LOGFILES[build_id] = logfile
  logfile.write(FIRST_LOG_LINE.format(build_id=build_id))
  logfile.flush()
  return logfile


class QueuedOutputs:
  """Class to store outputs for completed tasks until autoninja asks for them."""
  _lock = threading.Lock()
  _pending_outputs = collections.defaultdict(list)
  _output_pipes = {}

  @classmethod
  def add_output(cls, task: Task, output_str: str):
    with cls._lock:
      build_id = task.build_id
      cls._pending_outputs[build_id].append(output_str)
      # All ttys should be the same.
      if task.tty:
        cls._output_pipes[build_id] = task.tty

  @classmethod
  def get_pending_outputs(cls, build_id: str):
    with cls._lock:
      pending_outputs = cls._pending_outputs[build_id]
      cls._pending_outputs[build_id] = []
      return pending_outputs

  @classmethod
  def flush_messages(cls):
    with cls._lock:
      for build_id, messages in cls._pending_outputs.items():
        if messages:
          pipe = cls._output_pipes.get(build_id)
          if pipe:
            pipe.write('\nfast_local_dev_server.py shutting down with queued ' +
                       'task outputs. Flushing now:\n')
            for message in messages:
              pipe.write(message + '\n')
      cls._pending_outputs = collections.defaultdict(list)


class TaskStats:
  """Class to keep track of aggregate stats for all tasks across threads."""
  _num_processes = 0
  _completed_tasks = 0
  _total_tasks = 0
  _total_task_count_per_build = collections.defaultdict(int)
  _completed_task_count_per_build = collections.defaultdict(int)
  _running_processes_count_per_build = collections.defaultdict(int)
  _lock = threading.Lock()

  @classmethod
  def no_running_processes(cls):
    with cls._lock:
      return cls._num_processes == 0

  @classmethod
  def add_task(cls, build_id: str):
    with cls._lock:
      cls._total_tasks += 1
      cls._total_task_count_per_build[build_id] += 1

  @classmethod
  def add_process(cls, build_id: str):
    with cls._lock:
      cls._num_processes += 1
      cls._running_processes_count_per_build[build_id] += 1

  @classmethod
  def remove_process(cls, build_id: str):
    with cls._lock:
      cls._num_processes -= 1
      cls._running_processes_count_per_build[build_id] -= 1

  @classmethod
  def complete_task(cls, build_id: str):
    with cls._lock:
      cls._completed_tasks += 1
      cls._completed_task_count_per_build[build_id] += 1

  @classmethod
  def num_pending_tasks(cls, build_id: str = None):
    with cls._lock:
      if build_id:
        return cls._total_task_count_per_build[
            build_id] - cls._completed_task_count_per_build[build_id]
      return cls._total_tasks - cls._completed_tasks

  @classmethod
  def num_completed_tasks(cls, build_id: str = None):
    with cls._lock:
      if build_id:
        return cls._completed_task_count_per_build[build_id]
      return cls._completed_tasks

  @classmethod
  def prefix(cls, build_id: str = None):
    # Ninja's prefix is: [205 processes, 6/734 @ 6.5/s : 0.922s ]
    # Time taken and task completion rate are not important for the build server
    # since it is always running in the background and uses idle priority for
    # its tasks.
    with cls._lock:
      if build_id:
        _num_processes = cls._running_processes_count_per_build[build_id]
        _completed_tasks = cls._completed_task_count_per_build[build_id]
        _total_tasks = cls._total_task_count_per_build[build_id]
      else:
        _num_processes = cls._num_processes
        _completed_tasks = cls._completed_tasks
        _total_tasks = cls._total_tasks
      word = 'process' if _num_processes == 1 else 'processes'
      return (f'{_num_processes} {word}, '
              f'{_completed_tasks}/{_total_tasks}')


class TaskManager:
  """Class to encapsulate a threadsafe queue and handle deactivating it."""
  _queue: queue.SimpleQueue[Task] = queue.SimpleQueue()
  _deactivated = False

  @classmethod
  def add_task(cls, task: Task, options):
    assert not cls._deactivated
    TaskStats.add_task(build_id=task.build_id)
    cls._queue.put(task)
    log(f'QUEUED {task.name}', quiet=options.quiet, build_id=task.build_id)
    cls._maybe_start_tasks()

  @classmethod
  def deactivate(cls):
    cls._deactivated = True
    while not cls._queue.empty():
      try:
        task = cls._queue.get_nowait()
      except queue.Empty:
        return
      task.terminate()

  @staticmethod
  # pylint: disable=inconsistent-return-statements
  def _num_running_processes():
    with open('/proc/stat') as f:
      for line in f:
        if line.startswith('procs_running'):
          return int(line.rstrip().split()[1])
    assert False, 'Could not read /proc/stat'

  @classmethod
  def _maybe_start_tasks(cls):
    if cls._deactivated:
      return
    # Include load avg so that a small dip in the number of currently running
    # processes will not cause new tasks to be started while the overall load is
    # heavy.
    cur_load = max(cls._num_running_processes(), os.getloadavg()[0])
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
        next_task = cls._queue.get_nowait()
      except queue.Empty:
        return
      num_started += next_task.start(cls._maybe_start_tasks)


# TODO(wnwen): Break this into Request (encapsulating what ninja sends) and Task
#              when a Request starts to be run. This would eliminate ambiguity
#              about when and whether _proc/_thread are initialized.
class Task:
  """Class to represent one task and operations on it."""

  def __init__(self, name: str, cwd: str, cmd: List[str], tty: IO[str],
               stamp_file: str, build_id: str, remote_print: bool, options):
    self.name = name
    self.cwd = cwd
    self.cmd = cmd
    self.stamp_file = stamp_file
    self.tty = tty
    self.build_id = build_id
    self.remote_print = remote_print
    self.options = options
    self._terminated = False
    self._replaced = False
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
      TaskStats.add_process(self.build_id)
      log(f'STARTING {self.name}',
          quiet=self.options.quiet,
          build_id=self.build_id)
      # This use of preexec_fn is sufficiently simple, just one os.nice call.
      # pylint: disable=subprocess-popen-preexec-fn
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

  def terminate(self, replaced=False):
    """Can be called multiple times to cancel and ignore the task's output."""

    with self._lock:
      if self._terminated:
        return
      self._terminated = True
      self._replaced = replaced
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
    TaskStats.remove_process(build_id=self.build_id)
    self._complete(stdout)
    on_complete_callback()

  def _complete(self, stdout: str = ''):
    """Update the user and ninja after the task has run or been terminated.

    This method should only be run once per task. Avoid modifying the task so
    that this method does not need locking."""

    TaskStats.complete_task(build_id=self.build_id)
    delete_stamp = False
    status_string = 'FINISHED'
    if self._terminated:
      status_string = 'TERMINATED'
      # When tasks are replaced, avoid deleting the stamp file, context:
      # https://issuetracker.google.com/301961827.
      if not self._replaced:
        delete_stamp = True
    else:
      if stdout or self._return_code != 0:
        status_string = 'FAILED'
        delete_stamp = True
        message = '\n'.join([
            f'FAILED: {self.name}',
            f'Return code: {self._return_code}',
            'CMD: ' + ' '.join(self.cmd),
            'STDOUT:',
            stdout,
        ])
        log_to_file(message, build_id=self.build_id)
        if not self.options.quiet:
          # An extra new line is needed since we want to preserve the previous
          # _log line. Use a single print so that it is threadsafe.
          # TODO(wnwen): Improve stdout display by parsing over it and moving the
          #              actual error to the bottom. Otherwise long command lines
          #              in the Traceback section obscure the actual error(s).
          print('\n' + message)
        if self.remote_print:
          QueuedOutputs.add_output(self, message)
    log(f'{status_string} {self.name}',
        quiet=self.options.quiet,
        build_id=self.build_id)

    if delete_stamp:
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
    message_bytes = server_utils.ReceiveMessage(conn)
    if message_bytes:
      yield json.loads(message_bytes), conn


def _handle_add_task(data, tasks: Dict[Tuple[str, str], Task], options):
  is_experimental = data.get('experimental', False)
  tty = None
  build_id = data['build_id']
  task_outdir = data['cwd']
  create_logfile(build_id, task_outdir)
  if is_experimental:
    tty = open(data['tty'], 'wt')
  task = Task(name=data['name'],
              cwd=task_outdir,
              cmd=data['cmd'],
              tty=tty,
              build_id=build_id,
              remote_print=is_experimental,
              stamp_file=data['stamp_file'],
              options=options)
  existing_task = tasks.get(task.key)
  if existing_task:
    existing_task.terminate(replaced=True)
  tasks[task.key] = task
  TaskManager.add_task(task, options)


def _handle_query_build(data, connection: socket.socket):
  build_id = data['build_id']
  pending_outputs = QueuedOutputs.get_pending_outputs(build_id)
  pending_tasks = TaskStats.num_pending_tasks(build_id)
  completed_tasks = TaskStats.num_completed_tasks(build_id)
  response = {
      'build_id': build_id,
      'completed_tasks': completed_tasks,
      'pending_tasks': pending_tasks,
      'pending_outputs': pending_outputs,
  }
  try:
    with connection:
      server_utils.SendMessage(connection, json.dumps(response).encode('utf8'))
  except BrokenPipeError:
    # We should not die because the client died.
    pass


def _handle_heartbeat(connection: socket.socket):
  try:
    with connection:
      server_utils.SendMessage(connection,
                               json.dumps({
                                   'status': 'OK'
                               }).encode('utf8'))
  except BrokenPipeError:
    # We should not die because the client died.
    pass


def _process_requests(sock: socket.socket, options):
  # Since dicts in python can contain anything, explicitly type tasks to help
  # make static type checking more useful.
  tasks: Dict[Tuple[str, str], Task] = {}
  log(
      'READY... Remember to set android_static_analysis="build_server" in '
      'args.gn files',
      quiet=options.quiet)
  # pylint: disable=too-many-nested-blocks
  try:
    while True:
      try:
        for data, connection in _listen_for_request_data(sock):
          message_type = data.get('message_type', server_utils.ADD_TASK)
          if message_type == server_utils.POLL_HEARTBEAT:
            _handle_heartbeat(connection)
          if message_type == server_utils.ADD_TASK:
            connection.close()
            _handle_add_task(data, tasks, options)
          if message_type == server_utils.QUERY_BUILD:
            _handle_query_build(data, connection)
      except TimeoutError:
        # If we have not received a new task in a while and do not have any
        # pending tasks, then exit. Otherwise keep waiting.
        if TaskStats.num_pending_tasks() == 0 and options.exit_on_idle:
          raise

  except (KeyboardInterrupt, TimeoutError):
    # We expect these to happen, no need to print a stacktrace.
    pass
  finally:
    log('STOPPING SERVER...', end='\n', quiet=options.quiet)
    # Gracefully shut down the task manager, terminating all queued tasks.
    TaskManager.deactivate()
    # Terminate all currently running tasks.
    for task in tasks.values():
      task.terminate()
    QueuedOutputs.flush_messages()
    log('STOPPED', end='\n', quiet=options.quiet)


def query_build_info(build_id):
  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    sock.connect(server_utils.SOCKET_ADDRESS)
    sock.settimeout(1)
    server_utils.SendMessage(
        sock,
        json.dumps({
            'message_type': server_utils.QUERY_BUILD,
            'build_id': build_id,
        }).encode('utf8'))
    response_bytes = server_utils.ReceiveMessage(sock)
    return json.loads(response_bytes)


def _wait_for_build(build_id):
  start_time = datetime.datetime.now()
  while True:
    build_info = query_build_info(build_id)
    pending_tasks = build_info['pending_tasks']
    pending_outputs = build_info['pending_outputs']
    for pending_message in pending_outputs:
      print('\n' + pending_message)

    if pending_tasks == 0:
      print(f'\nAll tasks completed for build_id: {build_id}.')
      return 0

    current_time = datetime.datetime.now()
    duration = current_time - start_time
    print(f'\rWaiting for {pending_tasks} tasks [{str(duration)}]\033[K',
          end='',
          flush=True)
    time.sleep(1)


def _check_if_running():
  with socket.socket(socket.AF_UNIX) as sock:
    try:
      sock.connect(server_utils.SOCKET_ADDRESS)
    except socket.error:
      print('Build server is not running and '
            'android_static_analysis="build_server" is set.\nPlease run '
            'this command in a separate terminal:\n\n'
            '$ build/android/fast_local_dev_server.py\n')
      return 1
    else:
      return 0


def _wait_for_task_requests(args):
  with socket.socket(socket.AF_UNIX) as sock:
    sock.settimeout(_SOCKET_TIMEOUT)
    try:
      sock.bind(server_utils.SOCKET_ADDRESS)
    except socket.error as e:
      # errno 98 is Address already in use
      if e.errno == 98:
        print('fast_local_dev_server.py is already running.')
        return 1
      raise
    sock.listen()
    _process_requests(sock, args)
  return 0


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--fail-if-not-running',
      action='store_true',
      help='Used by GN to fail fast if the build server is not running.')
  parser.add_argument(
      '--exit-on-idle',
      action='store_true',
      help='Server started on demand. Exit when all tasks run out.')
  parser.add_argument('--quiet',
                      action='store_true',
                      help='Do not output status updates.')
  parser.add_argument('--wait-for-build',
                      metavar='BUILD_ID',
                      help='Wait for build server to finish with all tasks '
                      'for BUILD_ID and output any pending messages.')
  args = parser.parse_args()
  if args.fail_if_not_running:
    return _check_if_running()
  if args.wait_for_build:
    return _wait_for_build(args.wait_for_build)
  return _wait_for_task_requests(args)


if __name__ == '__main__':
  sys.exit(main())
