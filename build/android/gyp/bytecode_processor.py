#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps bin/helper/bytecode_processor and expands @FileArgs."""

import argparse
import collections
import logging
import os
import pathlib
import sys
from typing import Dict, List

from util import build_utils
from util import dep_utils
from util import jar_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.

_SRC_PATH = pathlib.Path(build_utils.DIR_SOURCE_ROOT).resolve()
sys.path.append(str(_SRC_PATH / 'tools/android/modularization/gn'))
from dep_operations import NO_VALID_GN_STR


def _ShouldIgnoreDep(dep_name: str):
  if 'gen.base_module.R' in dep_name:
    return True
  return False


def _ParseDepGraph(jar_path: str, output_dir: str):
  output = jar_utils.run_jdeps(build_output_dir=pathlib.Path(output_dir),
                               filepath=pathlib.Path(jar_path))
  assert output is not None, f'Unable to parse jdep for {jar_path}'
  dep_graph = collections.defaultdict(set)
  # pylint: disable=line-too-long
  # Example output:
  # java.javac.jar -> java.base
  # java.javac.jar -> not found
  #    org.chromium.chrome.browser.tabmodel.AsyncTabParamsManagerFactory -> java.lang.Object java.base
  #    org.chromium.chrome.browser.tabmodel.TabWindowManagerImpl -> org.chromium.base.ApplicationStatus not found
  #    org.chromium.chrome.browser.tabmodel.TabWindowManagerImpl -> org.chromium.base.ApplicationStatus$ActivityStateListener not found
  #    org.chromium.chrome.browser.tabmodel.TabWindowManagerImpl -> org.chromium.chrome.browser.tab.Tab not found
  # pylint: enable=line-too-long
  for line in output.splitlines():
    parsed = line.split()
    # E.g. java.javac.jar -> java.base
    if len(parsed) <= 3:
      continue
    # E.g. java.javac.jar -> not found
    if parsed[2] == 'not' and parsed[3] == 'found':
      continue
    if parsed[1] != '->':
      continue
    dep_from = parsed[0]
    dep_to = parsed[2]
    dep_graph[dep_from].add(dep_to)
  return dep_graph


def _GnTargetToBuildFilePath(gn_target: str):
  """Returns the relative BUILD.gn file path for this target from src root."""
  assert gn_target.startswith('//'), f'Relative {gn_target} name not supported.'
  ninja_target_name = gn_target[2:]

  # Remove the colon at the end
  colon_index = ninja_target_name.find(':')
  if colon_index != -1:
    ninja_target_name = ninja_target_name[:colon_index]

  return os.path.join(ninja_target_name, 'BUILD.gn')


def _EnsureDirectClasspathIsComplete(
    *,
    input_jar: str,
    gn_target: str,
    output_dir: str,
    sdk_classpath_jars: List[str],
    direct_classpath_jars: List[str],
    full_classpath_jars: List[str],
    full_classpath_gn_targets: List[str],
    warnings_as_errors: bool,
):
  logging.info('Parsing %d direct classpath jars', len(sdk_classpath_jars))
  sdk_classpath_deps = set()
  for jar in sdk_classpath_jars:
    deps = jar_utils.extract_full_class_names_from_jar(
        build_output_dir=pathlib.Path(output_dir), jar_path=pathlib.Path(jar))
    sdk_classpath_deps.update(deps)

  logging.info('Parsing %d direct classpath jars', len(direct_classpath_jars))
  direct_classpath_deps = set()
  for jar in direct_classpath_jars:
    deps = jar_utils.extract_full_class_names_from_jar(
        build_output_dir=pathlib.Path(output_dir), jar_path=pathlib.Path(jar))
    direct_classpath_deps.update(deps)

  logging.info('Parsing %d full classpath jars', len(full_classpath_jars))
  full_classpath_deps = set()
  dep_to_target = collections.defaultdict(set)
  for jar, target in zip(full_classpath_jars, full_classpath_gn_targets):
    deps = jar_utils.extract_full_class_names_from_jar(
        build_output_dir=pathlib.Path(output_dir), jar_path=pathlib.Path(jar))
    full_classpath_deps.update(deps)
    for dep in deps:
      dep_to_target[dep].add(target)

  transitive_deps = full_classpath_deps - direct_classpath_deps

  missing_targets: Dict[tuple, Dict[str, str]] = collections.defaultdict(dict)
  dep_graph = _ParseDepGraph(input_jar, output_dir)
  logging.info('Finding missing deps from %d classes', len(dep_graph))
  # dep_graph.keys() is a list of all the classes in the current input_jar. Skip
  # all of these to avoid checking dependencies in the same target (e.g. A
  # depends on B, but both A and B are in input_jar).
  # Since the bundle will always have access to classes in the current android
  # sdk, those should not be considered missing.
  seen_deps = set(dep_graph.keys()) | sdk_classpath_deps
  for dep_from, deps_to in dep_graph.items():
    for dep_to in deps_to - seen_deps:
      if _ShouldIgnoreDep(dep_to):
        continue
      seen_deps.add(dep_to)
      if dep_to in transitive_deps:
        missing_target_names = tuple(sorted(dep_to_target[dep_to]))
        missing_targets[missing_target_names][dep_to] = dep_from

  if missing_targets:

    def print_and_maybe_exit():
      print('=' * 30 + ' Dependency Checks Failed ' + '=' * 30)
      print(f'Target: {gn_target}')
      print('Direct classpath is incomplete. To fix, add deps on:')
      for missing_target_names, data in missing_targets.items():
        if len(missing_target_names) > 1:
          print(f' * One of {", ".join(missing_target_names)}')
        else:
          print(f' * {missing_target_names[0]}')
        for missing_class, used_by in data.items():
          print(f'     ** {missing_class} (needed by {used_by})')
      if warnings_as_errors:
        sys.exit(1)

    # TODO(https://crbug.com/1099522): This is better as a GN arg.
    if os.environ.get('AUTO_ADD_MISSING_DEPS') != '1':
      print_and_maybe_exit()
    else:
      # TODO(https://crbug.com/1099522): This should be generalized into util.
      build_file_path = _GnTargetToBuildFilePath(gn_target)
      cmd = [
          'tools/android/modularization/gn/dep_operations.py', 'add', '--quiet',
          '--file', build_file_path, '--target', gn_target, '--deps'
      ]
      # For simplicity, always pick the first suggested target.
      # TODO(https://crbug.com/1099522): Swap deps with preferred deps.
      missing_deps = [names[0] for names in missing_targets.keys()]
      cmd += missing_deps
      try:
        build_utils.CheckOutput(cmd, cwd=build_utils.DIR_SOURCE_ROOT)
      except build_utils.CalledProcessError as e:
        if NO_VALID_GN_STR in e.output:
          print(f'Unable to add missing dep(s) to {build_file_path}.')
          print_and_maybe_exit()
        else:
          raise
      else:
        print(f'Successfully updated {build_file_path} with missing direct '
              f'deps: {missing_deps}')


