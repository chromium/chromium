#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps bin/helper/bytecode_processor and expands @FileArgs."""

import argparse
import collections
import logging
import pathlib
import shlex
import sys
from typing import Dict, List, Optional, Set, Tuple

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

  missing_class_to_caller: Dict[str, str] = {}
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
        missing_class_to_caller[dep_to] = dep_from

  if missing_class_to_caller:
    _ProcessMissingDirectClasspathDeps(missing_class_to_caller, dep_to_target,
                                       gn_target, output_dir,
                                       warnings_as_errors, auto_add_deps)


def _ProcessMissingDirectClasspathDeps(
    missing_class_to_caller: Dict[str, str],
    dep_to_target: Dict[str, Set[str]],
    gn_target: str,
    output_dir: str,
    warnings_as_errors: bool,
    auto_add_deps: bool,
):
  potential_targets_to_missing_classes: Dict[
      Tuple, List[str]] = collections.defaultdict(list)
  for missing_class in missing_class_to_caller:
    potential_targets = tuple(sorted(dep_to_target[missing_class]))
    potential_targets_to_missing_classes[potential_targets].append(
        missing_class)

  deps_to_add_programatically = _DisambiguateMissingDeps(
      potential_targets_to_missing_classes, output_dir=output_dir)

  cmd = dep_utils.CreateAddDepsCommand(gn_target,
                                       sorted(deps_to_add_programatically))

  if not auto_add_deps:
    _PrintAndMaybeExit(potential_targets_to_missing_classes,
                       missing_class_to_caller, gn_target, warnings_as_errors,
                       cmd)
  else:
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
      _PrintAndMaybeExit(potential_targets_to_missing_classes,
                         missing_class_to_caller, gn_target, warnings_as_errors)
    else:
      gn_target_name = gn_target.split(':', 1)[-1]
      print(f'Successfully updated "{gn_target_name}" in {build_file_path} '
            f'with missing direct deps: {deps_to_add_programatically}')


def _DisambiguateMissingDeps(
    potential_targets_to_missing_classes: Dict[Tuple, List[str]],
    output_dir: str,
):
  deps_to_add_programatically = set()
  class_lookup_index = None
  for (potential_targets,
       missing_classes) in potential_targets_to_missing_classes.items():
    # No need to disambiguate if there's just one choice.
    if len(potential_targets) == 1:
      deps_to_add_programatically.add(potential_targets[0])
      continue

    # Rather than just picking any of the potential targets, we want to use
    # dep_utils.ClassLookupIndex to ensure we respect the preferred dep if any
    # exists for the missing deps. It is necessary to obtain the preferred dep
    # status of these potential targets by matching them to a ClassEntry.
    target_name_to_class_entry: Dict[str, dep_utils.ClassEntry] = {}
    for missing_class in missing_classes:
      # Lazily create the ClassLookupIndex in case all potential_targets lists
      # are only 1 element in length.
      if class_lookup_index is None:
        class_lookup_index = dep_utils.ClassLookupIndex(
            pathlib.Path(output_dir), should_build=False)
      for class_entry in class_lookup_index.match(missing_class):
        target_name_to_class_entry[class_entry.target] = class_entry
    potential_class_entries = [
        target_name_to_class_entry[t] for t in potential_targets
    ]
    potential_class_entries = dep_utils.DisambiguateDeps(
        potential_class_entries)
    deps_to_add_programatically.add(potential_class_entries[0].target)
  return deps_to_add_programatically


def _PrintAndMaybeExit(
    potential_targets_to_missing_classes: Dict[Tuple, List[str]],
    missing_class_to_caller: Dict[str, str],
    gn_target: str,
    warnings_as_errors: bool,
    cmd: Optional[List[str]] = None,
):
  print('=' * 30 + ' Dependency Checks Failed ' + '=' * 30)
  print(f'Target: {gn_target}')
  print('Direct classpath is incomplete. To fix, add deps on:')
  for (potential_targets,
       missing_classes) in potential_targets_to_missing_classes.items():
    if len(potential_targets) == 1:
      print(f' * {potential_targets[0]}')
    else:
      print(f' * One of {", ".join(potential_targets)}')
    for missing_class in missing_classes:
      caller = missing_class_to_caller[missing_class]
      print(f'     ** {missing_class} (needed by {caller})')
  if cmd:
    print('\nHint: Run the following command to add the missing deps:')
    print(f'    {shlex.join(cmd)}\n')
  if warnings_as_errors:
    sys.exit(1)


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
      auto_add_deps=args.auto_add_deps)
  logging.info('Check completed.')

  if args.stamp:
    build_utils.Touch(args.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
