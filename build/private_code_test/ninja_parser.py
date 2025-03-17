#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extract source file information from .ninja files."""

# Copied from //tools/binary_size/libsupersize

import argparse
import io
import json
import logging
import os
import re
import sys

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
import action_helpers

# E.g.:
# build obj/.../foo.o: cxx gen/.../foo.cc || obj/.../foo.inputdeps.stamp
# build obj/.../libfoo.a: alink obj/.../a.o obj/.../b.o |
# build ./libchrome.so ./lib.unstripped/libchrome.so: solink a.o b.o ...
# build libmonochrome.so: __chrome_android_libmonochrome___rule | ...
_REGEX = re.compile(r'build ([^:]+): \w+ (.*?)(?: *\||\n|$)')

_RLIBS_REGEX = re.compile(r'  rlibs = (.*?)(?:\n|$)')

# Unmatches seems to happen for empty source_sets(). E.g.:
# obj/chrome/browser/ui/libui_public_dependencies.a
_MAX_UNMATCHED_TO_LOG = 20
_MAX_UNMATCHED_TO_IGNORE = 200


class _SourceMapper:

  def __init__(self, dep_map, parsed_files):
    self._dep_map = dep_map
    self.parsed_files = parsed_files
    self._unmatched_paths = set()

  def _FindSourceForPathInternal(self, path):
    if not path.endswith(')'):
      if path.startswith('..'):
        return path
      return self._dep_map.get(path)

    # foo/bar.a(baz.o)
    start_idx = path.index('(')
    lib_name = path[:start_idx]
    obj_name = path[start_idx + 1:-1]
    by_basename = self._dep_map.get(lib_name)
    if not by_basename:
      if lib_name.endswith('rlib') and 'std/' in lib_name:
        # Currently we use binary prebuilt static libraries of the Rust
        # stdlib so we can't get source paths. That may change in future.
        return '(Rust stdlib)/%s' % lib_name
      return None
    if lib_name.endswith('.rlib'):
      # Rust doesn't really have the concept of an object file because
      # the compilation unit is the whole 'crate'. Return whichever
      # filename was the crate root.
      return next(iter(by_basename.values()))
    obj_path = by_basename.get(obj_name)
    if not obj_path:
      # Found the library, but it doesn't list the .o file.
      logging.warning('no obj basename for %s %s', path, obj_name)
      return None
    return self._dep_map.get(obj_path)

  def FindSourceForPath(self, path):
    """Returns the source path for the given object path (or None if not found).

    Paths for objects within archives should be in the format: foo/bar.a(baz.o)
    """
    ret = self._FindSourceForPathInternal(path)
    if not ret and path not in self._unmatched_paths:
      if self.unmatched_paths_count < _MAX_UNMATCHED_TO_LOG:
        logging.warning('Could not find source path for %s (empty source_set?)',
                        path)
      self._unmatched_paths.add(path)
    return ret

  @property
  def unmatched_paths_count(self):
    return len(self._unmatched_paths)


def _ParseNinjaPathList(path_list):
  ret = path_list.replace('\\ ', '\b')
  return [s.replace('\b', ' ') for s in ret.split()]


def _OutputsAreObject(outputs):
  return (outputs.endswith('.a') or outputs.endswith('.o')
          or outputs.endswith('.rlib'))


def _ParseOneFile(lines, dep_map, executable_path):
  sub_ninjas = []
  executable_inputs = None
  last_executable_paths = []
  for line in lines:
    if line.startswith('subninja '):
      sub_ninjas.append(line[9:-1])
    # Rust .rlibs are listed as implicit dependencies of the main
    # target linking rule, then are given as an extra
    #   rlibs =
    # variable on a subsequent line. Watch out for that line.
    elif m := _RLIBS_REGEX.match(line):
      if executable_path in last_executable_paths:
        executable_inputs.extend(_ParseNinjaPathList(m.group(1)))
    elif m := _REGEX.match(line):
      outputs, srcs = m.groups()
      if _OutputsAreObject(outputs):
        output = outputs.replace('\\ ', ' ')
        assert output not in dep_map, 'Duplicate output: ' + output
        if output[-1] == 'o':
          dep_map[output] = srcs.replace('\\ ', ' ')
        else:
          obj_paths = _ParseNinjaPathList(srcs)
          dep_map[output] = {os.path.basename(p): p for p in obj_paths}
      elif executable_path:
        last_executable_paths = [
            os.path.normpath(p) for p in _ParseNinjaPathList(outputs)
        ]
        if executable_path in last_executable_paths:
          executable_inputs = _ParseNinjaPathList(srcs)

  return sub_ninjas, executable_inputs


def _Parse(output_directory, executable_path):
  """Parses build.ninja and subninjas.

  Args:
    output_directory: Where to find the root build.ninja.
    executable_path: Path to binary to find inputs for.

  Returns: A tuple of (source_mapper, executable_inputs).
    source_mapper: _SourceMapper instance.
    executable_inputs: List of paths of linker inputs.
  """
  if executable_path:
    executable_path = os.path.relpath(executable_path, output_directory)
  to_parse = ['build.ninja']
  seen_paths = set(to_parse)
  dep_map = {}
  executable_inputs = None
  while to_parse:
    path = os.path.join(output_directory, to_parse.pop())
    with open(path, encoding='utf-8', errors='ignore') as obj:
      sub_ninjas, found_executable_inputs = _ParseOneFile(
          obj, dep_map, executable_path)
      if found_executable_inputs:
        assert not executable_inputs, (
            'Found multiple inputs for executable_path ' + executable_path)
        executable_inputs = found_executable_inputs
    for subpath in sub_ninjas:
      assert subpath not in seen_paths, 'Double include of ' + subpath
      seen_paths.add(subpath)
    to_parse.extend(sub_ninjas)

  assert executable_inputs, 'Failed to find rule that builds ' + executable_path
  return _SourceMapper(dep_map, seen_paths), executable_inputs


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--executable', required=True)
  parser.add_argument('--result-json', required=True)
  parser.add_argument('--depfile')
  args = parser.parse_args()
  logs_io = io.StringIO()
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s',
                      stream=logs_io)

  source_mapper, object_paths = _Parse('.', args.executable)
  logging.info('Found %d linker inputs', len(object_paths))
  source_paths = []
  for obj_path in object_paths:
    result = source_mapper.FindSourceForPath(obj_path) or obj_path
    # Need to recurse on .a files.
    if isinstance(result, dict):
      source_paths.extend(
          source_mapper.FindSourceForPath(v) or v for v in result.values())
    else:
      source_paths.append(result)
  logging.info('Found %d source paths', len(source_paths))

  num_unmatched = source_mapper.unmatched_paths_count
  if num_unmatched > _MAX_UNMATCHED_TO_LOG:
    logging.warning('%d paths were missing sources (showed the first %d)',
                    num_unmatched, _MAX_UNMATCHED_TO_LOG)
  if num_unmatched > _MAX_UNMATCHED_TO_IGNORE:
    raise Exception('Too many unmapped files. Likely a bug in ninja_parser.py')

  if args.depfile:
    action_helpers.write_depfile(args.depfile, args.result_json,
                                 source_mapper.parsed_files)

  with open(args.result_json, 'w', encoding='utf-8') as f:
    json.dump({
        'logs': logs_io.getvalue(),
        'source_paths': source_paths,
    }, f)


if __name__ == '__main__':
  main()
