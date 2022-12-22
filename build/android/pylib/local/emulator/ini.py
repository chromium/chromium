# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic .ini encoding and decoding.

The basic element in an ini file is the key. Every key is constructed by a name
and a value, delimited by an equals sign (=).

Keys may be grouped into sections. The secetion name will be a line by itself,
in square brackets ([ and ]). All keys after the section are associated with
that section until another section occurs.

Keys that are not under any section are considered at the top level.

Section and key names are case sensitive.
"""


import contextlib
import os


def add_key(line, config, strict=True):
  key, val = line.split('=', 1)
  key = key.strip()
  val = val.strip()
  if strict and key in config:
    raise ValueError('Multiple entries present for key "%s"' % key)
  config[key] = val


def loads(ini_str, strict=True):
  """Deserialize int_str to a dict (nested dict when has sections) object.

  Duplicated sections will merge their keys.

  When there are multiple entries for a key, at the top level, or under the
  same section:
   - If strict is true, ValueError will be raised.
   - If strict is false, only the last occurrence will be stored.
  """
  ret = {}
  section = None
  for line in ini_str.splitlines():
    # Empty line
    if not line:
      continue
    # Section line
    if line[0] == '[' and line[-1] == ']':
      section = line[1:-1]
      if section not in ret:
        ret[section] = {}
    # Key line
    else:
      config = ret if section is None else ret[section]
      add_key(line, config, strict=strict)

  return ret


def load(fp):
  return loads(fp.read())


def dumps(obj):
  results = []
  key_str = ''

  for k, v in sorted(obj.items()):
    if isinstance(v, dict):
      results.append('[%s]\n' % k + dumps(v))
    else:
      key_str += '%s = %s\n' % (k, str(v))

  # Insert key_str at the first position, before any sections
  if key_str:
    results.insert(0, key_str)

  return '\n'.join(results)


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
  ini_contents = {}
  if os.path.exists(ini_file_path):
    with open(ini_file_path) as ini_file:
      ini_contents = load(ini_file)

  yield ini_contents

  with open(ini_file_path, 'w') as ini_file:
    dump(ini_contents, ini_file)