def _AddSwitch(parser, val):
  parser.add_argument(
      val, action='store_const', default='--disabled', const=val)


def main(argv):
  build_utils.InitLogging('BYTECODE_PROCESSOR_DEBUG')
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  parser.add_argument('--target-name', help='Fully qualified GN target name.')
  parser.add_argument('--use-build-server',
                      action='store_true',
                      help='Always use the build server.')
  parser.add_argument('--script', required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--gn-target', required=True)
  parser.add_argument('--input-jar', required=True)
  parser.add_argument('--direct-classpath-jars')
  parser.add_argument('--sdk-classpath-jars')
  parser.add_argument('--full-classpath-jars')
  parser.add_argument('--full-classpath-gn-targets')
  parser.add_argument('--chromium-output-dir')
  parser.add_argument('--stamp')
  parser.add_argument('-v', '--verbose', action='store_true')
  parser.add_argument('--missing-classes-allowlist')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  _AddSwitch(parser, '--is-prebuilt')
  args = parser.parse_args(argv)

  if server_utils.MaybeRunCommand(name=args.target_name,
                                  argv=sys.argv,
                                  stamp_file=args.stamp,
                                  force=args.use_build_server):
    return

  args.sdk_classpath_jars = action_helpers.parse_gn_list(
      args.sdk_classpath_jars)
  args.direct_classpath_jars = action_helpers.parse_gn_list(
      args.direct_classpath_jars)
  args.full_classpath_jars = action_helpers.parse_gn_list(
      args.full_classpath_jars)
  args.full_classpath_gn_targets = [
      dep_utils.ReplaceGmsPackageIfNeeded(t)
      for t in action_helpers.parse_gn_list(args.full_classpath_gn_targets)
  ]
  args.missing_classes_allowlist = action_helpers.parse_gn_list(
      args.missing_classes_allowlist)

  verbose = '--verbose' if args.verbose else '--not-verbose'


  # TODO(https://crbug.com/1099522): Make jdeps the default.
  if os.environ.get('BYTECODE_PROCESSOR_USE_JDEPS'):
    logging.info('Processed args for %s, starting direct classpath check.',
                 args.target_name)
    _EnsureDirectClasspathIsComplete(
        input_jar=args.input_jar,
        gn_target=args.gn_target,
        output_dir=args.chromium_output_dir,
        sdk_classpath_jars=args.sdk_classpath_jars,
        direct_classpath_jars=args.direct_classpath_jars,
        full_classpath_jars=args.full_classpath_jars,
        full_classpath_gn_targets=args.full_classpath_gn_targets,
        warnings_as_errors=args.warnings_as_errors,
    )
    logging.info('Check completed.')
  else:
    cmd = [
        args.script, args.gn_target, args.input_jar, verbose, args.is_prebuilt
    ]
    cmd += [str(len(args.missing_classes_allowlist))]
    cmd += args.missing_classes_allowlist
    cmd += [str(len(args.sdk_classpath_jars))]
    cmd += args.sdk_classpath_jars
    cmd += [str(len(args.direct_classpath_jars))]
    cmd += args.direct_classpath_jars
    cmd += [str(len(args.full_classpath_jars))]
    cmd += args.full_classpath_jars
    cmd += [str(len(args.full_classpath_gn_targets))]
    cmd += args.full_classpath_gn_targets
    try:
      build_utils.CheckOutput(cmd,
                              print_stdout=True,
                              fail_func=None,
                              fail_on_output=args.warnings_as_errors)
    except build_utils.CalledProcessError as e:
      # Do not output command line because it is massive and makes the actual
      # error message hard to find.
      sys.stderr.write(e.output)
      sys.exit(1)

  if args.stamp:
    build_utils.Touch(args.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
