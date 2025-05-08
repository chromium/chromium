#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes a .build_config.json file.

See //build/android/docs/build_config.md
"""

import argparse
import collections
import itertools
import json
import os
import shutil
import sys
import xml.dom.minidom

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


# Types that should never be used as a dependency of another build config.
_ROOT_TYPES = ('android_apk', 'java_binary', 'java_annotation_processor',
               'robolectric_binary', 'android_app_bundle')
# Types that should not allow code deps to pass through.
_RESOURCE_TYPES = ('android_assets', 'android_resources', 'system_java_library')

# Cache of path -> JSON dict.
_dep_config_cache = {}


class OrderedSet(collections.OrderedDict):
  @staticmethod
  def fromkeys(iterable):
    out = OrderedSet()
    out.update(iterable)
    return out

  def add(self, key):
    self[key] = True

  def remove(self, key):
    if key in self:
      del self[key]

  def update(self, iterable):
    for v in iterable:
      self.add(v)

  def difference_update(self, iterable):
    for v in iterable:
      self.remove(v)


class AndroidManifest:
  def __init__(self, path):
    self.path = path
    dom = xml.dom.minidom.parse(path)
    manifests = dom.getElementsByTagName('manifest')
    assert len(manifests) == 1
    self.manifest = manifests[0]

  def GetInstrumentationElements(self):
    instrumentation_els = self.manifest.getElementsByTagName('instrumentation')
    if len(instrumentation_els) == 0:
      return None
    return instrumentation_els

  def CheckInstrumentationElements(self, expected_package):
    instrs = self.GetInstrumentationElements()
    if not instrs:
      raise Exception('No <instrumentation> elements found in %s' % self.path)
    for instr in instrs:
      instrumented_package = instr.getAttributeNS(
          'http://schemas.android.com/apk/res/android', 'targetPackage')
      if instrumented_package != expected_package:
        raise Exception(
            'Wrong instrumented package. Expected %s, got %s'
            % (expected_package, instrumented_package))

  def GetPackageName(self):
    return self.manifest.getAttribute('package')


def ReadJson(path):
  with open(path) as f:
    return json.load(f)


def GetDepConfig(path):
  if ret := _dep_config_cache.get(path):
    return ret
  config = ReadJson(path.replace('.build_config.json', '.params.json'))
  config.update(ReadJson(path))
  config['path'] = path
  _dep_config_cache[path] = config
  return config


def DepsOfType(wanted_type, configs):
  return [c for c in configs if c['type'] == wanted_type]


def DepPathsOfType(wanted_type, config_paths):
  return [p for p in config_paths if GetDepConfig(p)['type'] == wanted_type]


def GetAllDepsConfigsInOrder(deps_config_paths, filter_func=None):
  def apply_filter(paths):
    if filter_func:
      return [p for p in paths if filter_func(GetDepConfig(p))]
    return paths

  def discover(path):
    config = GetDepConfig(path)
    all_deps = (config.get('deps_configs', []) +
                config.get('public_deps_configs', []))
    return apply_filter(all_deps)

  deps_config_paths = apply_filter(deps_config_paths)
  deps_config_paths = build_utils.GetSortedTransitiveDependencies(
      deps_config_paths, discover)
  return deps_config_paths


def GetObjectByPath(obj, key_path):
  """Given an object, return its nth child based on a key path.
  """
  return GetObjectByPath(obj[key_path[0]], key_path[1:]) if key_path else obj


def RemoveObjDups(obj, base, *key_path):
  """Remove array items from an object[*kep_path] that are also
     contained in the base[*kep_path] (duplicates).
  """
  base_target = set(GetObjectByPath(base, key_path))
  target = GetObjectByPath(obj, key_path)
  target[:] = [x for x in target if x not in base_target]


class Deps:
  def __init__(self, direct_deps_config_paths):
    self._all_deps_config_paths = GetAllDepsConfigsInOrder(
        direct_deps_config_paths)
    self._direct_deps_configs = [
        GetDepConfig(p) for p in direct_deps_config_paths
    ]
    self._all_deps_configs = [
        GetDepConfig(p) for p in self._all_deps_config_paths
    ]
    self._direct_deps_config_paths = direct_deps_config_paths

  def All(self, wanted_type=None):
    if wanted_type is None:
      return self._all_deps_configs
    return DepsOfType(wanted_type, self._all_deps_configs)

  def Direct(self, wanted_type=None):
    if wanted_type is None:
      return self._direct_deps_configs
    return DepsOfType(wanted_type, self._direct_deps_configs)

  def AllConfigPaths(self):
    return self._all_deps_config_paths

  def GradlePrebuiltJarPaths(self):
    ret = []

    def helper(cur):
      for config in cur.Direct('java_library'):
        if config['is_prebuilt'] or config.get('gradle_treat_as_prebuilt'):
          if config['unprocessed_jar_path'] not in ret:
            ret.append(config['unprocessed_jar_path'])

    helper(self)
    return ret

  def GradleLibraryProjectDeps(self):
    ret = []

    def helper(cur):
      for config in cur.Direct('java_library'):
        if config['is_prebuilt']:
          pass
        elif config.get('gradle_treat_as_prebuilt'):
          all_deps = config.get('deps_configs', []) + config.get(
              'public_deps_configs', [])
          helper(Deps(all_deps))
        elif config not in ret:
          ret.append(config)

    helper(self)
    return ret


def _MergeAssets(all_assets):
  """Merges all assets from the given deps.

  Returns:
    A tuple of: (compressed, uncompressed, locale_paks)
    |compressed| and |uncompressed| are lists of "srcPath:zipPath". srcPath is
    the path of the asset to add, and zipPath is the location within the zip
    (excluding assets/ prefix).
    |locale_paks| is a set of all zipPaths that have been marked as
    treat_as_locale_paks=true.
  """
  compressed = {}
  uncompressed = {}
  locale_paks = set()
  for asset_dep in all_assets:
    entry = asset_dep['assets']
    disable_compression = entry.get('disable_compression')
    treat_as_locale_paks = entry.get('treat_as_locale_paks')
    dest_map = uncompressed if disable_compression else compressed
    other_map = compressed if disable_compression else uncompressed
    outputs = entry.get('outputs', [])
    for src, dest in itertools.zip_longest(entry['sources'], outputs):
      if not dest:
        dest = os.path.basename(src)
      # Merge so that each path shows up in only one of the lists, and that
      # deps of the same target override previous ones.
      other_map.pop(dest, 0)
      dest_map[dest] = src
      if treat_as_locale_paks:
        locale_paks.add(dest)

  def create_list(asset_map):
    # Sort to ensure deterministic ordering.
    items = sorted(asset_map.items())
    return [f'{src}:{dest}' for dest, src in items]

  return create_list(compressed), create_list(uncompressed), locale_paks


def _SuffixAssets(suffix_names, suffix, assets):
  new_assets = []
  for x in assets:
    src_path, apk_subpath = x.split(':', 1)
    if apk_subpath in suffix_names:
      apk_subpath += suffix
    new_assets.append(f'{src_path}:{apk_subpath}')
  return new_assets


def _ResolveGroupsAndPublicDeps(config_paths):
  """Returns a list of configs with all groups inlined."""

  def helper(config_path):
    config = GetDepConfig(config_path)
    if config['type'] == 'group':
      # Groups combine public_deps with deps_configs, so no need to check
      # public_config_paths separately.
      return config.get('deps_configs', [])
    if config['type'] == 'android_resources':
      # android_resources targets do not support public_deps, but instead treat
      # all resource deps as public deps.
      return DepPathsOfType('android_resources', config.get('deps_configs', []))

    return config.get('public_deps_configs', [])

  return build_utils.GetSortedTransitiveDependencies(config_paths, helper)


def _DepsFromPaths(dep_paths,
                   target_type,
                   filter_root_targets=True,
                   recursive_resource_deps=False):
  """Resolves all groups and trims dependency branches that we never want.

  E.g. When a resource or asset depends on an apk target, the intent is to
  include the .apk as a resource/asset, not to have the apk's classpath added.

  This method is meant to be called to get the top nodes (i.e. closest to
  current target) that we could then use to get a full transitive dependants
  list (eg using Deps#all). So filtering single elements out of this list,
  filters whole branches of dependencies. By resolving groups (i.e. expanding
  them to their constituents), depending on a group is equivalent to directly
  depending on each element of that group.
  """
  blocklist = []
  allowlist = []

  # Don't allow root targets to be considered as a dep.
  if filter_root_targets:
    blocklist.extend(_ROOT_TYPES)

  # Don't allow java libraries to cross through assets/resources.
  if target_type in _RESOURCE_TYPES:
    allowlist.extend(_RESOURCE_TYPES)
    # Pretend that this target directly depends on all of its transitive
    # dependencies.
    if recursive_resource_deps:
      dep_paths = GetAllDepsConfigsInOrder(dep_paths)
      # Exclude assets if recursive_resource_deps is set. The
      # recursive_resource_deps arg is used to pull resources into the base
      # module to workaround bugs accessing resources in isolated DFMs, but
      # assets should be kept in the DFMs.
      blocklist.append('android_assets')

  return _DepsFromPathsWithFilters(dep_paths, blocklist, allowlist)


def _FilterConfigPaths(dep_paths, blocklist=None, allowlist=None):
  if not blocklist and not allowlist:
    return dep_paths
  configs = [GetDepConfig(p) for p in dep_paths]
  if blocklist:
    configs = [c for c in configs if c['type'] not in blocklist]
  if allowlist:
    configs = [c for c in configs if c['type'] in allowlist]

  return [c['path'] for c in configs]


def _DepsFromPathsWithFilters(dep_paths, blocklist=None, allowlist=None):
  """Resolves all groups and trims dependency branches that we never want.

  See _DepsFromPaths.

  |blocklist| if passed, are the types of direct dependencies we do not care
  about (i.e. tips of branches that we wish to prune).

  |allowlist| if passed, are the only types of direct dependencies we care
  about (i.e. we wish to prune all other branches that do not start from one of
  these).
  """
  # Filter both before and after so that public_deps of blocked targets are not
  # added.
  allowlist_with_groups = None
  if allowlist:
    allowlist_with_groups = set(allowlist)
    allowlist_with_groups.add('group')
  dep_paths = _FilterConfigPaths(dep_paths, blocklist, allowlist_with_groups)
  dep_paths = _ResolveGroupsAndPublicDeps(dep_paths)
  dep_paths = _FilterConfigPaths(dep_paths, blocklist, allowlist)

  return Deps(dep_paths)


def _ExtractSharedLibsFromRuntimeDeps(runtime_deps_file):
  ret = []
  with open(runtime_deps_file) as f:
    for line in f:
      line = line.rstrip()
      if not line.endswith('.so'):
        continue
      # Only unstripped .so files are listed in runtime deps.
      # Convert to the stripped .so by going up one directory.
      ret.append(os.path.normpath(line.replace('lib.unstripped/', '')))
  ret.reverse()
  return ret


def _CreateJavaLibrariesList(library_paths):
  """Returns a java literal array with the "base" library names:
  e.g. libfoo.so -> foo
  """
  names = ['"%s"' % os.path.basename(s)[3:-3] for s in library_paths]
  return ('{%s}' % ','.join(sorted(set(names))))


def _CreateJavaLocaleListFromAssets(assets, locale_paks):
  """Returns a java literal array from a list of locale assets.

  Args:
    assets: A list of all APK asset paths in the form 'src:dst'
    locale_paks: A list of asset paths that correponds to the locale pak
      files of interest. Each |assets| entry will have its 'dst' part matched
      against it to determine if they are part of the result.
  Returns:
    A string that is a Java source literal array listing the locale names
    of the corresponding asset files, without directory or .pak suffix.
    E.g. '{"en-GB", "en-US", "es-ES", "fr", ... }'
  """
  assets_paths = [a.split(':')[1] for a in assets]
  locales = [os.path.basename(a)[:-4] for a in assets_paths if a in locale_paks]
  return '{%s}' % ','.join('"%s"' % l for l in sorted(locales))


def _AddJarMapping(jar_to_target, config):
  if jar := config.get('unprocessed_jar_path'):
    jar_to_target[jar] = config['gn_target']
  for jar in config.get('input_jars_paths', []):
    jar_to_target[jar] = config['gn_target']


def _CompareClasspathPriority(dep):
  return 1 if dep.get('low_classpath_priority') else 0


def _DedupFeatureModuleSharedCode(child_to_ancestors, modules,
                                  field_names_to_dedup):
  # Strip out duplicates from ancestors.
  for name, module in modules.items():
    if name == 'base':
      continue
    # Make sure we get all ancestors, not just direct parent.
    for ancestor in child_to_ancestors[name]:
      for f in field_names_to_dedup:
        if f in module:
          RemoveObjDups(module, modules[ancestor], f)

  # Strip out duplicates from siblings/cousins.
  for f in field_names_to_dedup:
    _PromoteToCommonAncestor(modules, child_to_ancestors, f)


def _PromoteToCommonAncestor(modules, child_to_ancestors, field_name):
  module_to_fields_set = {}
  for module_name, module in modules.items():
    if field_name in module:
      module_to_fields_set[module_name] = set(module[field_name])

  seen = set()
  dupes = set()
  for fields in module_to_fields_set.values():
    new_dupes = seen & fields
    if new_dupes:
      dupes |= new_dupes
    seen |= fields

  for d in dupes:
    owning_modules = []
    for module_name, fields in module_to_fields_set.items():
      if d in fields:
        owning_modules.append(module_name)
    assert len(owning_modules) >= 2
    # Rely on the fact that ancestors are inserted from closest to
    # farthest, where "base" should always be the last element.
    # Arbitrarily using the first owning module - any would work.
    for ancestor in child_to_ancestors[owning_modules[0]]:
      ancestor_is_shared_with_all = True
      for o in owning_modules[1:]:
        if ancestor not in child_to_ancestors[o]:
          ancestor_is_shared_with_all = False
          break
      if ancestor_is_shared_with_all:
        common_ancestor = ancestor
        break
    for o in owning_modules:
      module_to_fields_set[o].remove(d)
    module_to_fields_set[common_ancestor].add(d)

  for module_name, module in modules.items():
    if field_name in module:
      module[field_name] = sorted(list(module_to_fields_set[module_name]))


def _CopyBuildConfigsForDebugging(debug_dir):
  shutil.rmtree(debug_dir, ignore_errors=True)
  os.makedirs(debug_dir)
  for src_path in _dep_config_cache:
    dst_path = os.path.join(debug_dir, src_path)
    assert dst_path.startswith(debug_dir), dst_path
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    shutil.copy(src_path, dst_path)
  print(f'Copied {len(_dep_config_cache)} .build_config.json into {debug_dir}')


def _ListFromDeps(deps, key_name):
  return [config[key_name] for config in deps if key_name in config]


def _SetFromDeps(deps, key_name):
  combined = set()
  for config in deps:
    if value := config.get(key_name):
      if isinstance(value, str):
        combined.add(value)
      else:
        combined.update(value)
  return combined


def main():
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--params', help='Path to .params.json file.')
  parser.add_argument('--store-deps-for-debugging-to',
                      help='Path to copy all transitive build config files to.')
  options = parser.parse_args()

  params = ReadJson(options.params)
  output_path = options.params.replace('.params.json', '.build_config.json')

  if lines := params.get('fail'):
    parser.error('\n'.join(lines))

  target_type = params['type']

  is_bundle_module = target_type == 'android_app_bundle_module'
  is_apk = target_type == 'android_apk'
  is_apk_or_module = is_apk or is_bundle_module
  is_bundle = target_type == 'android_app_bundle'

  is_java_target = target_type in ('java_binary', 'robolectric_binary',
                                   'java_annotation_processor', 'java_library',
                                   'android_apk', 'dist_aar', 'dist_jar',
                                   'system_java_library',
                                   'android_app_bundle_module')

  deps_configs_paths = params.get('deps_configs', [])
  public_deps_configs_paths = params.get('public_deps_configs', [])
  deps_configs_paths += public_deps_configs_paths
  deps = _DepsFromPaths(
      deps_configs_paths,
      target_type,
      recursive_resource_deps=params.get('recursive_resource_deps'))
  public_deps = _DepsFromPaths(public_deps_configs_paths, target_type)
  processor_deps = _DepsFromPaths(params.get('processor_configs', []),
                                  target_type,
                                  filter_root_targets=False)

  all_inputs = deps.AllConfigPaths() + processor_deps.AllConfigPaths()

  if params.get('recursive_resource_deps'):
    # Include java_library targets since changes to these targets can remove
    # resource deps from the build, which would require rebuilding this target's
    # build config file: crbug.com/1168655.
    recursive_java_deps = _DepsFromPathsWithFilters(
        GetAllDepsConfigsInOrder(deps_configs_paths),
        allowlist=['java_library'])
    all_inputs.extend(recursive_java_deps.AllConfigPaths())

  system_library_deps = deps.Direct('system_java_library')
  all_deps = deps.All()
  all_library_deps = deps.All('java_library')

  direct_resources_deps = deps.Direct('android_resources')
  if target_type == 'java_library':
    # For Java libraries, restrict to resource targets that are direct deps, or
    # are indirect via other resource targets.
    # The indirect-through-other-targets ones are picked up because
    # _ResolveGroupsAndPublicDeps() treats resource deps of resource targets as
    # public_deps.
    all_resources_deps = direct_resources_deps
  else:
    all_resources_deps = deps.All('android_resources')

  if target_type == 'android_resources' and params.get(
      'recursive_resource_deps'):
    # android_resources targets that want recursive resource deps also need to
    # collect package_names from all library deps. This ensures the R.java files
    # for these libraries will get pulled in along with the resources.
    android_resources_library_deps = _DepsFromPathsWithFilters(
        deps_configs_paths, allowlist=['java_library']).All('java_library')

  base_module_build_config = None
  if path := params.get('base_module_config'):
    base_module_build_config = GetDepConfig(path)
  parent_module_build_config = base_module_build_config
  if path := params.get('parent_module_config'):
    parent_module_build_config = GetDepConfig(path)

  config = collections.defaultdict(dict)

  # The paths we record as deps can differ from deps_config_paths:
  # 1) Paths can be removed when blocked by _ROOT_TYPES / _RESOURCE_TYPES.
  # 2) Paths can be added when promoted from group deps or public_deps of deps.
  #    Deps are promoted from groups/public_deps in order to make the filtering
  #    of 1) work through group() targets (which themselves are not resource
  #    targets, but should be treated as such when depended on by a resource
  #    target. A more involved filtering implementation could work to maintain
  #    the semantics of 1) without the need to promote deps, but we've avoided
  #    such an undertaking so far.
  public_deps_set = set()
  if public_deps_configs_paths:
    resolved_public_deps_configs = [d['path'] for d in public_deps.Direct()]
    if resolved_public_deps_configs != params.get('public_deps_configs', []):
      config['public_deps_configs'] = resolved_public_deps_configs
    public_deps_set.update(resolved_public_deps_configs)

  resolved_deps_configs = [
      d['path'] for d in deps.Direct() if d['path'] not in public_deps_set
  ]
  if resolved_deps_configs != params.get('deps_configs', []):
    config['deps_configs'] = resolved_deps_configs

  apk_under_test_config = None
  if is_apk:
    config['apk_path'] = params['apk_path']
    if path := params.get('incremental_install_json_path'):
      config['incremental_install_json_path'] = path
      config['incremental_apk_path'] = params['incremental_apk_path']
    if path := params.get('apk_under_test_config'):
      apk_under_test_config = GetDepConfig(path)
      config['gradle']['apk_under_test'] = os.path.basename(
          apk_under_test_config['apk_path'])

  if is_java_target:
    dependent_prebuilt_jars = deps.GradlePrebuiltJarPaths()
    dependent_prebuilt_jars.sort()
    if dependent_prebuilt_jars:
      config['gradle']['dependent_prebuilt_jars'] = dependent_prebuilt_jars

    dependent_android_projects = []
    dependent_java_projects = []
    for c in deps.GradleLibraryProjectDeps():
      if c['requires_android']:
        dependent_android_projects.append(c['path'])
      else:
        dependent_java_projects.append(c['path'])

    config['gradle']['dependent_android_projects'] = dependent_android_projects
    config['gradle']['dependent_java_projects'] = dependent_java_projects

  if is_java_target:
    # robolectric is special in that its an android target that runs on host.
    # You are allowed to depend on both android |deps_require_android| and
    # non-android |deps_not_support_android| targets.
    if (not params.get('bypass_platform_checks')
        and not params.get('requires_robolectric')):
      deps_require_android = direct_resources_deps + [
          d for d in deps.Direct() if d.get('requires_android', False)
      ]
      deps_not_support_android = [
          d for d in deps.Direct() if not d.get('supports_android', True)
      ]

      if deps_require_android and not params.get('requires_android'):
        raise Exception(
            'Some deps require building for the Android platform:\n' +
            '\n'.join('* ' + d['gn_target'] for d in deps_require_android))

      if deps_not_support_android and params.get('supports_android'):
        raise Exception('Not all deps support the Android platform:\n' +
                        '\n'.join('* ' + d['gn_target']
                                  for d in deps_not_support_android))

  if is_apk_or_module or target_type == 'dist_jar':
    all_dex_files = [c['dex_path'] for c in all_library_deps]

  # Classpath values filled in below (after applying apk_under_test_config).
  if is_apk_or_module:
    all_dex_files.append(params['dex_path'])

  if is_bundle_module:
    config['unprocessed_jar_path'] = params['unprocessed_jar_path']
    config['res_size_info_path'] = params['res_size_info_path']

  if target_type == 'android_resources':
    if not params.get('package_name'):
      if path := params.get('android_manifest'):
        manifest = AndroidManifest(path)
        config['package_name'] = manifest.GetPackageName()

  if target_type in ('android_resources', 'android_apk', 'robolectric_binary',
                     'dist_aar', 'android_app_bundle_module', 'java_library'):
    dependency_zips = []
    dependency_zip_overlays = []
    for c in all_resources_deps:
      if not c.get('resources_zip'):
        continue

      dependency_zips.append(c['resources_zip'])
      if c.get('resource_overlay'):
        dependency_zip_overlays.append(c['resources_zip'])

    extra_package_names = []

    if target_type != 'android_resources':
      extra_package_names = _ListFromDeps(all_resources_deps, 'package_name')
      if name := params.get('package_name'):
        extra_package_names.append(name)

      # android_resources targets which specified recursive_resource_deps may
      # have extra_package_names.
      for resources_dep in all_resources_deps:
        extra_package_names.extend(resources_dep['extra_package_names'])

      # In final types (i.e. apks and modules) that create real R.java files,
      # they must collect package names from java_libraries as well.
      # https://crbug.com/1073476
      if target_type != 'java_library':
        extra_package_names += _ListFromDeps(all_library_deps, 'package_name')

    elif params.get('recursive_resource_deps'):
      # Pull extra_package_names from library deps if recursive resource deps
      # are required.
      extra_package_names = _ListFromDeps(android_resources_library_deps,
                                          'package_name')

    if target_type in ('dist_aar', 'java_library'):
      paths = _ListFromDeps(all_resources_deps, 'rtxt_path')
      config['dependency_rtxt_files'] = paths

    if is_apk and apk_under_test_config:
      config['arsc_package_name'] = apk_under_test_config['package_name']
      # We should not shadow the actual R.java files of the apk_under_test by
      # creating new R.java files with the same package names in the tested apk.
      extra_package_names = [
          package for package in extra_package_names
          if package not in apk_under_test_config['extra_package_names']
      ]

    # Safe to sort: Build checks that non-overlay resource have no overlap.
    dependency_zips.sort()
    config['dependency_zips'] = dependency_zips
    config['dependency_zip_overlays'] = dependency_zip_overlays
    # Order doesn't matter, so make stable.
    extra_package_names.sort()
    config['extra_package_names'] = extra_package_names

  mergeable_android_manifests = params.get('mergeable_android_manifests', [])
  mergeable_android_manifests.sort()
  if mergeable_android_manifests:
    config['mergeable_android_manifests'] = mergeable_android_manifests

  extra_proguard_classpath_jars = []
  proguard_configs = params.get('proguard_configs', [])

  if is_java_target:
    classpath_direct_deps = deps.Direct()
    classpath_direct_library_deps = deps.Direct('java_library')

    # The classpath used to compile this target when annotation processors are
    # present.
    javac_classpath = _SetFromDeps(classpath_direct_library_deps,
                                   'unprocessed_jar_path')
    # The classpath used to compile this target when annotation processors are
    # not present. These are also always used to know when a target needs to be
    # rebuilt.
    javac_interface_classpath = _SetFromDeps(classpath_direct_library_deps,
                                             'interface_jar_path')

    # Preserve order of |all_library_deps|. Move low priority libraries to the
    # end of the classpath.
    all_library_deps_sorted_for_classpath = sorted(
        all_library_deps[::-1], key=_CompareClasspathPriority)

    # The classpath used for bytecode-rewritting.
    javac_full_classpath = OrderedSet.fromkeys(
        c['unprocessed_jar_path']
        for c in all_library_deps_sorted_for_classpath)
    # The classpath used for error prone.
    javac_full_interface_classpath = OrderedSet.fromkeys(
        c['interface_jar_path'] for c in all_library_deps_sorted_for_classpath)

    # Adding base module to classpath to compile against its R.java file
    if base_module_build_config:
      javac_full_classpath.add(base_module_build_config['unprocessed_jar_path'])
      javac_full_interface_classpath.add(
          base_module_build_config['interface_jar_path'])
      # Turbine now compiles headers against only the direct classpath, so the
      # base module's interface jar must be on the direct interface classpath.
      javac_interface_classpath.add(
          base_module_build_config['interface_jar_path'])

    for dep in classpath_direct_deps:
      if paths := dep.get('input_jars_paths'):
        javac_classpath.update(paths)
        javac_interface_classpath.update(paths)
    for dep in all_deps:
      if paths := dep.get('input_jars_paths'):
        javac_full_classpath.update(paths)
        javac_full_interface_classpath.update(paths)

    # TODO(agrieve): Might be less confusing to fold these into bootclasspath.
    # Deps to add to the compile-time classpath (but not the runtime classpath).
    # These are jars specified by input_jars_paths that almost never change.
    # Just add them directly to all the classpaths.
    if paths := params.get('input_jars_paths'):
      javac_classpath.update(paths)
      javac_interface_classpath.update(paths)
      javac_full_classpath.update(paths)
      javac_full_interface_classpath.update(paths)

  if is_java_target or is_bundle:
    # The classpath to use to run this target (or as an input to ProGuard).
    device_classpath = []
    # dist_jar configs should not list their device jar in their own classpath
    # since the classpath is used to create the device jar itself.
    if is_java_target and target_type != 'dist_jar':
      if path := params.get('processed_jar_path'):
        device_classpath.append(path)
    device_classpath += _ListFromDeps(all_library_deps, 'processed_jar_path')
    if is_bundle:
      for d in deps.Direct('android_app_bundle_module'):
        device_classpath.extend(c for c in d.get('device_classpath', [])
                                if c not in device_classpath)

  all_dist_jar_deps = deps.All('dist_jar')

  # We allow lint to be run on android_apk targets, so we collect lint
  # artifacts for them.
  # We allow lint to be run on android_app_bundle targets, so we need to
  # collect lint artifacts for the android_app_bundle_module targets that the
  # bundle includes. Different android_app_bundle targets may include different
  # android_app_bundle_module targets, so the bundle needs to be able to
  # de-duplicate these lint artifacts.
  if is_apk_or_module:
    # Collect all sources and resources at the apk/bundle_module level.
    lint_aars = set()
    lint_srcjars = set()
    lint_sources = set()
    lint_resource_sources = set()
    lint_resource_zips = set()

    if path := params.get('target_sources_file'):
      lint_sources.add(path)
    if paths := params.get('bundled_srcjars'):
      lint_srcjars.update(paths)
    for c in all_library_deps:
      if c.get('chromium_code', True) and c['requires_android']:
        if path := c.get('target_sources_file'):
          lint_sources.add(path)
        lint_srcjars.update(c['bundled_srcjars'])
      if path := c.get('aar_path'):
        lint_aars.add(path)

    if path := params.get('res_sources_path'):
      lint_resource_sources.add(path)
    if path := params.get('resources_zip'):
      lint_resource_zips.add(path)
    for c in all_resources_deps:
      if c.get('chromium_code', True):
        # Prefer res_sources_path to resources_zips so that lint errors have
        # real paths and to avoid needing to extract during lint.
        if path := c.get('res_sources_path'):
          lint_resource_sources.add(path)
        else:
          lint_resource_zips.add(c['resources_zip'])

    config['lint_aars'] = sorted(lint_aars)
    config['lint_srcjars'] = sorted(lint_srcjars)
    config['lint_sources'] = sorted(lint_sources)
    config['lint_resource_sources'] = sorted(lint_resource_sources)
    config['lint_resource_zips'] = sorted(lint_resource_zips)
    config['lint_extra_android_manifests'] = []

  if is_bundle:
    modules_by_name = {m['name']: m for m in params['modules']}
    module_configs_by_name = {
        m['name']: GetDepConfig(m['build_config'])
        for m in params['modules']
    }
    modules = {name: {} for name in modules_by_name}

    child_to_ancestors = {n: ['base'] for n in modules_by_name}
    for name, module in modules_by_name.items():
      while parent := module.get('uses_split'):
        child_to_ancestors[name].append(parent)
        module = modules_by_name[parent]

    per_module_fields = [
        'device_classpath', 'trace_event_rewritten_device_classpath',
        'all_dex_files', 'assets', 'uncompressed_assets'
    ]
    lint_aars = set()
    lint_srcjars = set()
    lint_sources = set()
    lint_resource_sources = set()
    lint_resource_zips = set()
    lint_extra_android_manifests = set()
    for n, c in module_configs_by_name.items():
      if n == 'base':
        assert 'base_module_config' not in config, (
            'Must have exactly 1 base module!')
        config['package_name'] = c['package_name']
        config['version_code'] = c['version_code']
        config['version_name'] = c['version_name']
        config['base_module_config'] = c['path']
        config['android_manifest'] = c['android_manifest']
      else:
        # All manifests nodes are merged into the main manfiest by lint.py.
        lint_extra_android_manifests.add(c['android_manifest'])

      lint_extra_android_manifests.update(c['extra_android_manifests'])
      lint_aars.update(c['lint_aars'])
      lint_srcjars.update(c['lint_srcjars'])
      lint_sources.update(c['lint_sources'])
      lint_resource_sources.update(c['lint_resource_sources'])
      lint_resource_zips.update(c['lint_resource_zips'])
      module = modules[n]
      module['final_dex_path'] = c['final_dex_path']
      for f in per_module_fields:
        if f in c:
          module[f] = c[f]
    config['lint_aars'] = sorted(lint_aars)
    config['lint_srcjars'] = sorted(lint_srcjars)
    config['lint_sources'] = sorted(lint_sources)
    config['lint_resource_sources'] = sorted(lint_resource_sources)
    config['lint_resource_zips'] = sorted(lint_resource_zips)
    config['lint_extra_android_manifests'] = sorted(
        lint_extra_android_manifests)

    _DedupFeatureModuleSharedCode(child_to_ancestors, modules,
                                  per_module_fields)
    config['modules'] = modules

  if is_java_target or (is_bundle and params.get('proguard_enabled')):
    config['sdk_jars'] = [
        c['unprocessed_jar_path'] for c in system_library_deps
    ]
    config['sdk_interface_jars'] = [
        c['interface_jar_path'] for c in system_library_deps
    ]

  if target_type in ('android_apk', 'dist_aar', 'dist_jar',
                     'android_app_bundle_module', 'android_app_bundle'):
    for c in all_deps:
      proguard_configs.extend(c.get('proguard_configs', []))
      extra_proguard_classpath_jars.extend(c.get('input_jars_paths', []))
    if is_bundle:
      for c in deps.Direct('android_app_bundle_module'):
        proguard_configs.extend(c.get('proguard_configs', []))
        extra_proguard_classpath_jars.extend(
            j for j in c.get('proguard_classpath_jars', [])
            if j not in extra_proguard_classpath_jars)

      deps_proguard_enabled = []
      deps_proguard_disabled = []
      for d in deps.Direct('android_app_bundle_module'):
        if not d['device_classpath']:
          # We don't care about modules that have no Java code for proguarding.
          continue
        if d.get('proguard_enabled'):
          deps_proguard_enabled.append(d['module_name'])
        else:
          deps_proguard_disabled.append(d['module_name'])
      if deps_proguard_enabled and deps_proguard_disabled:
        raise Exception('Deps %s have proguard enabled while deps %s have '
                        'proguard disabled' % (deps_proguard_enabled,
                                               deps_proguard_disabled))

  # The java code for an instrumentation test apk is assembled differently for
  # R8 vs. non-R8.
  #
  # Without R8: Each library's jar is dexed separately and then combined
  # into a single classes.dex. A test apk will include all dex files not already
  # present in the apk-under-test. At runtime all test code lives in the test
  # apk, and the program code lives in the apk-under-test.
  #
  # With R8: Each library's .jar file is fed into R8, which outputs
  # a single .jar, which is then dexed into a classes.dex. A test apk includes
  # all jar files from the program and the tests because having them separate
  # doesn't work with R8's whole-program optimizations. Although the
  # apk-under-test still has all of its code in its classes.dex, none of it is
  # used at runtime because the copy of it within the test apk takes precidence.

  if is_apk and apk_under_test_config:
    if apk_under_test_config['proguard_enabled']:
      assert params.get('proguard_enabled'), (
          'proguard must be enabled for '
          'instrumentation apks if it\'s enabled for the tested apk.')
      # Mutating lists, so no need to explicitly re-assign to dict.
      proguard_configs.extend(apk_under_test_config['proguard_all_configs'])
      extra_proguard_classpath_jars.extend(
          apk_under_test_config['proguard_classpath_jars'])

    # Add all tested classes to the test's classpath to ensure that the test's
    # java code is a superset of the tested apk's java code
    device_classpath_extended = list(device_classpath)
    device_classpath_extended.extend(
        p for p in apk_under_test_config['device_classpath']
        if p not in device_classpath)
    # Include in the classpath classes that are added directly to the apk under
    # test (those that are not a part of a java_library).
    javac_classpath.add(apk_under_test_config['unprocessed_jar_path'])
    javac_interface_classpath.add(apk_under_test_config['interface_jar_path'])
    javac_full_classpath.add(apk_under_test_config['unprocessed_jar_path'])
    javac_full_interface_classpath.add(
        apk_under_test_config['interface_jar_path'])
    javac_full_classpath.update(apk_under_test_config['javac_full_classpath'])
    javac_full_interface_classpath.update(
        apk_under_test_config['javac_full_interface_classpath'])

    # Exclude .jar files from the test apk that exist within the apk under test.
    apk_under_test_deps = Deps([apk_under_test_config['path']])
    apk_under_test_library_deps = apk_under_test_deps.All('java_library')
    apk_under_test_dex_files = {
        c['dex_path']
        for c in apk_under_test_library_deps
    }
    all_dex_files = [
        p for p in all_dex_files if p not in apk_under_test_dex_files
    ]
    apk_under_test_jar_files = set(apk_under_test_config['device_classpath'])
    device_classpath = [
        p for p in device_classpath if p not in apk_under_test_jar_files
    ]

  if target_type in ('android_apk', 'dist_aar', 'dist_jar',
                     'android_app_bundle_module', 'android_app_bundle'):
    config['proguard_all_configs'] = sorted(set(proguard_configs))
    config['proguard_classpath_jars'] = sorted(
        set(extra_proguard_classpath_jars))

  if target_type in ('dist_jar', 'java_binary', 'robolectric_binary'):
    # The classpath to use to run this target.
    host_classpath = []
    if path := params.get('processed_jar_path'):
      host_classpath.append(path)
    host_classpath.extend(c['processed_jar_path'] for c in all_library_deps)
    # Collect all the dist_jar host jars.
    dist_jar_host_jars = _ListFromDeps(all_dist_jar_deps, 'processed_jar_path')
    # Collect all the jars that went into the dist_jar host jars.
    dist_jar_host_classpath = _SetFromDeps(all_dist_jar_deps, 'host_classpath')
    # Remove the jars that went into the dist_jar host jars.
    host_classpath = [
        p for p in host_classpath if p not in dist_jar_host_classpath
    ]
    # Add the dist_jar host jars themselves instead.
    host_classpath += dist_jar_host_jars
    config['host_classpath'] = host_classpath

  if is_java_target:
    dist_jar_device_classpath = _SetFromDeps(all_dist_jar_deps,
                                             'device_classpath')
    dist_jar_javac_full_classpath = _SetFromDeps(all_dist_jar_deps,
                                                 'javac_full_classpath')
    dist_jar_javac_full_interface_classpath = _SetFromDeps(
        all_dist_jar_deps, 'javac_full_interface_classpath')
    dist_jar_child_dex_files = _SetFromDeps(all_dist_jar_deps, 'all_dex_files')

    dist_jar_device_jars = _ListFromDeps(all_dist_jar_deps,
                                         'processed_jar_path')
    dist_jar_combined_dex_files = _ListFromDeps(all_dist_jar_deps, 'dex_path')
    dist_jar_interface_jars = _ListFromDeps(all_dist_jar_deps,
                                            'interface_jar_path')
    dist_jar_unprocessed_jars = _ListFromDeps(all_dist_jar_deps,
                                              'unprocessed_jar_path')

    device_classpath = [
        p for p in device_classpath if p not in dist_jar_device_classpath
    ]
    device_classpath += dist_jar_device_jars

    javac_full_classpath.difference_update(dist_jar_javac_full_classpath)
    javac_full_classpath.update(dist_jar_unprocessed_jars)

    javac_full_interface_classpath.difference_update(
        dist_jar_javac_full_interface_classpath)
    javac_full_interface_classpath.update(dist_jar_interface_jars)

    javac_interface_classpath.update(dist_jar_interface_jars)
    javac_classpath.update(dist_jar_unprocessed_jars)

    if is_apk_or_module or target_type == 'dist_jar':
      all_dex_files = [
          p for p in all_dex_files if p not in dist_jar_child_dex_files
      ]
      all_dex_files += dist_jar_combined_dex_files

  if is_apk_or_module or target_type == 'dist_jar':
    # Dependencies for the final dex file of an apk.
    config['all_dex_files'] = all_dex_files

  if is_java_target:
    config['classpath'] = sorted(javac_classpath)
    config['interface_classpath'] = sorted(javac_interface_classpath)
    # Direct() will be of type 'java_annotation_processor', and so not included
    # in All('java_library').
    # Annotation processors run as part of the build, so need processed_jar_path.
    config['processor_classpath'] = _ListFromDeps(
        processor_deps.Direct() + processor_deps.All('java_library'),
        'processed_jar_path')
    config['processor_classes'] = sorted(c['main_class']
                                         for c in processor_deps.Direct())
    config['javac_full_classpath'] = list(javac_full_classpath)
    config['javac_full_interface_classpath'] = list(
        javac_full_interface_classpath)
  elif is_bundle:
    # bundles require javac_full_classpath to create .aab.jar.info and require
    # javac_full_interface_classpath for lint.
    javac_full_classpath = OrderedSet()
    javac_full_interface_classpath = OrderedSet()
    for d in deps.Direct('android_app_bundle_module'):
      javac_full_classpath.update(d['javac_full_classpath'])
      javac_full_interface_classpath.update(d['javac_full_interface_classpath'])
      javac_full_classpath.add(d['unprocessed_jar_path'])
      javac_full_interface_classpath.add(d['interface_jar_path'])
    config['javac_full_classpath'] = list(javac_full_classpath)
    config['javac_full_interface_classpath'] = list(
        javac_full_interface_classpath)

  if target_type in ('android_apk', 'android_app_bundle',
                     'android_app_bundle_module', 'dist_aar', 'dist_jar'):
    config['device_classpath'] = device_classpath
    if trace_events_jar_dir := params.get('trace_events_jar_dir'):
      trace_event_rewritten_device_classpath = []
      for jar_path in device_classpath:
        jar_path = jar_path.replace('../', '')
        jar_path = jar_path.replace('obj/', '')
        jar_path = jar_path.replace('gen/', '')
        jar_path = jar_path.replace('.jar', '.tracing_rewritten.jar')
        rewritten_jar_path = os.path.join(trace_events_jar_dir, jar_path)
        trace_event_rewritten_device_classpath.append(rewritten_jar_path)

      config['trace_event_rewritten_device_classpath'] = (
          trace_event_rewritten_device_classpath)

    if apk_under_test_config:
      config['device_classpath_extended'] = device_classpath_extended

  if target_type == 'dist_jar':
    if params.get('direct_deps_only'):
      if params.get('use_interface_jars'):
        dist_jars = config['interface_classpath']
      else:
        dist_jars = sorted(c['processed_jar_path']
                           for c in classpath_direct_library_deps)
    else:
      if params.get('use_interface_jars'):
        dist_jars = [c['interface_jar_path'] for c in all_library_deps]
      else:
        dist_jars = config['device_classpath']

    config['dist_jar']['jars'] = dist_jars

  if is_apk_or_module:
    manifest = AndroidManifest(params['android_manifest'])
    if not apk_under_test_config and manifest.GetInstrumentationElements():
      # This must then have instrumentation only for itself.
      manifest.CheckInstrumentationElements(manifest.GetPackageName())

    config['package_name'] = manifest.GetPackageName()
    config['android_manifest'] = params['android_manifest']
    config['merged_android_manifest'] = params['merged_android_manifest']

    if is_apk:
      config['version_code'] = params['version_code']
      config['version_name'] = params['version_name']

    # TrichromeLibrary has no dex.
    if final_dex_path := params.get('final_dex_path'):
      config['final_dex_path'] = final_dex_path

    library_paths = []
    java_libraries_list = None
    if path := params.get('shared_libraries_runtime_deps_file'):
      # GN does not add this input to avoid depending on the generated_file() target.
      all_inputs.append(path)
      library_paths = _ExtractSharedLibsFromRuntimeDeps(path)
      java_libraries_list = _CreateJavaLibrariesList(library_paths)

    secondary_abi_library_paths = []
    if path := params.get('secondary_abi_shared_libraries_runtime_deps_file'):
      all_inputs.append(path)
      secondary_abi_library_paths = _ExtractSharedLibsFromRuntimeDeps(path)
      secondary_abi_library_paths.sort()
      paths_without_parent_dirs = [
          p for p in secondary_abi_library_paths if os.path.sep not in p
      ]
      if paths_without_parent_dirs:
        sys.stderr.write('Found secondary native libraries from primary '
                         'toolchain directory. This is a bug!\n')
        sys.stderr.write('\n'.join(paths_without_parent_dirs))
        sys.stderr.write('\n\nIt may be helpful to run: \n')
        sys.stderr.write('    gn path out/Default //chrome/android:'
                         'monochrome_secondary_abi_lib //base:base\n')
        sys.exit(1)

    config['native']['libraries'] = library_paths
    config['native']['secondary_abi_libraries'] = secondary_abi_library_paths
    config['native']['java_libraries_list'] = java_libraries_list

    if is_bundle_module:
      loadable_modules = params.get('loadable_modules', [])
      loadable_modules.sort()
      secondary_abi_loadable_modules = params.get(
          'secondary_abi_loadable_modules', [])
      secondary_abi_loadable_modules.sort()
      placeholder_paths = params.get('native_lib_placeholders', [])
      placeholder_paths.sort()
      secondary_abi_placeholder_paths = params.get(
          'secondary_native_lib_placeholders', [])
      secondary_abi_placeholder_paths.sort()

      config['native']['loadable_modules'] = loadable_modules
      config['native']['placeholders'] = placeholder_paths
      config['native'][
          'secondary_abi_loadable_modules'] = secondary_abi_loadable_modules
      config['native'][
          'secondary_abi_placeholders'] = secondary_abi_placeholder_paths
      config['native']['library_always_compress'] = params.get(
          'library_always_compress')
      config['proto_resources_path'] = params['proto_resources_path']
      config['base_allowlist_rtxt_path'] = params['base_allowlist_rtxt_path']
      config['rtxt_path'] = params['rtxt_path']
      config['module_pathmap_path'] = params['module_pathmap_path']

    # Collect java resources
    java_resources_jars = _ListFromDeps(all_library_deps,
                                        'java_resources_jar_path')
    if apk_under_test_config:
      apk_under_test_resource_jars = _SetFromDeps(apk_under_test_library_deps,
                                                  'java_resources_jar_path')
      java_resources_jars = [
          jar for jar in java_resources_jars
          if jar not in apk_under_test_resource_jars
      ]
    java_resources_jars.sort()
    config['java_resources_jars'] = java_resources_jars

  if is_apk_or_module or target_type == 'robolectric_binary':
    # android_resources deps which had recursive_resource_deps set should not
    # have the manifests from the recursively collected deps added to this
    # module. This keeps the manifest declarations in the child DFMs, since they
    # will have the Java implementations.
    def ExcludeRecursiveResourcesDeps(config):
      return not config.get('recursive_resource_deps', False)

    extra_manifest_deps = [
        GetDepConfig(p) for p in GetAllDepsConfigsInOrder(
            deps_configs_paths, filter_func=ExcludeRecursiveResourcesDeps)
    ]
    # Manifests are listed from highest priority to lowest priority.
    # Ensure directly manfifests come first, and then sort the rest by name.
    # https://developer.android.com/build/manage-manifests#merge_priorities
    config['extra_android_manifests'] = list(mergeable_android_manifests)
    manifests_from_deps = []
    for c in extra_manifest_deps:
      manifests_from_deps += c.get('mergeable_android_manifests', [])
    manifests_from_deps.sort(key=lambda p: (os.path.basename(p), p))
    config['extra_android_manifests'] += manifests_from_deps

    assets, uncompressed_assets, locale_paks = _MergeAssets(
        deps.All('android_assets'))
    config['assets'] = assets
    config['uncompressed_assets'] = uncompressed_assets
    config['locales_java_list'] = _CreateJavaLocaleListFromAssets(
        uncompressed_assets, locale_paks)
    if path := params.get('suffix_apk_assets_used_by_config'):
      if path == output_path:
        target_config = config
      else:
        target_config = GetDepConfig(path)
      all_assets = (target_config['assets'] +
                    target_config['uncompressed_assets'])
      suffix = '+' + target_config['package_name'] + '+'
      suffix_names = {
          x.split(':', 1)[1].replace(suffix, '')
          for x in all_assets
      }
      config['assets'] = _SuffixAssets(suffix_names, suffix, assets)
      config['uncompressed_assets'] = _SuffixAssets(suffix_names, suffix,
                                                    uncompressed_assets)
      config['apk_assets_suffixed_list'] = ','.join(
          f'"assets/{x}"' for x in sorted(suffix_names))
      config['apk_assets_suffix'] = suffix

  # DYNAMIC FEATURE MODULES:
  # There are two approaches to dealing with modules dependencies:
  # 1) Perform steps in android_apk_or_module(), with only the knowledge of
  #    ancesstor splits. Our implementation currently allows only for 2 levels:
  #        base -> parent -> leaf
  #    Bundletool normally fails if two leaf nodes merge the same manifest or
  #    resources. The fix is to add the common dep to the chrome or base module
  #    so that our deduplication logic will work.
  #    RemoveObjDups() implements this approach.
  # 2) Perform steps in android_app_bundle(), with knowledge of full set of
  #    modules. This is required for dex because it can handle the case of two
  #    leaf nodes having the same dep, and promoting that dep to their common
  #    parent.
  #    _DedupFeatureModuleSharedCode() implements this approach.
  if base_module_build_config:
    ancestor_configs = [base_module_build_config]
    if parent_module_build_config is not base_module_build_config:
      ancestor_configs += [parent_module_build_config]
    for c in ancestor_configs:
      RemoveObjDups(config, c, 'dependency_zips')
      RemoveObjDups(config, c, 'dependency_zip_overlays')
      RemoveObjDups(config, c, 'extra_android_manifests')
      RemoveObjDups(config, c, 'extra_package_names')

  if is_java_target:
    jar_to_target = {}
    _AddJarMapping(jar_to_target, params)
    for d in all_deps:
      _AddJarMapping(jar_to_target, d)
    if base_module_build_config:
      for c in ancestor_configs:
        _AddJarMapping(jar_to_target, c)
    if apk_under_test_config:
      _AddJarMapping(jar_to_target, apk_under_test_config)
      jar_to_target.update(
          zip(apk_under_test_config['javac_full_classpath'],
              apk_under_test_config['javac_full_classpath_targets']))

    # Used by check_for_missing_direct_deps.py to give better error message
    # when missing deps are found. Both javac_full_classpath_targets and
    # javac_full_classpath must be in identical orders, as they get passed as
    # separate arrays and then paired up based on index.
    config['javac_full_classpath_targets'] = [
        jar_to_target[x] for x in config['javac_full_classpath']
    ]

  build_utils.WriteJson(config, output_path, only_if_changed=True)

  if options.depfile:
    all_inputs += [
        p.replace('.build_config.json', '.params.json') for p in all_inputs
    ]
    action_helpers.write_depfile(options.depfile, output_path, all_inputs)

  if options.store_deps_for_debugging_to:
    GetDepConfig(output_path)  # Add it to cache.
    _CopyBuildConfigsForDebugging(options.store_deps_for_debugging_to)


if __name__ == '__main__':
  main()
