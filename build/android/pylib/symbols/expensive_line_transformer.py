# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from abc import ABC, abstractmethod
import logging
import subprocess
import threading
import time
import uuid

from devil.utils import reraiser_thread


class ExpensiveLineTransformer(ABC):
  def __init__(self, process_start_timeout, minimum_timeout, per_line_timeout):
    self._process_start_timeout = process_start_timeout
    self._minimum_timeout = minimum_timeout
    self._per_line_timeout = per_line_timeout
    self._started = False
    # Allow only one thread to call TransformLines() at a time.
    self._lock = threading.Lock()
    # Ensure that only one thread attempts to kill self._proc in Close().
    self._close_lock = threading.Lock()
    self._closed_called = False
    # Assign to None so that attribute exists if Popen() throws.
    self._proc = None
    # Start process eagerly to hide start-up latency.
    self._proc_start_time = None

  def start(self):
    # delay the start of the process, to allow the initialization of the
    # descendant classes first.
    if self._started:
      logging.error('%s: Trying to start an already started command', self.name)
      return

    # Start process eagerly to hide start-up latency.
    self._proc_start_time = time.time()

    if not self.command:
      logging.error('%s: No command available', self.name)
      return

    self._proc = subprocess.Popen(self.command,
                                  bufsize=1,
                                  stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  universal_newlines=True,
                                  close_fds=True)
    self._started = True

  def IsClosed(self):
    return (not self._started or self._closed_called
            or self._proc.returncode is not None)

  def IsBusy(self):
    return self._lock.locked()

  def IsReady(self):
    return self._started and not self.IsClosed() and not self.IsBusy()

  def TransformLines(self, lines):
    """Symbolizes names found in the given lines.

    If anything goes wrong (process crashes, timeout, etc), returns |lines|.

    Args:
      lines: A list of strings without trailing newlines.

    Returns:
      A list of strings without trailing newlines.
    """
    if not lines:
      return []

    # symbolized output contain more lines than the input, as the symbolized
    # stacktraces will be added. To account for the extra output lines, keep
    # reading until this eof_line token is reached. Using a format that will
    # be considered a "useful line" without modifying its output by
    # third_party/android_platform/development/scripts/stack_core.py
    eof_line = self.getEofLine()
    out_lines = []

    def _reader():
      while True:
        line = self._proc.stdout.readline()
        # Return an empty string at EOF (when stdin is closed).
        if not line:
          break
        line = line[:-1]
        if line == eof_line:
          break
        out_lines.append(line)

    if self.IsBusy():
      logging.warning('%s: Having to wait for transformation.', self.name)

    # Allow only one thread to operate at a time.
    with self._lock:
      if self.IsClosed():
        if self._started and not self._closed_called:
          logging.warning('%s: Process exited with code=%d.', self.name,
                          self._proc.returncode)
          self.Close()
        return lines

      reader_thread = reraiser_thread.ReraiserThread(_reader)
      reader_thread.start()

      try:
        self._proc.stdin.write('\n'.join(lines))
        self._proc.stdin.write('\n{}\n'.format(eof_line))
        self._proc.stdin.flush()
        time_since_proc_start = time.time() - self._proc_start_time
        timeout = (max(0, self._process_start_timeout - time_since_proc_start) +
                   max(self._minimum_timeout,
                       len(lines) * self._per_line_timeout))
        reader_thread.join(timeout)
        if self.IsClosed():
          logging.warning('%s: Close() called by another thread during join().',
                          self.name)
          return lines
        if reader_thread.is_alive():
          logging.error('%s: Timed out after %f seconds with input:', self.name,
                        timeout)
          for l in lines:
            logging.error(l)
          logging.error(eof_line)
          logging.error('%s: End of timed out input.', self.name)
          logging.error('%s: Timed out output was:', self.name)
          for l in out_lines:
            logging.error(l)
          logging.error('%s: End of timed out output.', self.name)
          self.Close()
          return lines
        return out_lines
      except IOError:
        logging.exception('%s: Exception during transformation', self.name)
        self.Close()
        return lines

  def Close(self):
    with self._close_lock:
      needs_closing = not self.IsClosed()
      self._closed_called = True

    if needs_closing:
      self._proc.stdin.close()
      self._proc.kill()
      self._proc.wait()

  def __del__(self):
    # self._proc is None when Popen() fails.
    if not self._closed_called and self._proc:
      logging.error('%s: Forgot to Close()', self.name)
      self.Close()

  @property
  @abstractmethod
  def name(self):
    ...

  @property
  @abstractmethod
  def command(self):
    ...

  @staticmethod
  def getEofLine():
    # Use a format that will be considered a "useful line" without modifying its
    # output by third_party/android_platform/development/scripts/stack_core.py
    return "Generic useful log header: \'{}\'".format(uuid.uuid4().hex)


class ExpensiveLineTransformerPool(ABC):
  def __init__(self, max_restarts, pool_size, passthrough_on_failure):
    self._max_restarts = max_restarts
    self._pool = [self.CreateTransformer() for _ in range(pool_size)]
    self._passthrough_on_failure = passthrough_on_failure
    # Allow only one thread to select from the pool at a time.
    self._lock = threading.Lock()
    self._num_restarts = 0

  def __enter__(self):
    pass

  def __exit__(self, *args):
    self.Close()

  def TransformLines(self, lines):
    with self._lock:
      assert self._pool, 'TransformLines() called on a closed Pool.'

      # transformation is broken.
      if self._num_restarts == self._max_restarts:
        if self._passthrough_on_failure:
          return lines
        raise Exception('%s is broken.' % self.name)

      # Restart any closed transformer.
      for i, d in enumerate(self._pool):
        if d.IsClosed():
          logging.warning('%s: Restarting closed instance.', self.name)
          self._pool[i] = self.CreateTransformer()
          self._num_restarts += 1
          if self._num_restarts == self._max_restarts:
            logging.warning('%s: MAX_RESTARTS reached.', self.name)
            if self._passthrough_on_failure:
              return lines
            raise Exception('%s is broken.' % self.name)

      selected = next((x for x in self._pool if x.IsReady()), self._pool[0])
      # Rotate the order so that next caller will not choose the same one.
      self._pool.remove(selected)
      self._pool.append(selected)

    return selected.TransformLines(lines)

  def Close(self):
    with self._lock:
      for d in self._pool:
        d.Close()
      self._pool = None

  @abstractmethod
  def CreateTransformer(self):
    ...

  @property
  @abstractmethod
  def name(self):
    ...
