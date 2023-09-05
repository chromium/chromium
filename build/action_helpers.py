# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions useful when writing scripts used by action() targets."""

import contextlib
import filecmp
import os
import pathlib
import posixpath
import shutil
import tempfile

import gn_helpers

from typing import Optional
from typing import Sequence


@contextlib.contextmanager
def atomic_output(path, mode='w+b', only_if_changed=True):
  """Prevent half-written files and dirty mtimes for unchanged files.

  Args:
    path: Path to the final output file, which will be written atomically.
    mode: The mode to open the file in (str).
    only_if_changed: Whether to maintain the mtime if the file has not changed.
  Returns:
    A Context Manager that yields a NamedTemporaryFile instance. On exit, the
    manager will check if the file contents is different from the destination
    and if so, move it into place.

  Example:
    with action_helpers.atomic_output(output_path) as tmp_file:
      subprocess.check_call(['prog', '--output', tmp_file.name])
  """
  # Create in same directory to ensure same filesystem when moving.
  dirname = os.path.dirname(path) or '.'
  os.makedirs(dirname, exist_ok=True)
  with tempfile.NamedTemporaryFile(mode,
                                   suffix=os.path.basename(path),
                                   dir=dirname,
                                   delete=False) as f:
    try:
      yield f

      # File should be closed before comparison/move.
      f.close()
      if not (only_if_changed and os.path.exists(path)
              and filecmp.cmp(f.name, path)):
        shutil.move(f.name, path)
    finally:
      f.close()
      if os.path.exists(f.name):
        os.unlink(f.name)


def add_depfile_arg(parser):
  if hasattr(parser, 'add_option'):
    func = parser.add_option
  else:
    func = parser.add_argument
  func('--depfile', help='Path to depfile (refer to "gn help depfile")')


def write_depfile(depfile_path: str,
                  first_gn_output: str,
                  inputs: Optional[Sequence[str]] = None) -> None:
  """Writes a ninja depfile.

  See notes about how to use depfiles in //build/docs/writing_gn_templates.md.

  Args:
    depfile_path: Path to file to write.
    first_gn_output: Path of first entry in action's outputs.
    inputs: List of inputs to add to depfile.
  """
  assert depfile_path != first_gn_output  # http://crbug.com/646165
  assert not isinstance(inputs, str)  # Easy mistake to make

  def _process_path(path):
    assert not os.path.isabs(path), f'Found abs path in depfile: {path}'
    if os.path.sep != posixpath.sep:
      path = str(pathlib.Path(path).as_posix())
    assert '\\' not in path, f'Found \\ in depfile: {path}'
    return path.replace(' ', '\\ ')

  sb = []
  sb.append(_process_path(first_gn_output))
  if inputs:
    # Sort and uniquify to ensure file is hermetic.
    # One path per line to keep it human readable.
    sb.append(': \\\n ')
    sb.append(' \\\n '.join(sorted(_process_path(p) for p in set(inputs))))
  else:
    sb.append(': ')
  sb.append('\n')

  path = pathlib.Path(depfile_path)
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(''.join(sb))


def parse_gn_list(value):
  """Converts a "GN-list" command-line parameter into a list.

  Conversions handled:
    * None -> []
    * '' -> []
    * 'asdf' -> ['asdf']
    * '["a", "b"]' -> ['a', 'b']
    * ['["a", "b"]', 'c'] -> ['a', 'b', 'c']  (action='append')

  This allows passing args like:
  gn_list = [ "one", "two", "three" ]
  args = [ "--items=$gn_list" ]
  """
  # Convert None to [].
  if not value:
    return []
  # Convert a list of GN lists to a flattened list.
  if isinstance(value, list):
    ret = []
    for arg in value:
      ret.extend(parse_gn_list(arg))
    return ret
  # Convert normal GN list.
  if value.startswith('['):
    return gn_helpers.GNValueParser(value).ParseList()
  # Convert a single string value to a list.
  return [value]
