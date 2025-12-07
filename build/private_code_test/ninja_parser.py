#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extract source file information from .ninja files."""

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
# build somefile.cc somefile.h: __some_rule | ...
_REGEX = re.compile(
    r'^(?:subninja (.*)|build ([^:]+): [^\s]+ (.*)|  rlibs = (.*))',
    re.MULTILINE)

_IGNORED_SUFFIXES = (
    '.build_metadata',
    '.empty',
    '_expected_outputs.txt',
    '_grd_files.json',
    '.modulemap',
    '.rsp',
    'typemap_config',
)

# Unmatches seems to happen for empty source_sets(). E.g.:
# obj/chrome/browser/ui/libui_public_dependencies.a
_MAX_UNMATCHED_TO_LOG = 1000
_MAX_UNMATCHED_TO_IGNORE = 200
_MAX_RECURSION = 200


class _SourceMapper:

  def __init__(self, dep_map, parsed_files):
    self._dep_map = dep_map
    self.parsed_files = parsed_files
    self._unmatched_paths = set()

  def Lookup(self, path):
    ret = self._dep_map.get(path)
    if not ret:
      # .ninja files do not record origin of files written by write_file() and
      # generated_file().
      if not path.endswith(_IGNORED_SUFFIXES):
        if path not in self._unmatched_paths:
          if self.unmatched_paths_count < _MAX_UNMATCHED_TO_LOG:
            logging.warning('Could not find source path for %s', path)
          self._unmatched_paths.add(path)
    return ret

  @property
  def unmatched_paths_count(self):
    return len(self._unmatched_paths)


def _ParseNinjaPathList(path_list):
  ret = path_list.replace('\\ ', '\b')
  return [
      os.path.normpath(s.replace('\b', ' ')) for s in ret.split() if s[0] != '|'
  ]


def _ParseOneFile(data, dep_map, executable_path):
  sub_ninjas = []
  prev_inputs = None
  for m in _REGEX.finditer(data):
    groups = m.groups()
    if groups[0]:
      sub_ninjas.append(groups[0])
    elif groups[3]:
      # Rust .rlibs are listed as implicit dependencies of the main
      # target linking rule, then are given as an extra
      #   rlibs =
      # variable on a subsequent line. Watch out for that line.
      prev_inputs.extend(_ParseNinjaPathList(groups[3]))
    else:
      outputs = _ParseNinjaPathList(groups[1])
      input_paths = _ParseNinjaPathList(groups[2])
      for output in outputs:
        assert output not in dep_map, f'Duplicate output: {output}'
        dep_map[output] = input_paths
      prev_inputs = input_paths

  return sub_ninjas


def _Parse(output_directory, executable_path):
  """Parses build.ninja and subninjas.

  Args:
    output_directory: Where to find the root build.ninja.
    executable_path: Path to binary to find inputs for.

  Returns: _SourceMapper instance.
  """
  to_parse = ['build.ninja']
  seen_paths = set(to_parse)
  dep_map = {}
  while to_parse:
    path = os.path.join(output_directory, to_parse.pop())
    with open(path, encoding='utf-8', errors='ignore') as obj:
      data = obj.read()
      sub_ninjas = _ParseOneFile(data, dep_map, executable_path)
    for subpath in sub_ninjas:
      assert subpath not in seen_paths, 'Double include of ' + subpath
      seen_paths.add(subpath)
    to_parse.extend(sub_ninjas)

  assert executable_path in dep_map, ('Failed to find rule that builds ' +
                                      executable_path)
  return _SourceMapper(dep_map, seen_paths)


def _CollectInputs(source_mapper, ret, output_path, stack, visited):
  token = len(visited)
  if visited.setdefault(output_path, token) != token:
    return
  if token % 10000 == 0:
    logging.info('Resolved %d', token)

  # Don't recurse into non-generated files or into shared libraries.
  if output_path.startswith('..') or output_path.endswith(('.so', '.so.TOC')):
    ret.add(output_path)
    return

  inputs = source_mapper.Lookup(output_path)
  if not inputs:
    return

  stack.append(output_path)
  if len(stack) > _MAX_RECURSION:
    print('Input loop!')
    print('\n'.join(enumerate(stack)))
    sys.exit(1)
  for p in inputs:
    _CollectInputs(source_mapper, ret, p, stack, visited)
  stack.pop()


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

  logging.info('Parsing build.ninja')
  executable_path = os.path.relpath(args.executable)
  source_mapper = _Parse('.', executable_path)
  object_paths = source_mapper.Lookup(executable_path)
  logging.info('Found %d linker inputs', len(object_paths))
  source_paths = set()
  stack = []
  visited = {}
  for p in object_paths:
    _CollectInputs(source_mapper, source_paths, p, stack, visited)
  logging.info('Found %d source paths', len(source_paths))

  num_unmatched = source_mapper.unmatched_paths_count
  logging.warning('%d paths were missing sources', num_unmatched)
  if num_unmatched > _MAX_UNMATCHED_TO_IGNORE:
    raise Exception(f'{num_unmatched} unmapped files (of {len(object_paths)}).'
                    ' Likely a bug in ninja_parser.py')

  if args.depfile:
    action_helpers.write_depfile(args.depfile, args.result_json,
                                 source_mapper.parsed_files)

  logging.warning('Writing %s source paths', len(source_paths))
  with open(args.result_json, 'w', encoding='utf-8') as f:
    json.dump(
        {
            'logs': logs_io.getvalue(),
            'source_paths': sorted(source_paths),
        }, f)


if __name__ == '__main__':
  main()
