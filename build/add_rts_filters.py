#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a dummy RTS filter file if a real ones do not exist yet.
  Real filter files are generated for suites with skippable tests.
  Not every test suite will have filter data to use and therefore
  no filter file will be created. This ensures that a file exists
  to avoid file not found errors. The files will contain no skippable
  tests, so there is no effect.

  Implementation uses try / except because the filter files are written
  relatively close to when this code creates the dummy files.

  The following type of implementation would have a race condition:
  if not os.path.isfile(filter_file):
    open(filter_file, 'w') as fp:
      fp.write('*')
"""
import errno
import os
import sys


def main():
  filter_file = sys.argv[1]
  # '*' is a dummy that means run everything
  write_filter_file(filter_file, '*')


def write_filter_file(filter_file, filter_string):
  directory = os.path.dirname(filter_file)
  try:
    os.makedirs(directory)
  except OSError as err:
    if err.errno == errno.EEXIST:
      pass
    else:
      raise
  try:
    fp = os.open(filter_file, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
  except OSError as err:
    if err.errno == errno.EEXIST:
      pass
    else:
      raise
  else:
    with os.fdopen(fp, 'w') as file_obj:
      file_obj.write(filter_string)


if __name__ == '__main__':
  sys.exit(main())
