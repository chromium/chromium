#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Lint as: python3
"""Prints out available java targets.

Examples:
# List GN target for bundles:
build/android/list_java_targets.py -C out/Default --type android_app_bundle \
--gn-labels

# List all android targets with types:
build/android/list_java_targets.py -C out/Default --print-types

# Build all apk targets:
build/android/list_java_targets.py -C out/Default --type android_apk | xargs \
autoninja -C out/Default

# Show how many of each target type exist:
build/android/list_java_targets.py -C out/Default --stats

"""

import argparse
import collections
import json
import logging
import os
import shlex
import shutil
import subprocess
import sys

_SRC_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..',
                                          '..'))
sys.path.append(os.path.join(_SRC_ROOT, 'build'))
import gn_helpers

sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android'))
from pylib import constants

_VALID_TYPES = (
    'android_apk',
    'android_app_bundle',
    'android_app_bundle_module',
    'android_assets',
    'android_resources',
    'dist_aar',
    'dist_jar',
    'group',
    'java_annotation_processor',
    'java_binary',
    'java_library',
    'robolectric_binary',
    'system_java_library',
)


def _resolve_ninja():
  # Prefer the version on PATH, but fallback to known version if PATH doesn't
  # have one (e.g. on bots).
  if shutil.which('ninja') is None:
    return os.path.join(_SRC_ROOT, 'third_party', 'ninja', 'ninja')
  return 'ninja'


def _compile(output_dir, args, quiet=False):
  cmd = gn_helpers.CreateBuildCommand(output_dir) + args
  logging.info('Running: %s', shlex.join(cmd))
  if quiet:
    subprocess.run(cmd, check=True, capture_output=True)
  else:
    subprocess.run(cmd, check=True, stdout=sys.stderr)


def _query_for_build_config_targets(output_dir):
  # Query ninja rather than GN since it's faster.
  # Use ninja rather than autoninja to avoid extra output if user has set the
  # NINJA_SUMMARIZE_BUILD environment variable.
  cmd = [_resolve_ninja(), '-C', output_dir, '-t', 'targets']
  logging.info('Running: %r', cmd)
  ninja_output = subprocess.run(cmd,
                                check=True,
                                capture_output=True,
                                encoding='ascii').stdout
  ret = []
  SUFFIX = '__build_config_crbug_908819'
  SUFFIX_LEN = len(SUFFIX)
  for line in ninja_output.splitlines():
    ninja_target = line.rsplit(':', 1)[0]
    # Ignore root aliases by ensuring a : exists.
    if ':' in ninja_target and ninja_target.endswith(SUFFIX):
      ret.append(f'//{ninja_target[:-SUFFIX_LEN]}')
  return ret


def _query_json(*, json_dict: dict, query: str, path: str):
  """Traverses through the json dictionary according to the query.

  If at any point a key does not exist, return the empty string, but raise an
  error if a key exists but is the wrong type.

  This is roughly equivalent to returning
  json_dict[queries[0]]?[queries[1]]?...[queries[N]]? where the ? means that if
  the key doesn't exist, the empty string is returned.

  Example:
  Given json_dict = {'a': {'b': 'c'}}
  - If queries = ['a', 'b']
    Return: 'c'
  - If queries = ['a', 'd']
    Return ''
  - If queries = ['x']
    Return ''
  - If queries = ['a', 'b', 'x']
    Raise an error since json_dict['a']['b'] is the string 'c' instead of an
    expected dict that can be indexed into.

  Returns the final result after exhausting all the queries.
  """
  queries = query.split('.')
  value = json_dict
  try:
    for key in queries:
      value = value.get(key)
      if value is None:
        return ''
  except AttributeError as e:
    raise Exception(
        f'Failed when attempting to get {queries} from {path}') from e
  return value


