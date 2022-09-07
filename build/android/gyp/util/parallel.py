# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers related to multiprocessing.

Based on: //tools/binary_size/libsupersize/parallel.py
"""

import atexit
import logging
import multiprocessing
import os
import sys
import threading
import traceback

DISABLE_ASYNC = os.environ.get('DISABLE_ASYNC') == '1'
if DISABLE_ASYNC:
  logging.warning('Running in synchronous mode.')

_all_pools = None
_is_child_process = False
_silence_exceptions = False

# Used to pass parameters to forked processes without pickling.
_fork_params = None
_fork_kwargs = None


class _ImmediateResult:
  def __init__(self, value):
    self._value = value

  def get(self):
    return self._value

  def wait(self):
    pass

  def ready(self):
    return True

  def successful(self):
    return True


class _ExceptionWrapper:
  """Used to marshal exception messages back to main process."""

  def __init__(self, msg, exception_type=None):
    self.msg = msg
    self.exception_type = exception_type

  def MaybeThrow(self):
    if self.exception_type:
      raise getattr(__builtins__,
                    self.exception_type)('Originally caused by: ' + self.msg)


class _FuncWrapper:
  """Runs on the fork()'ed side to catch exceptions and spread *args."""

  def __init__(self, func):
    global _is_child_process
    _is_child_process = True
    self._func = func

  def __call__(self, index, _=None):
    global _fork_kwargs
    try:
      if _fork_kwargs is None:  # Clarifies _fork_kwargs is map for pylint.
        _fork_kwargs = {}
      return self._func(*_fork_params[index], **_fork_kwargs)
    except Exception as e:
      # Only keep the exception type for builtin exception types or else risk
      # further marshalling exceptions.
      exception_type = None
      if hasattr(__builtins__, type(e).__name__):
        exception_type = type(e).__name__
      # multiprocessing is supposed to catch and return exceptions automatically
      # but it doesn't seem to work properly :(.
      return _ExceptionWrapper(traceback.format_exc(), exception_type)
    except:  # pylint: disable=bare-except
      return _ExceptionWrapper(traceback.format_exc())


class _WrappedResult:
  """Allows for host-side logic to be run after child process has terminated.

  * Unregisters associated pool _all_pools.
  * Raises exception caught by _FuncWrapper.
  """

  def __init__(self, result, pool=None):
    self._result = result
    self._pool = pool

  def get(self):
    self.wait()
    value = self._result.get()
    _CheckForException(value)
    return value

  def wait(self):
    self._result.wait()
    if self._pool:
      _all_pools.remove(self._pool)
      self._pool = None

  def ready(self):
    return self._result.ready()

  def successful(self):
    return self._result.successful()


def _TerminatePools():
  """Calls .terminate() on all active process pools.

  Not supposed to be necessary according to the docs, but seems to be required
  when child process throws an exception or Ctrl-C is hit.
  """
  global _silence_exceptions
  _silence_exceptions = True
  # Child processes cannot have pools, but atexit runs this function because
  # it was registered before fork()ing.
  if _is_child_process:
    return

  def close_pool(pool):
    try:
      pool.terminate()
    except:  # pylint: disable=bare-except
      pass

  for i, pool in enumerate(_all_pools):
    # Without calling terminate() on a separate thread, the call can block
    # forever.
    thread = threading.Thread(name='Pool-Terminate-{}'.format(i),
                              target=close_pool,
                              args=(pool, ))
    thread.daemon = True
    thread.start()


def _CheckForException(value):
  if isinstance(value, _ExceptionWrapper):
    global _silence_exceptions
    if not _silence_exceptions:
      value.MaybeThrow()
      _silence_exceptions = True
      logging.error('Subprocess raised an exception:\n%s', value.msg)
    sys.exit(1)


def _MakeProcessPool(job_params, **job_kwargs):
  global _all_pools
  global _fork_params
  global _fork_kwargs
  assert _fork_params is None
  assert _fork_kwargs is None
  pool_size = min(len(job_params), multiprocessing.cpu_count())
  _fork_params = job_params
  _fork_kwargs = job_kwargs
  ret = multiprocessing.Pool(pool_size)
  _fork_params = None
  _fork_kwargs = None
  if _all_pools is None:
    _all_pools = []
    atexit.register(_TerminatePools)
  _all_pools.append(ret)
  return ret


def ForkAndCall(func, args):
  """Runs |func| in a fork'ed process.

  Returns:
    A Result object (call .get() to get the return value)
  """
  if DISABLE_ASYNC:
    pool = None
    result = _ImmediateResult(func(*args))
  else:
    pool = _MakeProcessPool([args])  # Omit |kwargs|.
    result = pool.apply_async(_FuncWrapper(func), (0, ))
    pool.close()
  return _WrappedResult(result, pool=pool)


def BulkForkAndCall(func, arg_tuples, **kwargs):
  """Calls |func| in a fork'ed process for each set of args within |arg_tuples|.

  Args:
    kwargs: Common keyword arguments to be passed to |func|.

  Yields the return values in order.
  """
  arg_tuples = list(arg_tuples)
  if not arg_tuples:
    return

  if DISABLE_ASYNC:
    for args in arg_tuples:
      yield func(*args, **kwargs)
    return

  pool = _MakeProcessPool(arg_tuples, **kwargs)
  wrapped_func = _FuncWrapper(func)
  try:
    for result in pool.imap(wrapped_func, range(len(arg_tuples))):
      _CheckForException(result)
      yield result
  finally:
    pool.close()
    pool.join()
    _all_pools.remove(pool)
