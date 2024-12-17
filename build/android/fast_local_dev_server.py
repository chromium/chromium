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
import re
import signal
import shlex
import shutil
import socket
import subprocess
import sys
import threading
import traceback
import time
from typing import Callable, Dict, List, Optional, Tuple, IO

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils

_SOCKET_TIMEOUT = 60  # seconds

_LOGFILES = {}
_LOGFILE_NAME = 'buildserver.log'
_MAX_LOGFILES = 6

FIRST_LOG_LINE = '#### Start of log for build_id = {build_id} ####\n'
BUILD_ID_RE = re.compile(r'^#### .*build_id = (?P<build_id>.+) ####')


def log(msg: str, quiet: bool = False):
  if quiet:
    return
  # Ensure we start our message on a new line.
  print('\n' + msg)


def set_status(msg: str, *, quiet: bool = False, build_id: str = None):
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
  print(f'\r{prefix}{msg}\033[K', end='', flush=True)


def log_to_file(message: str, build_id: str):
  logfile = _LOGFILES[build_id]
  print(message, file=logfile, flush=True)


def _exception_hook(exctype: type, exc: Exception, tb):
  # Output uncaught exceptions to all live terminals
  BuildManager.broadcast(''.join(traceback.format_exception(exctype, exc, tb)))
  # Cancel all pending tasks cleanly (i.e. delete stamp files if necessary).
  TaskManager.deactivate()
  sys.__excepthook__(exctype, exc, tb)


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


class TaskStats:
  """Class to keep track of aggregate stats for all tasks across threads."""
  _num_processes = 0
  _completed_tasks = 0
  _total_tasks = 0
  _total_task_count_per_build = collections.defaultdict(int)
  _completed_task_count_per_build = collections.defaultdict(int)
  _running_processes_count_per_build = collections.defaultdict(int)
  _outdir_per_build = {}
  _lock = threading.RLock()

  @classmethod
  def no_running_processes(cls):
    with cls._lock:
      return cls._num_processes == 0

  @classmethod
  def add_task(cls, build_id: str, outdir: str):
    with cls._lock:
      cls._total_tasks += 1
      cls._total_task_count_per_build[build_id] += 1
      cls._outdir_per_build[build_id] = outdir

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
  def query_build(cls, query_build_id: str = None):
    with cls._lock:
      active_builds = BuildManager.get_live_builds()
      if query_build_id:
        build_ids = [query_build_id]
      else:
        build_ids = sorted(
            set(active_builds) | set(cls._total_task_count_per_build))
      builds = []
      for build_id in build_ids:
        current_tasks = TaskManager.get_current_tasks(build_id)
        builds.append({
            'build_id': build_id,
            'is_active': build_id in active_builds,
            'completed_tasks': cls.num_completed_tasks(build_id),
            'pending_tasks': cls.num_pending_tasks(build_id),
            'active_tasks': [t.cmd for t in current_tasks],
            'outdir': cls._outdir_per_build.get(build_id),  # None if no tasks.
        })
      return {
          'pid': os.getpid(),
          'builds': builds,
      }

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


def check_pid_alive(pid: int):
  try:
    os.kill(pid, 0)
  except OSError:
    return False
  return True


class BuildManager:
  _live_builders: dict[str, int] = dict()
  _build_ttys: dict[str, IO[str]] = dict()
  _lock = threading.RLock()

  @classmethod
  def register_builder(cls, build_id, builder_pid):
    with cls._lock:
      cls._live_builders[build_id] = int(builder_pid)

  @classmethod
  def register_tty(cls, build_id, tty):
    with cls._lock:
      cls._build_ttys[build_id] = tty

  @classmethod
  def get_live_builds(cls):
    with cls._lock:
      for build_id, builder_pid in list(cls._live_builders.items()):
        if not check_pid_alive(builder_pid):
          del cls._live_builders[build_id]
      return list(cls._live_builders.keys())

  @classmethod
  def broadcast(cls, msg: str):
    seen = set()
    with cls._lock:
      for tty in cls._build_ttys.values():
        # Do not output to the same tty multiple times. Use st_ino and st_dev to
        # compare open file descriptors.
        st = os.stat(tty.fileno())
        key = (st.st_ino, st.st_dev)
        if key in seen:
          continue
        seen.add(key)
        try:
          tty.write(msg + '\n')
          tty.flush()
        except BrokenPipeError:
          pass

  @classmethod
  def has_live_builds(cls):
    return bool(cls.get_live_builds())


