# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic .ini encoding and decoding."""


import contextlib
import os


def loads(ini_str, strict=True):
  ret = {}
  for line in ini_str.splitlines():
    key, val = line.split('=', 1)
    key = key.strip()
    val = val.strip()
    if strict and key in ret:
      raise ValueError('Multiple entries present for key "%s"' % key)
    ret[key] = val

  return ret


def load(fp):
  return loads(fp.read())


def dumps(obj):
  ret = ''
  for k, v in sorted(obj.items()):
    ret += '%s = %s\n' % (k, str(v))
  return ret


def dump(obj, fp):
  fp.write(dumps(obj))


@contextlib.contextmanager
def update_ini_file(ini_file_path):
  """Load and update the contents of an ini file.

  Args:
    ini_file_path: A string containing the absolute path of the ini file.
  Yields:
    The contents of the file, as a dict
  """
  if os.path.exists(ini_file_path):
    with open(ini_file_path) as ini_file:
      ini_contents = load(ini_file)
  else:
    ini_contents = {}

  yield ini_contents

  with open(ini_file_path, 'w') as ini_file:
    dump(ini_contents, ini_file)
