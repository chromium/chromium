# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Converts exceptions to return codes and prints error messages.

This makes it easier to query build tables for particular error types as
exit codes are visible to queries while exception stack traces are not."""

import errno
import fcntl
import logging
import os
import subprocess
import sys
import traceback

from target import FuchsiaTargetException

def _PrintException(value, trace):
  """Prints stack trace and error message for the current exception."""

  traceback.print_tb(trace)
  print(str(value))


def IsStdoutBlocking():
  """Returns True if sys.stdout is blocking or False if non-blocking.

  sys.stdout should always be blocking.  Non-blocking is associated with
  intermittent IOErrors (crbug.com/1080858).
  """

  nonblocking = fcntl.fcntl(sys.stdout, fcntl.F_GETFL) & os.O_NONBLOCK
  return not nonblocking


def HandleExceptionAndReturnExitCode():
  """Maps the current exception to a return code and prints error messages.

  Mapped exception types are assigned blocks of 8 return codes starting at 64.
  The choice of 64 as the starting code is based on the Advanced Bash-Scripting
  Guide (http://tldp.org/LDP/abs/html/exitcodes.html).

  A generic exception is mapped to the start of the block.  More specific
  exceptions are mapped to numbers inside the block.  For example, a
  FuchsiaTargetException is mapped to return code 64, unless it involves SSH
  in which case it is mapped to return code 65.

  Exceptions not specifically mapped go to return code 1.

  Returns the mapped return code."""

  (type, value, trace) = sys.exc_info()
  _PrintException(value, trace)

  if type is FuchsiaTargetException:
    if 'ssh' in str(value).lower():
      print('Error: FuchsiaTargetException: SSH to Fuchsia target failed.')
      return 65
    return 64
  elif type is IOError:
    if value.errno == errno.EAGAIN:
      logging.info('Python print to sys.stdout probably failed')
      if not IsStdoutBlocking():
        logging.warn('sys.stdout is non-blocking')
      return 73
    return 72
  elif type is subprocess.CalledProcessError:
    if os.path.basename(value.cmd[0]) == 'scp':
      print('Error: scp operation failed - %s' % str(value))
      return 81
    if os.path.basename(value.cmd[0]) == 'qemu-img':
      print('Error: qemu-img fuchsia image generation failed.')
      return 82
    return 80
  else:
    return 1