class TaskManager:
  """Class to encapsulate a threadsafe queue and handle deactivating it."""
  _queue: collections.deque[Task] = collections.deque()
  _current_tasks: set[Task] = set()
  _deactivated = False
  _lock = threading.RLock()

  @classmethod
  def add_task(cls, task: Task, options):
    assert not cls._deactivated
    TaskStats.add_task(task.build_id, task.cwd)
    with cls._lock:
      cls._queue.appendleft(task)
    set_status(f'QUEUED {task.name}',
               quiet=options.quiet,
               build_id=task.build_id)
    cls._maybe_start_tasks()

  @classmethod
  def task_done(cls, task: Task):
    TaskStats.complete_task(build_id=task.build_id)
    with cls._lock:
      cls._current_tasks.remove(task)

  @classmethod
  def get_current_tasks(cls, build_id):
    with cls._lock:
      return [t for t in cls._current_tasks if t.build_id == build_id]

  @classmethod
  def deactivate(cls):
    cls._deactivated = True
    tasks_to_terminate: list[Task] = []
    with cls._lock:
      while cls._queue:
        task = cls._queue.pop()
        tasks_to_terminate.append(task)
      # Cancel possibly running tasks.
      tasks_to_terminate.extend(cls._current_tasks)
    # Terminate outside lock since task threads need the lock to finish
    # terminating.
    for task in tasks_to_terminate:
      task.terminate()

  @classmethod
  def cancel_build(cls, build_id):
    terminated_pending_tasks: list[Task] = []
    terminated_current_tasks: list[Task] = []
    with cls._lock:
      # Cancel pending tasks.
      for task in cls._queue:
        if task.build_id == build_id:
          terminated_pending_tasks.append(task)
      for task in terminated_pending_tasks:
        cls._queue.remove(task)
      # Cancel running tasks.
      for task in cls._current_tasks:
        if task.build_id == build_id:
          terminated_current_tasks.append(task)
    # Terminate tasks outside lock since task threads need the lock to finish
    # terminating.
    for task in terminated_pending_tasks:
      task.terminate()
    for task in terminated_current_tasks:
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
      with cls._lock:
        try:
          next_task = cls._queue.pop()
          cls._current_tasks.add(next_task)
        except IndexError:
          return
      num_started += next_task.start(cls._maybe_start_tasks)


