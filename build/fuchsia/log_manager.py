# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates and manages log file objects.

Provides an object that handles opening and closing file streams for
logging purposes.
"""

import os


class LogManager(object):
  def __init__(self, logs_dir):

    # A dictionary with the log file path as the key and a file stream as value.
    self._logs = {}

    self._logs_dir = logs_dir
    if self._logs_dir:
      if not os.path.isdir(self._logs_dir):
        os.makedirs(self._logs_dir)

  def IsLoggingEnabled(self):
    return self._logs_dir is not None

  def GetLogDirectory(self):
    """Get the directory logs are placed into."""

    return self._logs_dir

  def Open(self, log_file_name):
    """Open a file stream with log_file_name in the logs directory."""

    parent_dir = self.GetLogDirectory()
    if not parent_dir:
      return open(os.devnull, 'w')
    log_file_path = os.path.join(parent_dir, log_file_name)
    if log_file_path in self._logs:
      return self._logs[log_file_path]
    log_file = open(log_file_path, 'w', buffering=1)
    self._logs[log_file_path] = log_file
    return log_file

  def Stop(self):
    for log in self._logs.values():
      log.close()

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    self.Stop()
