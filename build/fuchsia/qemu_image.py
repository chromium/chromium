# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Workaround for qemu-img bug on arm64 platforms with multiple cores.

Runs qemu-img command with timeout and retries the command if it hangs.

See:
crbug.com/1046861 QEMU is out of date; current version of qemu-img
is unstable

https://bugs.launchpad.net/qemu/+bug/1805256 qemu-img hangs on
rcu_call_ready_event logic in Aarch64 when converting images

TODO(crbug.com/1046861): Remove this workaround when the bug is fixed.
"""

import logging
import subprocess
import tempfile
import time


# qemu-img p99 run time on Cavium ThunderX2 servers is 26 seconds.
# Using 2x the p99 time as the timeout.
QEMU_IMG_TIMEOUT_SEC = 52


def _ExecQemuImgWithTimeout(command):
  """Execute qemu-img command in subprocess with timeout.

  Returns: None if command timed out or return code if command completed.
  """

  logging.info('qemu-img starting')
  command_output_file = tempfile.NamedTemporaryFile('w')
  p = subprocess.Popen(command, stdout=command_output_file,
                       stderr=subprocess.STDOUT)
  start_sec = time.time()
  while p.poll() is None and time.time() - start_sec < QEMU_IMG_TIMEOUT_SEC:
    time.sleep(1)
  stop_sec = time.time()
  logging.info('qemu-img duration: %f' % float(stop_sec - start_sec))

  if p.poll() is None:
    returncode = None
    p.kill()
    p.wait()
  else:
    returncode = p.returncode

  log_level = logging.WARN if returncode else logging.DEBUG
  for line in open(command_output_file.name, 'r'):
    logging.log(log_level, 'qemu-img stdout: ' + line.strip())

  return returncode


def ExecQemuImgWithRetry(command):
  """ Execute qemu-img command in subprocess with 2 retries.

  Raises CalledProcessError if command does not complete successfully.
  """

  tries = 0
  status = None
  while status is None and tries <= 2:
    tries += 1
    status = _ExecQemuImgWithTimeout(command)

  if status is None:
    raise subprocess.CalledProcessError(-1, command)
  if status:
    raise subprocess.CalledProcessError(status, command)