# TODO(wnwen): Break this into Request (encapsulating what ninja sends) and Task
#              when a Request starts to be run. This would eliminate ambiguity
#              about when and whether _proc/_thread are initialized.
class Task:
  """Class to represent one task and operations on it."""

  def __init__(self, name: str, cwd: str, cmd: List[str], tty: IO[str],
               stamp_file: str, build_id: str, options):
    self.name = name
    self.cwd = cwd
    self.cmd = cmd
    self.stamp_file = stamp_file
    self.tty = tty
    self.build_id = build_id
    self.options = options
    self._terminated = False
    self._replaced = False
    self._lock = threading.RLock()
    self._proc: Optional[subprocess.Popen] = None
    self._thread: Optional[threading.Thread] = None
    self._delete_stamp_thread: Optional[threading.Thread] = None
    self._return_code: Optional[int] = None

  @property
  def key(self):
    return (self.cwd, self.name)

  def __hash__(self):
    return hash((self.key, self.build_id))

  def __eq__(self, other):
    return self.key == other.key and self.build_id == other.build_id

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
      set_status(f'STARTING {self.name}',
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

    delete_stamp = False
    status_string = 'FINISHED'
    if self._terminated:
      status_string = 'TERMINATED'
      # When tasks are replaced, avoid deleting the stamp file, context:
      # https://issuetracker.google.com/301961827.
      if not self._replaced:
        delete_stamp = True
    elif stdout or self._return_code != 0:
      status_string = 'FAILED'
      delete_stamp = True
      preamble = [
          f'FAILED: {self.name}',
          f'Return code: {self._return_code}',
          'CMD: ' + shlex.join(self.cmd),
          'STDOUT:',
      ]

      message = '\n'.join(preamble + [stdout])
      log_to_file(message, build_id=self.build_id)
      log(message, quiet=self.options.quiet)
      if self.tty:
        # Add emoji to show that output is from the build server.
        preamble = [f'⏩ {line}' for line in preamble]
        remote_message = '\n'.join(preamble + [stdout])
        # Add a new line at start of message to clearly delineate from previous
        # output/text already on the remote tty we are printing to.
        self.tty.write(f'\n{remote_message}')
        self.tty.flush()
    if delete_stamp:
      # Force siso to consider failed targets as dirty.
      try:
        os.unlink(os.path.join(self.cwd, self.stamp_file))
      except FileNotFoundError:
        pass
    else:
      # We do not care about the action writing a too new mtime. Siso only cares
      # about the mtime that is recorded in its database at the time the
      # original action finished.
      pass
    TaskManager.task_done(self)
    set_status(f'{status_string} {self.name}',
               quiet=self.options.quiet,
               build_id=self.build_id)


def _handle_add_task(data, current_tasks: Dict[Tuple[str, str], Task], options):
  """Handle messages of type ADD_TASK."""
  build_id = data['build_id']
  task_outdir = data['cwd']
  tty_name = data.get('tty')

  tty = None
  if tty_name:
    tty = open(tty_name, 'wt')
    BuildManager.register_tty(build_id, tty)

  # Make sure a logfile for the build_id exists.
  create_logfile(build_id, task_outdir)

  new_task = Task(name=data['name'],
                  cwd=task_outdir,
                  cmd=data['cmd'],
                  tty=tty,
                  build_id=build_id,
                  stamp_file=data['stamp_file'],
                  options=options)
  existing_task = current_tasks.get(new_task.key)
  if existing_task:
    existing_task.terminate(replaced=True)
  current_tasks[new_task.key] = new_task

  TaskManager.add_task(new_task, options)


def _handle_query_build(data, connection: socket.socket):
  """Handle messages of type QUERY_BUILD."""
  build_id = data['build_id']
  response = TaskStats.query_build(build_id)
  try:
    with connection:
      server_utils.SendMessage(connection, json.dumps(response).encode('utf8'))
  except BrokenPipeError:
    # We should not die because the client died.
    pass


def _handle_heartbeat(connection: socket.socket):
  """Handle messages of type POLL_HEARTBEAT."""
  try:
    with connection:
      server_utils.SendMessage(connection,
                               json.dumps({
                                   'status': 'OK'
                               }).encode('utf8'))
  except BrokenPipeError:
    # We should not die because the client died.
    pass


def _handle_register_builder(data):
  """Handle messages of type REGISTER_BUILDER."""
  build_id = data['build_id']
  builder_pid = data['builder_pid']
  BuildManager.register_builder(build_id, builder_pid)


def _handle_cancel_build(data):
  """Handle messages of type CANCEL_BUILD."""
  build_id = data['build_id']
  TaskManager.cancel_build(build_id)


def _listen_for_request_data(sock: socket.socket):
  """Helper to encapsulate getting a new message."""
  while True:
    conn = sock.accept()[0]
    message_bytes = server_utils.ReceiveMessage(conn)
    if message_bytes:
      yield json.loads(message_bytes), conn


def _register_cleanup_signal_handlers(options):
  original_sigint_handler = signal.getsignal(signal.SIGINT)
  original_sigterm_handler = signal.getsignal(signal.SIGTERM)

  def _cleanup(signum, frame):
    log('STOPPING SERVER...', quiet=options.quiet)
    # Gracefully shut down the task manager, terminating all queued tasks.
    TaskManager.deactivate()
    log('STOPPED', quiet=options.quiet)
    if signum == signal.SIGINT:
      if callable(original_sigint_handler):
        original_sigint_handler(signum, frame)
      else:
        raise KeyboardInterrupt()
    if signum == signal.SIGTERM:
      # Sometimes sigterm handler is not a callable.
      if callable(original_sigterm_handler):
        original_sigterm_handler(signum, frame)
      else:
        sys.exit(1)

  signal.signal(signal.SIGINT, _cleanup)
  signal.signal(signal.SIGTERM, _cleanup)


def _process_requests(sock: socket.socket, options):
  """Main loop for build server receiving request messages."""
  # Since dicts in python can contain anything, explicitly type tasks to help
  # make static type checking more useful.
  tasks: Dict[Tuple[str, str], Task] = {}
  log(
      'READY... Remember to set android_static_analysis="build_server" in '
      'args.gn files',
      quiet=options.quiet)
  _register_cleanup_signal_handlers(options)
  # pylint: disable=too-many-nested-blocks
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
        if message_type == server_utils.REGISTER_BUILDER:
          connection.close()
          _handle_register_builder(data)
        if message_type == server_utils.CANCEL_BUILD:
          connection.close()
          _handle_cancel_build(data)
    except TimeoutError:
      # If we have not received a new task in a while and do not have any
      # pending tasks or running builds, then exit. Otherwise keep waiting.
      if (TaskStats.num_pending_tasks() == 0
          and not BuildManager.has_live_builds() and options.exit_on_idle):
        break
    except KeyboardInterrupt:
      break


def query_build_info(build_id):
  """Communicates with the main server to query build info."""
  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    sock.connect(server_utils.SOCKET_ADDRESS)
    sock.settimeout(3)
    server_utils.SendMessage(
        sock,
        json.dumps({
            'message_type': server_utils.QUERY_BUILD,
            'build_id': build_id,
        }).encode('utf8'))
    response_bytes = server_utils.ReceiveMessage(sock)
    return json.loads(response_bytes)


def _wait_for_build(build_id):
  """Comunicates with the main server waiting for a build to complete."""
  start_time = datetime.datetime.now()
  while True:
    try:
      build_info = query_build_info(build_id)['builds'][0]
    except ConnectionRefusedError:
      print('No server running. It likely finished all tasks.')
      print('You can check $OUTDIR/buildserver.log.0 to be sure.')
      return 0

    pending_tasks = build_info['pending_tasks']

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
  """Communicates with the main server to make sure its running."""
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


def _send_message_and_close(message_dict):
  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    sock.connect(server_utils.SOCKET_ADDRESS)
    sock.settimeout(3)
    server_utils.SendMessage(sock, json.dumps(message_dict).encode('utf8'))


def _send_cancel_build(build_id):
  _send_message_and_close({
      'message_type': server_utils.CANCEL_BUILD,
      'build_id': build_id,
  })
  return 0


def _register_builder(build_id, builder_pid):
  for _attempt in range(3):
    try:
      _send_message_and_close({
          'message_type': server_utils.REGISTER_BUILDER,
          'build_id': build_id,
          'builder_pid': builder_pid,
      })
      return 0
    except socket.error:
      time.sleep(0.05)
  print(f'Failed to register builer for build_id={build_id}.')
  return 1


def _print_build_status_all():
  try:
    query_data = query_build_info(None)
  except ConnectionRefusedError:
    print('No server running. Consult $OUTDIR/buildserver.log.0')
    return 0
  builds = query_data['builds']
  pid = query_data['pid']
  all_active_tasks = []
  print(f'Build server (PID={pid}) has {len(builds)} registered builds')
  for build_info in builds:
    build_id = build_info['build_id']
    pending_tasks = build_info['pending_tasks']
    completed_tasks = build_info['completed_tasks']
    active_tasks = build_info['active_tasks']
    out_dir = build_info['outdir']
    active = build_info['is_active']
    total_tasks = pending_tasks + completed_tasks
    all_active_tasks += active_tasks
    if total_tasks == 0 and not active:
      status = 'Finished without any jobs'
    else:
      if active:
        status = 'Siso still running'
      else:
        status = 'Siso finished'
      if out_dir:
        status += f' in {out_dir}'
      status += f'. Completed [{completed_tasks}/{total_tasks}].'
      if completed_tasks < total_tasks:
        status += f' {len(active_tasks)} tasks currently executing'
    print(f'{build_id}: {status}')
    if all_active_tasks:
      total = len(all_active_tasks)
      to_show = min(4, total)
      print(f'Currently executing (showing {to_show} of {total}):')
      for cmd in sorted(all_active_tasks)[:to_show]:
        truncated = shlex.join(cmd)
        if len(truncated) > 200:
          truncated = truncated[:200] + '...'
        print(truncated)
  return 0


def _print_build_status(build_id):
  try:
    build_info = query_build_info(build_id)['builds'][0]
  except ConnectionRefusedError:
    print('No server running. Consult $OUTDIR/buildserver.log.0')
    return 0
  pending_tasks = build_info['pending_tasks']
  completed_tasks = build_info['completed_tasks']
  total_tasks = pending_tasks + completed_tasks

  # Print nothing if we never got any tasks.
  if completed_tasks:
    if pending_tasks:
      print('Build server is still running in the background. ' +
            f'[{completed_tasks}/{total_tasks}] Tasks Done.')
      print('Run this to wait for the pending tasks:')
      server_path = os.path.relpath(str(server_utils.SERVER_SCRIPT))
      print(' '.join([server_path, '--wait-for-build', build_id]))
    else:
      print('Build Server is done with all background tasks. ' +
            f'Completed [{completed_tasks}/{total_tasks}].')
  return 0


def _wait_for_task_requests(args):
  with socket.socket(socket.AF_UNIX) as sock:
    sock.settimeout(_SOCKET_TIMEOUT)
    try:
      sock.bind(server_utils.SOCKET_ADDRESS)
    except socket.error as e:
      # errno 98 is Address already in use
      if e.errno == 98:
        return 1
      raise
    sock.listen()
    _process_requests(sock, args)
  return 0


def main():
  # pylint: disable=too-many-return-statements
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
  parser.add_argument('--print-status',
                      metavar='BUILD_ID',
                      help='Print the current state of a build.')
  parser.add_argument('--print-status-all',
                      action='store_true',
                      help='Print the current state of all active builds.')
  parser.add_argument(
      '--register-build-id',
      metavar='BUILD_ID',
      help='Inform the build server that a new build has started.')
  parser.add_argument('--builder-pid',
                      help='Builder process\'s pid for build BUILD_ID.')
  parser.add_argument('--cancel-build',
                      metavar='BUILD_ID',
                      help='Cancel all pending and running tasks for BUILD_ID.')
  args = parser.parse_args()
  if args.fail_if_not_running:
    return _check_if_running()
  if args.wait_for_build:
    return _wait_for_build(args.wait_for_build)
  if args.print_status:
    return _print_build_status(args.print_status)
  if args.print_status_all:
    return _print_build_status_all()
  if args.register_build_id:
    return _register_builder(args.register_build_id, args.builder_pid)
  if args.cancel_build:
    return _send_cancel_build(args.cancel_build)
  return _wait_for_task_requests(args)


if __name__ == '__main__':
  sys.excepthook = _exception_hook
  sys.exit(main())
