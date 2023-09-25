# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods for managing deps based on build_config.json files."""

from __future__ import annotations
import collections

import dataclasses
import json
import logging
import os
import pathlib
import subprocess
import sys
from typing import Dict, Iterator, List, Set

from util import jar_utils

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]

sys.path.append(str(_SRC_PATH / 'build/android'))
# Import list_java_targets so that the dependency is found by print_python_deps.
import list_java_targets


@dataclasses.dataclass(frozen=True)
class ClassEntry:
  """An assignment of a Java class to a build target."""
  full_class_name: str
  target: str
  preferred_dep: bool

  def __lt__(self, other: 'ClassEntry'):
    # Prefer canonical targets first.
    if self.preferred_dep and not other.preferred_dep:
      return True
    # Prefer targets without __ in the name. Usually double underscores are used
    # for internal subtargets and not top level targets.
    if '__' not in self.target and '__' in other.target:
      return True
    # Prefer shorter target names first since they are usually the correct ones.
    if len(self.target) < len(other.target):
      return True
    if len(self.target) > len(other.target):
      return False
    # Use string comparison to get a stable ordering of equal-length names.
    return self.target < other.target


@dataclasses.dataclass
class BuildConfig:
  """Container for information from a build config."""
  target_name: str
  relpath: str
  is_group: bool
  preferred_dep: bool
  dependent_config_paths: List[str]
  full_class_names: Set[str]

  def all_dependent_configs(
      self,
      path_to_configs: Dict[str, 'BuildConfig'],
  ) -> Iterator['BuildConfig']:
    for path in self.dependent_config_paths:
      dep_build_config = path_to_configs.get(path)
      # This can happen when a java group depends on non-java targets.
      if dep_build_config is None:
        continue
      yield dep_build_config
      if dep_build_config.is_group:
        yield from dep_build_config.all_dependent_configs(path_to_configs)


class ClassLookupIndex:
  """A map from full Java class to its build targets.

  A class might be in multiple targets if it's bytecode rewritten."""
  def __init__(self, build_output_dir: pathlib.Path, should_build: bool):
    self._abs_build_output_dir = build_output_dir.resolve().absolute()
    self._should_build = should_build
    self._class_index = self._index_root()

  def match(self, search_string: str) -> List[ClassEntry]:
    """Get class/target entries where the class matches search_string"""
    # Priority 1: Exact full matches
    if search_string in self._class_index:
      return self._entries_for(search_string)

    # Priority 2: Match full class name (any case), if it's a class name
    matches = []
    lower_search_string = search_string.lower()
    if '.' not in lower_search_string:
      for full_class_name in self._class_index:
        package_and_class = full_class_name.rsplit('.', 1)
        if len(package_and_class) < 2:
          continue
        class_name = package_and_class[1]
        class_lower = class_name.lower()
        if class_lower == lower_search_string:
          matches.extend(self._entries_for(full_class_name))
      if matches:
        return matches

    # Priority 3: Match anything
    for full_class_name in self._class_index:
      if lower_search_string in full_class_name.lower():
        matches.extend(self._entries_for(full_class_name))

    # Priority 4: Match parent class when no matches and it's an inner class.
    if not matches:
      components = search_string.rsplit('.', 2)
      if len(components) == 3:
        package, outer_class, inner_class = components
        if outer_class[0].isupper() and inner_class[0].isupper():
          matches.extend(self.match(f'{package}.{outer_class}'))

    return matches

  def _entries_for(self, class_name) -> List[ClassEntry]:
    return sorted(self._class_index[class_name])

  def _index_root(self) -> Dict[str, Set[ClassEntry]]:
    """Create the class to target index."""
    logging.debug('Running list_java_targets.py...')
    list_java_targets_command = [
        'build/android/list_java_targets.py', '--gn-labels',
        '--print-build-config-paths',
        f'--output-directory={self._abs_build_output_dir}'
    ]
    if self._should_build:
      list_java_targets_command += ['--build']

    list_java_targets_run = subprocess.run(list_java_targets_command,
                                           cwd=_SRC_PATH,
                                           capture_output=True,
                                           text=True,
                                           check=True)
    logging.debug('... done.')

    # Parse output of list_java_targets.py into BuildConfig objects.
    path_to_build_config: Dict[str, BuildConfig] = {}
    target_lines = list_java_targets_run.stdout.splitlines()
    for target_line in target_lines:
      # Skip empty lines
      if not target_line:
        continue

      target_line_parts = target_line.split(': ')
      assert len(target_line_parts) == 2, target_line_parts
      target_name, build_config_path = target_line_parts

      if not os.path.exists(build_config_path):
        assert not self._should_build
        continue

      with open(build_config_path) as build_config_contents:
        build_config_json: Dict = json.load(build_config_contents)
      deps_info = build_config_json['deps_info']

      # Checking the library type here instead of in list_java_targets.py avoids
      # reading each .build_config file twice.
      if deps_info['type'] not in ('java_library', 'group'):
        continue

      relpath = os.path.relpath(build_config_path, self._abs_build_output_dir)
      preferred_dep = bool(deps_info.get('preferred_dep'))
      is_group = bool(deps_info.get('type') == 'group')
      dependent_config_paths = deps_info.get('deps_configs', [])
      full_class_names = self._compute_full_class_names_for_build_config(
          deps_info)
      build_config = BuildConfig(relpath=relpath,
                                 target_name=target_name,
                                 is_group=is_group,
                                 preferred_dep=preferred_dep,
                                 dependent_config_paths=dependent_config_paths,
                                 full_class_names=full_class_names)
      path_to_build_config[relpath] = build_config

    # From GN's perspective, depending on a java group is the same as depending
    # on all of its deps directly, since groups are collapsed in
    # write_build_config.py. Thus, collect all the java files in a java group's
    # deps (recursing into other java groups) and set that as the java group's
    # list of classes.
    for build_config in path_to_build_config.values():
      if build_config.is_group:
        for dep_build_config in build_config.all_dependent_configs(
            path_to_build_config):
          build_config.full_class_names.update(
              dep_build_config.full_class_names)

    class_index = collections.defaultdict(set)
    for build_config in path_to_build_config.values():
      for full_class_name in build_config.full_class_names:
        class_index[full_class_name].add(
            ClassEntry(full_class_name=full_class_name,
                       target=build_config.target_name,
                       preferred_dep=build_config.preferred_dep))

    return class_index

  def _compute_full_class_names_for_build_config(self,
                                                 deps_info: Dict) -> Set[str]:
    """Returns set of fully qualified class names for build config."""

    full_class_names = set()

    # Read the location of the target_sources_file from the build_config
    sources_path = deps_info.get('target_sources_file')
    if sources_path:
      # Read the target_sources_file, indexing the classes found
      with open(self._abs_build_output_dir / sources_path) as sources_contents:
        for source_line in sources_contents:
          source_path = pathlib.Path(source_line.strip())
          java_class = jar_utils.parse_full_java_class(source_path)
          if java_class:
            full_class_names.add(java_class)

    # |unprocessed_jar_path| is set for prebuilt targets. (ex:
    # android_aar_prebuilt())
    # |unprocessed_jar_path| might be set but not exist if not all targets have
    # been built.
    unprocessed_jar_path = deps_info.get('unprocessed_jar_path')
    if unprocessed_jar_path:
      abs_unprocessed_jar_path = (self._abs_build_output_dir /
                                  unprocessed_jar_path)
      if abs_unprocessed_jar_path.exists():
        # Normalize path but do not follow symlink if .jar is symlink.
        abs_unprocessed_jar_path = (abs_unprocessed_jar_path.parent.resolve() /
                                    abs_unprocessed_jar_path.name)

        full_class_names.update(
            jar_utils.extract_full_class_names_from_jar(
                abs_unprocessed_jar_path))

    return full_class_names


