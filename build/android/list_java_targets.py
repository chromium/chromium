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
import glob
import json
import logging
import os
import shlex
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


def _compile(output_dir, args, quiet=False):
  cmd = gn_helpers.CreateBuildCommand(output_dir) + args
  logging.info('Running: %s', shlex.join(cmd))
  if quiet:
    subprocess.run(cmd, check=True, capture_output=True)
  else:
    subprocess.run(cmd, check=True, stdout=sys.stderr)


def _query_for_targets(output_dir):
  # Query ninja rather than GN since it's faster.
  cmd = [
      os.path.join(_SRC_ROOT, 'third_party', 'siso', 'cipd', 'siso'), 'query',
      'targets', '-C', output_dir
  ]
  logging.info('Running: %r', cmd)
  try:
    query_output = subprocess.run(cmd,
                                  check=True,
                                  capture_output=True,
                                  encoding='ascii').stdout
  except subprocess.CalledProcessError as e:
    sys.stderr.write('Command output:\n' + e.stdout + e.stderr)
    raise

  # Dict of target name -> has_build_config
  ret = {}
  # java_prebuilt() targets do not write build_config files, so look for
  # __assetres as well. Targets like android_assets() will not appear at all
  # (oh well).
  SUFFIX1 = '__build_config_crbug_908819'
  SUFFIX_LEN1 = len(SUFFIX1)
  SUFFIX2 = '__assetres'
  SUFFIX_LEN2 = len(SUFFIX2)
  for line in query_output.splitlines():
    ninja_target = line.rsplit(':', 1)[0]
    # Ignore root aliases by ensuring a : exists.
    if ':' in ninja_target:
      if ninja_target.endswith(SUFFIX1):
        ret[f'//{ninja_target[:-SUFFIX_LEN1]}'] = True
      elif ninja_target.endswith(SUFFIX2):
        ret.setdefault(f'//{ninja_target[:-SUFFIX_LEN2]}', False)
  return ret


def _query_json(*, json_dict: dict, query: str, target: str):
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
        f'Failed when attempting to get {queries} from {target}') from e
  return value


class _TargetEntry:

  def __init__(self, gn_target, has_build_config):
    assert gn_target.startswith('//'), f'{gn_target} does not start with //'
    assert ':' in gn_target, f'Non-root {gn_target} required'
    self.gn_target = gn_target
    self.has_build_config = has_build_config
    self._combined_config = None
    self._params_json = None

  @property
  def ninja_target(self):
    return self.gn_target[2:]

  @property
  def ninja_build_config_target(self):
    assert self.has_build_config, 'No build config for ' + self.gn_target
    return self.ninja_target + '__build_config_crbug_908819'

  @property
  def build_config_path(self):
    """Returns the filepath of the project's .build_config.json."""
    assert self.has_build_config, 'No build config for ' + self.gn_target
    return self.params_path.replace('.params.json', '.build_config.json')

  @property
  def params_path(self):
    """Returns the filepath of the project's .params.json."""
    ninja_target = self.ninja_target
    # Support targets at the root level. e.g. //:foo
    if ninja_target[0] == ':':
      ninja_target = ninja_target[1:]
    subpath = ninja_target.replace(':', os.path.sep) + '.params.json'
    return os.path.relpath(
        os.path.join(constants.GetOutDirectory(), 'gen', subpath))

  def params_values(self):
    if not self._params_json:
      with open(self.params_path) as f:
        self._params_json = json.load(f)
    return self._params_json

  def combined_config_values(self):
    """Union of .params.json and *.build_config.json"""
    if not self._combined_config:
      config = dict(self.params_values())
      if self.has_build_config:
        pattern = self.build_config_path.replace('.build_config.json',
                                                 '*.build_config.json')
        for p in glob.glob(pattern):
          with open(p) as f:
            config.update(json.load(f))
      self._combined_config = config
    return self._combined_config

  def get_type(self):
    """Returns the target type from its .build_config.json."""
    return self.params_values()['type']

  def proguard_enabled(self):
    """Returns whether proguard runs for this target."""
    # Modules set proguard_enabled, but the proguarding happens only once at the
    # bundle level.
    if self.get_type() == 'android_app_bundle_module':
      return False
    return self.params_values().get('proguard_enabled', False)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('-C',
                      '--output-directory',
                      help='If outdir is not provided, will attempt to guess.')
  parser.add_argument('--gn-labels',
                      action='store_true',
                      help='Print GN labels rather than ninja targets')
  parser.add_argument('--omit-targets',
                      action='store_true',
                      help='Do not print the target / gn label')
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
  parser.add_argument('--print-params-paths',
                      action='store_true',
                      help='Print path to the .params.json of each target')
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
                      '--query unprocessed_jar_path to show a list '
                      'of all targets that have a non-empty '
                      '"unprocessed_jar_path" value in that dict.')
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

  if args.build:
    _compile(output_dir, ['build.ninja'])

  # Query ninja for all __build_config_crbug_908819 targets.
  # TODO(agrieve): java_group, android_assets, and android_resources do not
  # write .build_config.json files, and so will not show up by this query.
  # If we ever need them to, use "gn gen" into a temp dir, and set an extra
  # gn arg that causes all write_build_config() template to print all targets.
  result = _query_for_targets(output_dir)
  entries = [_TargetEntry(t, v) for t, v in sorted(result.items())]
  entries = [e for e in entries if os.path.exists(e.params_path)]

  if not entries:
    logging.warning('No targets found. Run with --build')
    sys.exit(1)

  if args.build:
    targets = [
        e.ninja_build_config_target for e in entries if e.has_build_config
    ]
    logging.warning('Building %d .build_config.json files...', len(targets))
    _compile(output_dir, targets, quiet=args.quiet)

  if args.type:
    if set(args.type) & {'android_resources', 'android_assets', 'group'}:
      logging.warning('Cannot filter by this type. See TODO.')
      sys.exit(1)
    entries = [e for e in entries if e.get_type() in args.type]

  if args.proguard_enabled:
    entries = [e for e in entries if e.proguard_enabled()]

  if args.stats:
    counts = collections.Counter(e.get_type() for e in entries)
    for entry_type, count in sorted(counts.items()):
      print(f'{entry_type}: {count}')
  else:
    for e in entries:
      if args.omit_targets:
        target_part = ''
      else:
        if args.gn_labels:
          target_part = e.gn_target
        else:
          target_part = e.ninja_target

        # Convert to top-level target
        if not args.nested:
          target_part = target_part.replace('__test_apk',
                                            '').replace('__apk', '')

      type_part = ''
      if args.print_types:
        type_part = e.get_type()
      elif args.print_build_config_paths:
        type_part = e.build_config_path if e.has_build_config else 'N/A'
      elif args.print_params_paths:
        type_part = e.params_path
      elif args.query:
        type_part = _query_json(json_dict=e.combined_config_values(),
                                query=args.query,
                                target=e.gn_target)
        if not type_part:
          continue

      if target_part and type_part:
        print(f'{target_part}: {type_part}')
      elif target_part or type_part:
        print(target_part or type_part)


if __name__ == '__main__':
  main()
