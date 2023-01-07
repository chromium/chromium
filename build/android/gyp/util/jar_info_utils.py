# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# Utilities to read and write .jar.info files.
#
# A .jar.info file contains a simple mapping from fully-qualified Java class
# names to the source file that actually defines it.
#
# For APKs, the .jar.info maps the class names to the .jar file that which
# contains its .class definition instead.


def ReadAarSourceInfo(info_path):
  """Returns the source= path from an .aar's source.info file."""
  # The .info looks like: "source=path/to/.aar\n".
  with open(info_path) as f:
    return f.read().rstrip().split('=', 1)[1]


def ParseJarInfoFile(info_path):
  """Parse a given .jar.info file as a dictionary.

  Args:
    info_path: input .jar.info file path.
  Returns:
    A new dictionary mapping fully-qualified Java class names to file paths.
  """
  info_data = dict()
  if os.path.exists(info_path):
    with open(info_path, 'r') as info_file:
      for line in info_file:
        line = line.strip()
        if line:
          fully_qualified_name, path = line.split(',', 1)
          info_data[fully_qualified_name] = path
  return info_data


def WriteJarInfoFile(output_obj, info_data, source_file_map=None):
  """Generate a .jar.info file from a given dictionary.

  Args:
    output_obj: output file object.
    info_data: a mapping of fully qualified Java class names to filepaths.
    source_file_map: an optional mapping from java source file paths to the
      corresponding source .srcjar. This is because info_data may contain the
      path of Java source files that where extracted from an .srcjar into a
      temporary location.
  """
  for fully_qualified_name, path in sorted(info_data.items()):
    if source_file_map and path in source_file_map:
      path = source_file_map[path]
      assert not path.startswith('/tmp'), (
          'Java file path should not be in temp dir: {}'.format(path))
    output_obj.write(('{},{}\n'.format(fully_qualified_name,
                                       path)).encode('utf8'))
