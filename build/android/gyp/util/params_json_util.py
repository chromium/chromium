# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides functions for reading and parsing .params.json files.

This module is used by build scripts to access parameters of GN targets and
their transitive dependencies.

It exposes helper methods to:
  * Read and cache JSON files.
  * Access transitive dependencies.
  * Filter and collect information from dependencies.

The main entry point is `get_params()`, which returns a `ParamsJson` object.
This object is a dictionary-like view of a .params.json file with added helper
methods for dependency traversal.
"""

import functools
import json
import os

# Types that should never be used as a dependency of another build config.
_ROOT_TYPES = frozenset([
    'android_apk', 'java_binary', 'java_annotation_processor',
    'robolectric_binary', 'android_app_bundle'
])
# Types that should not allow code deps to pass through.
_RESOURCE_TYPES = frozenset(['android_assets', 'android_resources'])

_COMPILE_RESOURCES_TYPES = frozenset([
    'android_apk',
    'android_app_bundle_module',
    'robolectric_binary',
])

_MERGES_MANIFESTS_TYPES = _COMPILE_RESOURCES_TYPES

_COLLECTS_NATIVE_LIBRARIES_TYPES = frozenset([
    'android_apk',
    'android_app_bundle_module',
    'robolectric_binary',
])

_COMPILE_TYPES = frozenset([
    'android_apk',
    'android_app_bundle_module',
    'java_annotation_processor',
    'java_binary',
    'java_library',
    'robolectric_binary',
])

_CLASSPATH_TYPES = frozenset(list(_COMPILE_TYPES) + [
    'dist_aar',
    'dist_jar',
])

# Track inputs for use in depfiles.
_input_paths = []

# By default scripts are run from the output directory, otherwise call
# set_output_dir() before using methods in this module.
_output_dir_path = ''


def set_output_dir(path):
  """Resolve paths relative to this directory."""
  global _output_dir_path
  _output_dir_path = path


def all_read_file_paths():
  """Returns a list of all paths read by _get_json()."""
  return list(_input_paths)


def _get_json(path):
  """Reads a JSON file and records the path for depfile tracking."""
  path = os.path.join(_output_dir_path, path)
  _input_paths.append(path)
  with open(path, encoding='utf-8') as f:
    config = json.load(f)
  return config


@functools.cache  # pylint: disable=method-cache-max-size-none
def get_build_config(path):
  """Cached version of _get_json() for .build_config.json files."""
  return _get_json(path)


@functools.cache  # pylint: disable=method-cache-max-size-none
def get_params(path):
  """Returns a cached, dictionary-like object for a .params.json file."""
  # It's important to cache the ParamsJson object rather than the json dict
  # because some ParamsJson methods use @cache.
  return ParamsJson(path, _get_json(path))


def _topological_walk(top, deps_func):
  """Gets the list of all transitive dependencies in topological order.

  Args:
    top: A list of the top level nodes
    deps_func: A function that takes a node and returns a list of its direct
        dependencies.
  Returns:
    A list of all transitive dependencies of nodes in top in order (a node will
    appear in the list at a lower index than all of its dependencies).
  """
  seen = {}

  def discover(nodes):
    for node in nodes:
      if node in seen:
        continue
      deps = deps_func(node)
      discover(deps)
      seen[node] = True

  discover(top)
  return list(reversed(seen))


def _filter_deps(deps, restrict_to_resource_types=False):
  """Filters dependencies based on their type."""
  if restrict_to_resource_types:
    # Consider groups that set input_jars_paths() as java targets. This
    # prevents such groups from contributing to the classpath when they are
    # depended on via resource deps. Admittedly, a bit of a hack...
    keep_func = lambda x: x.is_resource_type() or (x.is_group() and not x.get(
        'input_jars_paths'))

  else:
    keep_func = lambda x: not x.is_root_type()

  return [d for d in deps if keep_func(d)]


def _collect_public_deps(deps):
  """Recursively collects all public_deps from a list of dependencies."""
  ret = []
  # public_deps() contains public_deps of public_deps, so no need to recurse.
  for x in deps:
    ret += x.public_deps()
  return ret


def _deps_for_traversal(dep):
  """Returns the direct dependencies of a node for traversal."""
  # Do not let restrict_to_resource_types targets traverse groups directly.
  # rely on the group's public_deps (which are filtered by
  # restrict_to_resource_types) to surface the deps.
  return [] if dep.is_group() else dep.deps()


class _HashableList(list):
  """A list that can be used in a set / dict key."""

  def __init__(self, *args, sealed=False, **kwargs):
    super().__init__(*args, **kwargs)
    self._sealed = sealed

  def __hash__(self):
    self._sealed = True
    return id(self)

  def __iadd__(self, other):
    self._check_not_sealed()
    return super().__iadd__(other)

  def __add__(self, other):
    return DepsList(super().__add__(other))

  def _check_not_sealed(self):
    assert not self._sealed, 'Cannot mutate a DepsList after it has been used.'

  def append(self, value):
    self._check_not_sealed()
    return super().append(value)

  def extend(self, iterable):
    self._check_not_sealed()
    return super().extend(iterable)

  def insert(self, index, value):
    self._check_not_sealed()
    return super().insert(index, value)

  def remove(self, value):
    self._check_not_sealed()
    return super().remove(value)

  def pop(self, index=-1):
    self._check_not_sealed()
    return super().pop(index)

  def clear(self):
    self._check_not_sealed()
    return super().clear()

  def sort(self, *, key=None, reverse=False):
    self._check_not_sealed()
    return super().sort(key=key, reverse=reverse)

  def reverse(self):
    self._check_not_sealed()
    return super().reverse()


class DepsList(_HashableList):
  """A list of ParamsJson objects with helper methods for traversal."""

  def __repr__(self):
    return ','.join(repr(x) for x in self)

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def recursive(self):
    """Returns all transitive dependencies."""
    # Reverse so that deps appear with higher indices.
    return self.walk(lambda x: True)

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def recursive_resource_deps(self):
    """Returns all transitive resource dependencies.

    This is a special traversal for resources, which have different rules
    for how they depend on other targets.
    """

    def helper(x):
      if not x.get('recursive_resource_deps'):
        return True
      # Get unfiltered deps so that libraries come through.
      ret = [get_params(p) for p in x.get('public_deps_configs', [])]
      ret += _collect_public_deps(ret)
      return ret

    return self.walk(helper)

  def of_type(self, target_type):
    """Filters the list to dependencies of a specific GN target type."""
    return DepsList(d for d in self if d.type == target_type)

  def not_of_type(self, target_type):
    """Filters the list to dependencies not of a specific GN target type."""
    return DepsList(d for d in self if d.type != target_type)

  def filter(self, cond):
    """Filters the list based on a custom condition."""
    return DepsList(p for p in self if cond(p))

  def walk(self, visit_func):
    """Performs a topological walk, allowing for custom traversal logic.

    Args:
      visit_func: A function called for each node. It can return:
          * True (or None): Recurse normally.
          * False: Prune this node and its descendants from the result.
          * A list: Use this list as the node's children instead of its
            actual dependencies.
    """
    to_prune = set()

    def deps_func(dep):
      children = visit_func(dep)

      if children in (None, True):
        children = _deps_for_traversal(dep)
      if children is False:
        to_prune.add(dep)
        children = []
      return children

    ret = _topological_walk(self, deps_func)
    if to_prune:
      ret = [x for x in ret if x not in to_prune]
    return DepsList(ret)

  def collect(self, key_name, flatten=False):
    """Collects values for a given key from all dependencies in the list.

    Args:
      key_name: The key to look up in each dependency's params.
      flatten: If True, and the values are lists, flatten them into a
          single list.
    """
    if flatten:
      ret = []
      for p in self:
        if values := p.get(key_name):
          ret += values
      return ret

    return [p[key_name] for p in self if key_name in p]


def _extract_native_libraries_from_runtime_deps(path):
  """Extracts a list of .so paths from a runtime_deps file."""
  with open(os.path.join(_output_dir_path, path), encoding='utf-8') as f:
    lines = f.read().splitlines()
  ret = [
      os.path.normpath(l.replace('lib.unstripped/', '')) for l in lines
      if l.endswith('.so')
  ]
  ret.reverse()
  return ret


class ParamsJson(dict):
  """A dictionary-like view of a .params.json file with helper methods."""

  def __init__(self, path, json_dict):
    super().__init__(json_dict)
    self.path = path
    self.type = self['type']

  def __hash__(self):
    return id(self)

  def __eq__(self, other):
    return self is other

  def __repr__(self):
    return f'<{self.path}>'

  def build_config_path(self, suffix='.build_config.json'):
    """Returns the .build_config.json path."""
    return self.path.replace('.params.json', suffix)

  def build_config_json(self):
    """Returns the parsed JSON of the .build_config.json."""
    return get_build_config(self.build_config_path())

  def javac_build_config_json(self):
    """Returns the parsed JSON of the .javac.build_config.json."""
    return get_build_config(self.build_config_path('.javac.build_config.json'))

  def manifest_build_config_json(self):
    """Returns the parsed JSON of the .manifest.build_config.json."""
    return get_build_config(
        self.build_config_path('.manifest.build_config.json'))

  def is_root_type(self):
    """Returns True if the target type is a "root" type (e.g., an APK)."""
    return self.type in _ROOT_TYPES

  def collects_resources(self):
    """Returns True if the target type collects Android resources."""
    return self.compiles_resources() or self.type == 'dist_aar'

  def compiles_resources(self):
    """Returns True if the target type runs compile_resources.py."""
    return self.type in _COMPILE_RESOURCES_TYPES

  def merges_manifests(self):
    """Returns True if the target type runs manifest_merger.py."""
    return self.type in _MERGES_MANIFESTS_TYPES

  def needs_full_javac_classpath(self):
    """Returns True if the target type runs manifest_merger.py."""
    return self.type in ('android_apk', 'android_app_bundle_module') or (
        self.is_library() and self.get('needs_full_javac_classpath',
                                       False)) or (self.type == 'dist_jar'
                                                   and self.requires_android())

  def collects_dex_paths(self):
    """Returns True if the target type collects transitive .dex files."""
    if self.type in ('dist_aar', 'dist_jar'):
      return self.supports_android()
    if self.is_bundle_module():
      return not self.get('proguard_enabled')
    return self.is_apk()

  def collects_processed_classpath(self):
    """Returns True if the target type collects the processed classpath."""
    if self.get('dex_needs_classpath'):
      return True
    if self.type in ('dist_aar', 'dist_jar', 'java_binary',
                     'robolectric_binary'):
      return True
    if self.is_apk() or self.is_bundle() or self.is_bundle_module():
      # Required for is_bundle_module only because write_build_config.py uses
      # them as inputs.
      return self.get('proguard_enabled', False)
    return False

  def collects_native_libraries(self):
    """Returns True if the target type collects native libraries."""
    return self.type in _COLLECTS_NATIVE_LIBRARIES_TYPES

  def has_classpath(self):
    """Returns True if the target type has a classpath."""
    if self.is_library():
      return bool(self.get('dex_needs_classpath') or not self.is_prebuilt())
    return self.type in _CLASSPATH_TYPES

  def is_compile_type(self):
    """Returns True if the target has a compile step."""
    return not self.is_prebuilt() and self.type in _COMPILE_TYPES

  def needs_transitive_rtxt(self):
    """Returns True if the target populates "dependency_rtxt_files"."""
    return self.type == 'dist_aar' or (self.is_library()
                                       and not self.is_prebuilt())

  def is_prebuilt(self):
    """If it's a java_library prebuilt."""
    return self.is_library() and self.get('is_prebuilt', False)

  def is_resource_type(self):
    """Returns True if the target is an Android resource type."""
    return self.type in _RESOURCE_TYPES

  def is_apk(self):
    return self.type == 'android_apk'

  def is_bundle(self):
    return self.type == 'android_app_bundle'

  def is_bundle_module(self):
    return self.type == 'android_app_bundle_module'

  def is_dist_xar(self):
    return self.type in ('dist_aar', 'dist_jar')

  def is_group(self):
    return self.type == 'group'

  def is_library(self):
    return self.type == 'java_library'

  def is_system_library(self):
    return self.type == 'system_java_library'

  def requires_android(self):
    """Returns True if the target requires the Android platform."""
    if self.type.startswith('android') or self.type == 'dist_aar':
      return True
    return self.is_resource_type() or self.get('requires_android', False)

  def supports_android(self):
    """Returns True if the target supports the Android platform."""
    return self.requires_android() or self.get('supports_android', True)

  def _direct_deps(self):
    """Returns only the direct dependencies (from `deps_configs`)."""
    # android_resources use only public_dep_configs, so no need for
    # restrict_to_resource_types.
    return [get_params(p) for p in self.get('deps_configs', [])]

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def _cached_direct_public_deps(self):
    """Returns only the direct public dependencies."""
    deps = [get_params(p) for p in self.get('public_deps_configs', [])]
    return _filter_deps(deps,
                        restrict_to_resource_types=self.is_resource_type())

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def deps(self):
    """Returns deps + resolved public_deps."""
    deps = DepsList(self._direct_deps())
    deps += self._cached_direct_public_deps()
    deps += _filter_deps(_collect_public_deps(deps),
                         restrict_to_resource_types=self.is_resource_type())
    # Return a cached DepsList so that multiple calls to .deps().recursive()
    # result in a cache hit.
    return DepsList(dict.fromkeys(deps), sealed=True)

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def public_deps(self):
    """Returns direct public dependencies and their transitive public_deps."""
    deps = self._cached_direct_public_deps()
    deps = deps + _filter_deps(
        _collect_public_deps(deps),
        restrict_to_resource_types=self.is_resource_type())
    return DepsList(deps, sealed=True)

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def processor_deps(self):
    """Returns all transitive annotation processor dependencies."""
    deps = [get_params(p) for p in self.get('processor_configs', [])]
    deps += _filter_deps(_collect_public_deps(deps))
    return DepsList(dict.fromkeys(deps), sealed=True)

  def apk_under_test(self):
    """Returns the ParamsJson for the apk_under_test, or None."""
    if path := self.get('apk_under_test_config'):
      return get_params(path)
    return None

  @functools.cache  # pylint: disable=method-cache-max-size-none
  def module_deps(self):
    """For a bundle, returns the ParamsJson for all module dependencies."""
    deps = sorted(self.deps().of_type('android_app_bundle_module'),
                  key=lambda x: x['module_name'])
    if not self.is_bundle():
      return deps
    base_module = self.base_module()
    ret = {base_module: 1}
    ret.update(
        dict.fromkeys(x for x in deps if x.parent_module() is base_module))
    ret.update(dict.fromkeys(deps))
    return list(ret)

  def parent_module(self):
    """For a bundle module, returns its direct parent module."""
    assert self.is_bundle_module(), 'got: ' + self.type
    module_deps = self.module_deps()

    if self['module_name'] == 'base':
      assert not module_deps, ('Base module should not depend on ' +
                               ','.join(module_deps.collect('module_name')))
      return None

    assert len(module_deps) != 0, 'Should depend on base module'
    assert len(module_deps) == 1, (
        'Can depend on only one parent module. Found: ' +
        ','.join(module_deps.collect('module_name')))
    return module_deps[0]

  def base_module(self):
    """For a bundle module, returns the root 'base' module."""
    if self.is_bundle():
      return next(x for x in self.deps() if x.get('module_name') == 'base')

    assert self.is_bundle_module(), 'got: ' + self.type
    # Find the base split.
    ret = self
    while not ret.is_base_module():
      ret = ret.parent_module()
    return ret

  def is_base_module(self):
    """Returns True if this is the base module of an app bundle."""
    return self.is_bundle_module() and self.parent_module() is None

  def resource_deps(self):
    """Returns the transitive resource dependencies."""
    # For Java libraries, restrict to resource targets that are direct deps, or
    # are indirect via other resource targets.
    if self.is_library():
      return self.deps().of_type('android_resources')
    return self.deps().recursive_resource_deps().of_type('android_resources')

  def native_libraries(self):
    if path := self.get('shared_libraries_runtime_deps_file'):
      return _extract_native_libraries_from_runtime_deps(path)
    return []

  def secondary_abi_native_libraries(self):
    if path := self.get('secondary_abi_shared_libraries_runtime_deps_file'):
      return _extract_native_libraries_from_runtime_deps(path)
    return []