class _TargetEntry:

  def __init__(self, gn_target):
    assert gn_target.startswith('//'), f'{gn_target} does not start with //'
    assert ':' in gn_target, f'Non-root {gn_target} required'
    self.gn_target = gn_target
    self._build_config = None

  @property
  def ninja_target(self):
    return self.gn_target[2:]

  @property
  def ninja_build_config_target(self):
    return self.ninja_target + '__build_config_crbug_908819'

  @property
  def build_config_path(self):
    """Returns the filepath of the project's .build_config.json."""
    ninja_target = self.ninja_target
    # Support targets at the root level. e.g. //:foo
    if ninja_target[0] == ':':
      ninja_target = ninja_target[1:]
    subpath = ninja_target.replace(':', os.path.sep) + '.build_config.json'
    return os.path.join(constants.GetOutDirectory(), 'gen', subpath)

  def build_config(self):
    """Reads and returns the project's .build_config.json JSON."""
    if not self._build_config:
      with open(self.build_config_path) as jsonfile:
        self._build_config = json.load(jsonfile)
    return self._build_config

  def get_type(self):
    """Returns the target type from its .build_config.json."""
    return self.build_config()['deps_info']['type']

  def proguard_enabled(self):
    """Returns whether proguard runs for this target."""
    # Modules set proguard_enabled, but the proguarding happens only once at the
    # bundle level.
    if self.get_type() == 'android_app_bundle_module':
      return False
    return self.build_config()['deps_info'].get('proguard_enabled', False)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('-C',
                      '--output-directory',
                      help='If outdir is not provided, will attempt to guess.')
  parser.add_argument('--gn-labels',
                      action='store_true',
                      help='Print GN labels rather than ninja targets')
  parser.add_argument(
      '--nested',
      action='store_true',
      help='Do not convert nested targets to their top-level equivalents. '
      'E.g. Without this, foo_test__apk -> foo_test')
  parser.add_argument('--print-types',
                      action='store_true',
                      help='Print type of each target')
  parser.add_argument(
      '--print-build-config-paths',
      action='store_true',
      help='Print path to the .build_config.json of each target')
  parser.add_argument('--build',
                      action='store_true',
                      help='Build all .build_config.json files.')
  parser.add_argument('--type',
                      action='append',
                      help='Restrict to targets of given type',
                      choices=_VALID_TYPES)
  parser.add_argument('--stats',
                      action='store_true',
                      help='Print counts of each target type.')
  parser.add_argument('--proguard-enabled',
                      action='store_true',
                      help='Restrict to targets that have proguard enabled.')
  parser.add_argument('--query',
                      help='A dot separated string specifying a query for a '
                      'build config json value of each target. Example: Use '
                      '--query deps_info.unprocessed_jar_path to show a list '
                      'of all targets that have a non-empty deps_info dict and '
                      'non-empty "unprocessed_jar_path" value in that dict.')
  parser.add_argument('-v', '--verbose', default=0, action='count')
  parser.add_argument('-q', '--quiet', default=0, action='count')
  args = parser.parse_args()

  args.build |= bool(args.type or args.proguard_enabled or args.print_types
                     or args.stats or args.query)

  logging.basicConfig(level=logging.WARNING + 10 * (args.quiet - args.verbose),
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  if args.output_directory:
    constants.SetOutputDirectory(args.output_directory)
  constants.CheckOutputDirectory()
  output_dir = constants.GetOutDirectory()

  # Query ninja for all __build_config_crbug_908819 targets.
  targets = _query_for_build_config_targets(output_dir)
  entries = [_TargetEntry(t) for t in targets]

  if args.build:
    logging.warning('Building %d .build_config.json files...', len(entries))
    _compile(output_dir, [e.ninja_build_config_target for e in entries],
             quiet=args.quiet)

  if args.type:
    entries = [e for e in entries if e.get_type() in args.type]

  if args.proguard_enabled:
    entries = [e for e in entries if e.proguard_enabled()]

  if args.stats:
    counts = collections.Counter(e.get_type() for e in entries)
    for entry_type, count in sorted(counts.items()):
      print(f'{entry_type}: {count}')
  else:
    for e in entries:
      if args.gn_labels:
        to_print = e.gn_target
      else:
        to_print = e.ninja_target

      # Convert to top-level target
      if not args.nested:
        to_print = to_print.replace('__test_apk', '').replace('__apk', '')

      if args.print_types:
        to_print = f'{to_print}: {e.get_type()}'
      elif args.print_build_config_paths:
        to_print = f'{to_print}: {e.build_config_path}'
      elif args.query:
        value = _query_json(json_dict=e.build_config(),
                            query=args.query,
                            path=e.build_config_path)
        if not value:
          continue
        to_print = f'{to_print}: {value}'

      print(to_print)


if __name__ == '__main__':
  main()
