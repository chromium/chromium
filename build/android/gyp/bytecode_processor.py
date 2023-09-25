#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps bin/helper/bytecode_processor and expands @FileArgs."""

import argparse
import collections
import json
import logging
import os
import pathlib
import sys
from typing import Dict, List, Tuple

from util import build_utils
from util import dep_utils
from util import jar_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.

_SRC_PATH = pathlib.Path(build_utils.DIR_SOURCE_ROOT).resolve()
sys.path.append(str(_SRC_PATH / 'build/gn_ast'))
from gn_editor import NO_VALID_GN_STR


def _ShouldIgnoreDep(dep_name: str):
  if 'gen.base_module.R' in dep_name:
    return True
  return False


def _ParseDepGraph(jar_path: str):
  output = jar_utils.run_jdeps(pathlib.Path(jar_path))
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
    auto_add_deps: bool,
):
  logging.info('Parsing %d direct classpath jars', len(sdk_classpath_jars))
  sdk_classpath_deps = set()
  for jar in sdk_classpath_jars:
    deps = jar_utils.extract_full_class_names_from_jar(jar)
    sdk_classpath_deps.update(deps)

  logging.info('Parsing %d direct classpath jars', len(direct_classpath_jars))
  direct_classpath_deps = set()
  for jar in direct_classpath_jars:
    deps = jar_utils.extract_full_class_names_from_jar(jar)
    direct_classpath_deps.update(deps)

  logging.info('Parsing %d full classpath jars', len(full_classpath_jars))
  full_classpath_deps = set()
  dep_to_target = collections.defaultdict(set)
  for jar, target in zip(full_classpath_jars, full_classpath_gn_targets):
    deps = jar_utils.extract_full_class_names_from_jar(jar)
    full_classpath_deps.update(deps)
    for dep in deps:
      dep_to_target[dep].add(target)

  transitive_deps = full_classpath_deps - direct_classpath_deps

  missing_classes: Dict[str, str] = {}
  dep_graph = _ParseDepGraph(input_jar)
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
        # Allow clobbering since it doesn't matter which specific class depends
        # on |dep_to|.
        missing_classes[dep_to] = dep_from

        # missing_target_names = tuple(sorted(dep_to_target[dep_to]))
        # missing_targets[missing_target_names][dep_to] = dep_from
  if missing_classes:

    def print_and_maybe_exit():
      missing_targets: Dict[Tuple, List[str]] = collections.defaultdict(list)
      for dep_to, dep_from in missing_classes.items():
        missing_target_names = tuple(sorted(dep_to_target[dep_to]))
        missing_targets[missing_target_names].append(dep_to)
      print('=' * 30 + ' Dependency Checks Failed ' + '=' * 30)
      print(f'Target: {gn_target}')
      print('Direct classpath is incomplete. To fix, add deps on:')
      for missing_target_names, deps_to in missing_targets.items():
        if len(missing_target_names) > 1:
          print(f' * One of {", ".join(missing_target_names)}')
        else:
          print(f' * {missing_target_names[0]}')
        for dep_to in deps_to:
          dep_from = missing_classes[dep_to]
          print(f'     ** {dep_to} (needed by {dep_from})')
      if warnings_as_errors:
        sys.exit(1)

    if not auto_add_deps:
      print_and_maybe_exit()
    else:
      class_lookup_index = dep_utils.ClassLookupIndex(pathlib.Path(output_dir),
                                                      should_build=False)
      missing_deps = set()
      for dep_to in missing_classes:
        # Using dep_utils.ClassLookupIndex ensures we respect the preferred dep
        # if any exists for the missing deps.
        suggested_deps = class_lookup_index.match(dep_to)
        assert suggested_deps, f'Unable to find target for {dep_to}'
        suggested_deps = dep_utils.DisambiguateDeps(suggested_deps)
        missing_deps.add(suggested_deps[0].target)
      cmd = dep_utils.CreateAddDepsCommand(gn_target, sorted(missing_deps))

      failed = False
      try:
        stdout = build_utils.CheckOutput(cmd,
                                         cwd=build_utils.DIR_SOURCE_ROOT,
                                         fail_on_output=warnings_as_errors)
        if f'Unable to find {gn_target}' in stdout:
          # This can happen if a target's deps are stored in a variable instead
          # of a list and then simply assigned: `deps = deps_variable`. These
          # need to be manually added to the `deps_variable`.
          failed = True
      except build_utils.CalledProcessError as e:
        if NO_VALID_GN_STR in e.output:
          failed = True
        else:
          raise

      build_file_path = dep_utils.GnTargetToBuildFilePath(gn_target)
      if failed:
        print(f'Unable to auto-add missing dep(s) to {build_file_path}.')
        print_and_maybe_exit()
      else:
        gn_target_name = gn_target.split(':', 1)[-1]
        print(f'Successfully updated "{gn_target_name}" in {build_file_path} '
              f'with missing direct deps: {missing_deps}')


def main(argv):
  build_utils.InitLogging('BYTECODE_PROCESSOR_DEBUG')
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  parser.add_argument('--target-name', help='Fully qualified GN target name.')
  parser.add_argument('--use-build-server',
                      action='store_true',
                      help='Always use the build server.')
  parser.add_argument('--gn-target', required=True)
  parser.add_argument('--input-jar', required=True)
  parser.add_argument('--direct-classpath-jars')
  parser.add_argument('--sdk-classpath-jars')
  parser.add_argument('--full-classpath-jars')
  parser.add_argument('--full-classpath-gn-targets')
  parser.add_argument('--chromium-output-dir')
  parser.add_argument('--stamp')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument(
      '--auto-add-deps',
      action='store_true',
      help='Attempt to automatically add missing deps to the corresponding '
      'BUILD.gn file.')
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
      auto_add_deps=args.auto_add_deps,
  )
  logging.info('Check completed.')

  if args.stamp:
    build_utils.Touch(args.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