def GnTargetToBuildFilePath(gn_target: str):
  """Returns the relative BUILD.gn file path for this target from src root."""
  assert gn_target.startswith('//'), f'Relative {gn_target} name not supported.'
  ninja_target_name = gn_target[2:]

  # Remove the colon at the end
  colon_index = ninja_target_name.find(':')
  if colon_index != -1:
    ninja_target_name = ninja_target_name[:colon_index]

  return os.path.join(ninja_target_name, 'BUILD.gn')


def CreateAddDepsCommand(gn_target: str, missing_deps: List[str]) -> List[str]:
  # Normalize chrome_public_apk__java to chrome_public_apk.
  gn_target = gn_target.split('__', 1)[0]

  build_file_path = GnTargetToBuildFilePath(gn_target)
  return [
      'build/gn_editor', 'add', '--quiet', '--file', build_file_path,
      '--target', gn_target, '--deps'
  ] + missing_deps


def ReplaceGmsPackageIfNeeded(target_name: str) -> str:
  if target_name.startswith(
      ('//third_party/android_deps:google_play_services_',
       '//clank/third_party/google3:google_play_services_')):
    return f'$google_play_services_package:{target_name.split(":")[1]}'
  return target_name


def DisambiguateDeps(class_entries: List[ClassEntry]):
  def filter_if_not_empty(entries, filter_func):
    filtered_entries = [e for e in entries if filter_func(e)]
    return filtered_entries or entries

  # When some deps are preferred, ignore all other potential deps.
  class_entries = filter_if_not_empty(class_entries, lambda e: e.preferred_dep)

  # E.g. javax_annotation_jsr250_api_java.
  class_entries = filter_if_not_empty(class_entries,
                                      lambda e: 'jsr' in e.target)

  # Avoid suggesting subtargets when regular targets exist.
  class_entries = filter_if_not_empty(class_entries,
                                      lambda e: '__' not in e.target)

  # Swap out GMS package names if needed.
  class_entries = [
      dataclasses.replace(e, target=ReplaceGmsPackageIfNeeded(e.target))
      for e in class_entries
  ]

  # Convert to dict and then use list to get the keys back to remove dups and
  # keep order the same as before.
  class_entries = list({e: True for e in class_entries})

  return class_entries
