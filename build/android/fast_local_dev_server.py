#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates an server to offload non-critical-path GN targets."""

import argparse
# pylint: disable=wrong-import-order
import concurrent.futures
import json
import logging
import os
import socket
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import server_utils


def _run_command(name, cwd, cmd):
  logging.info('Started %s', name)
  logging.debug('CWD: %s', cwd)
  logging.debug('CMD: %s', ' '.join(cmd))
  # The environment variable forces the script to actually run in order to avoid
  # infinite recursion.
  env = os.environ.copy()
  env[server_utils.BUILD_SERVER_ENV_VARIABLE] = '1'
  # Use os.nice(19) to ensure the lowest priority (idle) for these analysis
  # tasks since we want to avoid slowing down the actual build.
  # pylint: disable=no-member
  completed_process = subprocess.run(
      cmd,
      capture_output=True,
      cwd=cwd,
      env=env,
      preexec_fn=lambda: os.nice(19),
      text=True,
  )
  if completed_process.stdout or completed_process.stderr:
    # Since this logging is done in the main process (just in separate threads),
    # and logging is threadsafe, we do not need additional synchronization.
    logging.error('Finished %s, but had output:\n%s', name,
                  completed_process.stdout + completed_process.stderr)
  else:
    logging.info('Finished %s', name)


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
    yield json.loads(b''.join(received))


def _bind_and_process_requests(sock, executor):
  sock.bind(server_utils.SOCKET_ADDRESS)
  sock.listen()
  running = set()
  for data in _listen_for_request_data(sock):
    name, cwd, cmd = data['name'], data['cwd'], data['cmd']
    key = (name, cwd)
    if key in running:
      logging.info('Already running %s', name)
    else:
      running.add(key)
      future = executor.submit(_run_command, name=name, cwd=cwd, cmd=cmd)
      # Capture the current key in the lambda's default parameter.
      # Ignore the first parameter as it is just the future.
      future.add_done_callback(lambda _, k=key: running.discard(k))


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('-v',
                      '--verbose',
                      action='count',
                      default=0,
                      help='-v to print info logs, -vv to print debug logs.')
  args = parser.parse_args()
  if args.verbose == 1:
    level = logging.INFO
  elif args.verbose == 2:
    level = logging.DEBUG
  else:
    level = logging.WARNING
  logging.basicConfig(
      level=level,
      format='%(levelname).1s %(process)d %(relativeCreated)6d %(message)s')
  with socket.socket(socket.AF_UNIX) as sock:
    with concurrent.futures.ThreadPoolExecutor() as executor:
      _bind_and_process_requests(sock, executor)


if __name__ == '__main__':
  sys.exit(main())
