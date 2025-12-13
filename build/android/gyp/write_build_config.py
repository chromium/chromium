#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes a .build_config.json file.

This script collects information about a target and all of its transitive
dependencies and writes it to a .build_config.json file for use by other build
steps. It also performs a few validations.

See //build/android/docs/build_config.md for more information.
"""


import argparse
import itertools
import os
import sys
import xml.dom.minidom

from util import build_utils
from util import params_json_util
import action_helpers


class OrderedSet(dict):
  """A simple implementation of an ordered set."""

  def __init__(self, iterable=()):
    super().__init__()
    self.update(iterable)

  def __add__(self, other):
    ret = OrderedSet(self)
    ret.update(other)
    return ret

  def add(self, key):
    self[key] = True

  def remove(self, key):
    self.pop(key, None)

  def update(self, iterable):
    for k in iterable:
      self[k] = True

  def difference_update(self, iterable):
    for k in iterable:
      self.pop(k, None)


class AndroidManifest:
  """Helper class to inspect properties of an AndroidManifest.xml file."""
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


class _TransitiveValues:
  """A container for the transitive dependencies of a target.

  This class holds sets of paths for various types of dependencies, such as
  jars, resources, assets, etc.
  """
  # Some values should not be removed when subtracting subtrees.
  _NEVER_REMOVE = frozenset([
      'all_interface_jars',
      'all_input_jars_paths',
      'direct_input_jars_paths',
      'direct_interface_jars',
      'direct_unprocessed_jars',
      'proguard_configs',
  ])

  def __init__(self):
    # Direct classpath:
    self.direct_unprocessed_jars = OrderedSet()
    self.direct_interface_jars = OrderedSet()
    self.direct_input_jars_paths = OrderedSet()
    # Jar files
    self.all_unprocessed_jars = OrderedSet()
    self.all_interface_jars = OrderedSet()
    self.all_input_jars_paths = OrderedSet()
    self.all_dex_files = OrderedSet()
    self.all_processed_jars = OrderedSet()
    # Assets
    self.assets = OrderedSet()
    self.uncompressed_assets = OrderedSet()
    self.locale_paks = OrderedSet()
    # Resources
    self.dependency_zip_overlays = OrderedSet()
    self.dependency_zips = OrderedSet()
    self.extra_package_names = OrderedSet()
    # Other
    self.android_manifests = OrderedSet()
    self.java_resources_jars = OrderedSet()
    self.proguard_configs = OrderedSet()

  def RemoveSubtree(self,
                    other,
                    retain_processed_jars=False,
                    retain_unprocessed_jars=False,
                    retain_resource_zips=False,
                    retain_extra_package_names=False,
                    retain_android_manifests=False):
    """Removes all values from |other| from this instance."""
    for key, value in self.__dict__.items():
      if not (key in _TransitiveValues._NEVER_REMOVE or
              (retain_processed_jars and key == 'all_processed_jars') or
              (retain_unprocessed_jars
               and key in ('all_unprocessed_jars', 'all_interface_jars')) or
              (retain_resource_zips
               and key in ('dependency_zips', 'dependency_zip_overlays')) or
              (retain_extra_package_names and key == 'extra_package_names') or
              (retain_android_manifests and key == 'android_manifests')):
        value.difference_update(getattr(other, key))


def _SortClasspath(dep_list):
  """Sorts a list of dependencies for the classpath.

  Move low priority libraries to the end of the classpath.
  """
  dep_list.sort(key=lambda p: 1 if p.get('low_classpath_priority') else 0)
  return dep_list


class _TransitiveValuesBuilder:

  def __init__(self, params, remove_parent_module_overlap=True):
    self._params = params
    self._remove_parent_module_overlap = remove_parent_module_overlap
    self._ret = _TransitiveValues()

  def Build(self):
    """Computes the transitive dependencies for a given target.

    This is the core logic of the script, collecting all necessary paths and
    metadata from the dependency graph.
    """
    params = self._params

    # System .jar files go into sdk_jars / sdk_interface_jars.
    direct_deps = _SortClasspath(
        params.deps().not_of_type('system_java_library'))
    all_deps = _SortClasspath(
        direct_deps.recursive().not_of_type('system_java_library'))

    all_deps_without_under_test = all_deps
    if apk_under_test_params := params.apk_under_test():
      # Cannot use .append() since this is the cached instance.
      direct_deps = direct_deps + [apk_under_test_params]
      all_deps = _SortClasspath(
          direct_deps.recursive().not_of_type('system_java_library'))

    self._CollectClasspath(direct_deps, all_deps)
    if params.merges_manifests():
      self._CollectManifests(all_deps_without_under_test)
    if params.collects_resources():
      self._CollectResources(all_deps_without_under_test)
    self._CollectExtraPackageNames(direct_deps, all_deps)

    self._ret.proguard_configs.update(params.get('proguard_configs', []))
    self._ret.proguard_configs.update(
        all_deps.collect('proguard_configs', flatten=True))

    if params.is_bundle_module() and self._remove_parent_module_overlap:
      self._RemoveParentModuleOverlap()

    # If there are deps common between dist_jar() and non-dist_jar(), then
    # subtract them off the full classpath to avoid double-deps.
    for dist_jar_params in all_deps.of_type('dist_jar'):
      self._RemoveJarsOwnedByDistJars(dist_jar_params)

    if apk_under_test_params := params.apk_under_test():
      self._RemoveApkUnderTestOverlap(apk_under_test_params)

    if params.is_bundle_module() and not params.is_base_module():
      self._AddBaseModuleToClasspath()
    return self._ret

  def _CollectClasspath(self, direct_deps, all_deps):
    params = self._params
    ret = self._ret
    # Bundle modules do not depend on each other because their names do not
    # match the java naming scheme that identifies them as libraries.
    # TODO(agrieve): Recognize bundle module names as java_library targets so
    #     that they contribute to classpath.
    ret.direct_unprocessed_jars.update(
        direct_deps.collect('unprocessed_jar_path'))
    ret.direct_interface_jars.update(direct_deps.collect('interface_jar_path'))
    ret.all_unprocessed_jars.update(all_deps.collect('unprocessed_jar_path'))
    ret.all_interface_jars.update(all_deps.collect('interface_jar_path'))

    # Deps to add to the compile-time classpath (but not the runtime
    # classpath).
    ret.direct_input_jars_paths.update(params.get('input_jars_paths', []))
    ret.direct_input_jars_paths.update(
        direct_deps.collect('input_jars_paths', flatten=True))
    ret.all_input_jars_paths.update(params.get('input_jars_paths', []))
    ret.all_input_jars_paths.update(
        all_deps.collect('input_jars_paths', flatten=True))

    # Add the target's .jar (except for dist_jar, where it's the output .jar).
    if params.collects_processed_classpath():
      if not params.is_dist_xar():
        if path := params.get('processed_jar_path'):
          ret.all_processed_jars.add(path)
      ret.all_processed_jars.update(all_deps.collect('processed_jar_path'))

    if params.collects_dex_paths():
      if not params.is_dist_xar():
        if path := params.get('dex_path'):
          ret.all_dex_files.add(path)
      ret.all_dex_files.update(all_deps.collect('dex_path'))

  def _AddBaseModuleToClasspath(self):
    base_module = self._params.base_module()
    # Adding base module to classpath to compile against its R.java file
    self._ret.direct_unprocessed_jars.add(base_module['unprocessed_jar_path'])
    self._ret.all_unprocessed_jars.add(base_module['unprocessed_jar_path'])
    self._ret.direct_interface_jars.add(base_module['interface_jar_path'])
    self._ret.all_interface_jars.add(base_module['interface_jar_path'])

  def _CollectManifests(self, all_deps_without_under_test):
    # Manifests are listed from highest priority to lowest priority.
    # Ensure direct manifests come first, then sort the rest by name.
    # https://developer.android.com/build/manage-manifests#merge_priorities
    params = self._params
    ret = self._ret
    ret.android_manifests.update(params.get('mergeable_android_manifests', []))
    indirect_manifests = all_deps_without_under_test.collect(
        'mergeable_android_manifests', flatten=True)
    indirect_manifests.sort(key=lambda p: (os.path.basename(p), p))
    ret.android_manifests.update(indirect_manifests)
    # Prevent the main manifest from showing up in mergeable_android_manifests.
    if path := params.get('android_manifest'):
      if path in ret.android_manifests:
        ret.android_manifests.remove(path)

  def _CollectResources(self, all_deps_without_under_test):
    params = self._params
    ret = self._ret
    resource_deps = params.resource_deps()
    ret.dependency_zips.update(resource_deps.collect('resources_zip'))
    ret.dependency_zip_overlays.update(
        resource_deps.collect('resources_overlay_zip'))

    assets, uncompressed_assets, locale_paks = _MergeAssets(
        all_deps_without_under_test.of_type('android_assets'))
    ret.assets.update(assets)
    ret.uncompressed_assets.update(uncompressed_assets)
    ret.locale_paks = locale_paks

    if params.get('java_resources_jar_path'):
      ret.java_resources_jars.add(params)
    ret.java_resources_jars.update(
        all_deps_without_under_test.collect('java_resources_jar_path'))

  def _CollectExtraPackageNames(self, direct_deps, all_deps):
    # Needed by java_library targets.
    if self._params.is_library():
      # TODO(agrieve): We would ideally move away from being able to use R.java
      # from package_names other than the library's own.
      resource_deps = direct_deps.of_type('android_resources')
    else:
      resource_deps = all_deps.recursive_resource_deps()
    self._ret.extra_package_names.update(resource_deps.collect('package_name'))
    if name := self._params.get('package_name'):
      self._ret.extra_package_names.add(name)

  def _RemoveJarsOwnedByDistJars(self, dist_jar_params):
    """Removes jars from a _TransitiveValues object that are owned by a
    dist_jar.
    """
    if dist_jar_params.get('use_interface_jars'):
      return
    if dist_jar_params.get('direct_deps_only'):
      deps = dist_jar_params.deps()
      dist_jar_tvs = _TransitiveValues()
      dist_jar_tvs.all_unprocessed_jars = deps.collect('unprocessed_jar_path')
      dist_jar_tvs.all_interface_jars = deps.collect('interface_jar_path')
      dist_jar_tvs.all_dex_files = deps.collect('dex_path')
      dist_jar_tvs.all_processed_jars = deps.collect('processed_jar_path')
    else:
      dist_jar_tvs = _TransitiveValuesBuilder(dist_jar_params).Build()

    self._ret.RemoveSubtree(dist_jar_tvs,
                            retain_resource_zips=True,
                            retain_extra_package_names=True)
    self._ret.direct_unprocessed_jars.difference_update(
        dist_jar_tvs.direct_unprocessed_jars)
    self._ret.direct_interface_jars.difference_update(
        dist_jar_tvs.direct_interface_jars)

  def _RemoveParentModuleOverlap(self):
    # There are two approaches to dealing with modules dependencies:
    # 1) Perform steps in android_apk_or_module(), with only the knowledge of
    #    ancesstor splits.
    # 2) Perform steps in android_app_bundle(), with knowledge of full set of
    #    modules. This is required for dex because it can handle the case of
    #    two leaf nodes having the same dep, and promoting that dep to their
    #    common parent.
    #    _PromoteToCommonAncestor() implements this approach.
    #
    # We do 1) unconditionally, and 2) for dex / proguard, but we really
    # should do 2) for all applicable fields.
    if parent_module := self._params.parent_module():
      x = _TransitiveValuesBuilder(parent_module,
                                   remove_parent_module_overlap=False).Build()
      self._ret.RemoveSubtree(x)

  def _RemoveApkUnderTestOverlap(self, apk_under_test_params):
    # The java code for an instrumentation test apk is assembled differently
    # for R8 vs. non-R8.
    #
    # Without R8: Each library's jar is dexed separately and then combined
    # into a single classes.dex. A test apk will include all dex files not
    # present in the apk-under-test. At runtime all test code lives in the
    # test apk, and the program code lives in the apk-under-test.
    #
    # With R8: Each library's .jar file is fed into R8, which outputs
    # a single .jar, which is then dexed into a classes.dex. A test apk
    # includes all jar files from the program and the tests because having
    # them separate doesn't work with R8's whole-program optimizations.
    # Although the apk-under-test still has all of its code in its
    # classes.dex, none of it is used at runtime because the copy within the
    # test apk takes precedence.
    self._ret.RemoveSubtree(
        _TransitiveValuesBuilder(apk_under_test_params).Build(),
        retain_processed_jars=self._params.get('proguard_enabled'),
        retain_unprocessed_jars=True,
        retain_resource_zips=True,
        retain_android_manifests=True)


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
  """Adds mappings from jar path to GN target for a given config."""
  if jar := config.get('unprocessed_jar_path'):
    jar_to_target[jar] = config['gn_target']
  for jar in config.get('input_jars_paths', []):
    jar_to_target[jar] = config['gn_target']


def _PromoteToCommonAncestor(modules, child_to_ancestors, field_name):
  """Finds duplicates of a field across modules and moves them to the
  nearest common ancestor module. This is used for app bundles to avoid
  duplicating dependencies in multiple modules.
  """
  module_to_fields_set = {}
  for module_name, module in modules.items():
    if field_name in module:
      module_to_fields_set[module_name] = set(module[field_name])

  # Find all items that are duplicated across at least two modules.
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
      for o in owning_modules[1:]:
        if ancestor not in child_to_ancestors[o] and ancestor is not o:
          break
      else:
        common_ancestor = ancestor
        break
    else:
      raise Exception('Should have already removed ancestor dupes. ' +
                      ','.join(owning_modules) + ' field=' + field_name +
                      ' dupes: ' + ','.join(dupes))
    # Move the duplicated item to the common ancestor.
    for o in owning_modules:
      module_to_fields_set[o].remove(d)
    module_to_fields_set[common_ancestor].add(d)

  # Update the original modules dictionary with the de-duplicated lists.
  for module_name, module in modules.items():
    if field_name in module:
      module[field_name] = sorted(list(module_to_fields_set[module_name]))


def _DoPlatformChecks(params):
  """Check for platform mismatches between a target and its dependencies."""
  # Robolectric is special in that it's an android target that runs on host.
  # You are allowed to depend on both android |deps_require_android| and
  # non-android |deps_not_support_android| targets.
  if params.get('bypass_platform_checks') or params.get('requires_robolectric'):
    return

  deps_require_android = [d for d in params.deps() if d.requires_android()]
  deps_not_support_android = [
      d for d in params.deps() if not d.supports_android()
  ]

  if deps_require_android and not params.requires_android():
    raise Exception('Some deps require building for the Android platform:\n' +
                    '\n'.join('* ' + d['gn_target']
                              for d in deps_require_android))

  if deps_not_support_android and params.supports_android():
    raise Exception('Not all deps support the Android platform:\n' +
                    '\n'.join('* ' + d['gn_target']
                              for d in deps_not_support_android))


def _SuffixAssets(config, target_config):
  """Adds a suffix to asset paths to avoid collisions."""

  def helper(suffix_names, suffix, assets):
    new_assets = []
    for x in assets:
      src_path, apk_subpath = x.split(':', 1)
      if apk_subpath in suffix_names:
        apk_subpath += suffix
      new_assets.append(f'{src_path}:{apk_subpath}')
    return new_assets

  all_assets = target_config['assets'] + target_config['uncompressed_assets']
  suffix = '+' + target_config['package_name'] + '+'
  suffix_names = {x.split(':', 1)[1].replace(suffix, '') for x in all_assets}
  config['assets'] = helper(suffix_names, suffix, config['assets'])
  config['uncompressed_assets'] = helper(suffix_names, suffix,
                                         config['uncompressed_assets'])
  config['apk_assets_suffixed_list'] = ','.join(f'"assets/{x}"'
                                                for x in sorted(suffix_names))
  config['apk_assets_suffix'] = suffix


def _ToTraceEventRewrittenPath(jar_dir, path):
  """Returns the path to the trace-event-rewritten version of a jar."""
  path = path.replace('../', '')
  path = path.replace('obj/', '')
  path = path.replace('gen/', '')
  path = path.replace('.jar', '.tracing_rewritten.jar')
  return os.path.join(jar_dir, path)


def _CreateLintConfig(params, javac_config, manifest_config):
  # Collect all sources and resources at the apk/bundle_module level.
  aars = set()
  srcjars = set()
  sources = set()
  resource_sources = set()
  resource_zips = set()

  if path := params.get('target_sources_file'):
    sources.add(path)
  if paths := params.get('bundled_srcjars'):
    srcjars.update(paths)
  for c in params.deps().recursive():
    if c.get('chromium_code', True) and c.requires_android():
      if path := c.get('target_sources_file'):
        sources.add(path)
      if paths := c.get('bundled_srcjars'):
        srcjars.update(paths)
    if path := c.get('aar_path'):
      aars.add(path)

  for c in params.resource_deps():
    if c.get('chromium_code', True):
      # Prefer res_sources_path to resources_zips so that lint errors have
      # real paths and to avoid needing to extract during lint.
      if path := c.get('res_sources_path'):
        resource_sources.add(path)
      else:
        resource_zips.add(
            c.get('resources_zip') or c.get('resources_overlay_zip'))

  if params.is_bundle():
    classpath = OrderedSet()
    manifests = OrderedSet(p['android_manifest'] for p in params.module_deps())
    for m in params.module_deps():
      classpath.update(
          m.javac_build_config_json()['javac_full_interface_classpath'])
      manifests.update(
          m.manifest_build_config_json()['extra_android_manifests'])
    classpath = list(classpath)
    manifests = list(manifests)
  else:
    classpath = javac_config['javac_full_interface_classpath']
    manifests = [params['android_manifest']]
    manifests += manifest_config['extra_android_manifests']

  config = {}
  config['aars'] = sorted(aars)
  config['android_manifests'] = manifests
  config['classpath'] = classpath
  config['sources'] = sorted(sources)
  config['srcjars'] = sorted(srcjars)
  config['resource_sources'] = sorted(resource_sources)
  config['resource_zips'] = sorted(resource_zips)
  return config


def main():
  parser = argparse.ArgumentParser(
      description='Writes a .build_config.json file.')
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--output', help='.build_config.json to write.')
  options = parser.parse_args()
  build_config_path = options.output

  params = params_json_util.get_params(
      build_config_path.replace('.build_config.json', '.params.json'))

  if lines := params.get('fail'):
    parser.error('\n'.join(lines))

  target_type = params.type

  is_bundle_module = params.is_bundle_module()
  is_apk = params.is_apk()
  is_apk_or_module = is_apk or is_bundle_module
  is_bundle = params.is_bundle()
  has_classpath = params.has_classpath()
  proguard_enabled = params.get('proguard_enabled', False)

  if has_classpath:
    _DoPlatformChecks(params)

  if is_bundle_module:
    if parent_module_params := params.parent_module():
      # Validate uses_split matches the parent split's name.
      parent_split_name = params.get('uses_split', 'base')
      actual = parent_module_params['module_name']
      assert actual == parent_split_name, (
          f'uses_split={parent_split_name} but parent was {actual}')

  if is_bundle:
    deps_proguard_enabled = []
    deps_proguard_disabled = []
    for module in params.module_deps():
      if module.get('proguard_enabled'):
        deps_proguard_enabled.append(module['module_name'])
      else:
        deps_proguard_disabled.append(module['module_name'])
    if deps_proguard_enabled and deps_proguard_disabled:
      raise Exception('Deps %s have proguard enabled while deps %s have '
                      'proguard disabled' %
                      (deps_proguard_enabled, deps_proguard_disabled))

  apk_under_test_params = params.apk_under_test()

  if is_apk and apk_under_test_params:
    if apk_under_test_params['proguard_enabled']:
      assert proguard_enabled, (
          'proguard must be enabled for '
          'instrumentation apks if it\'s enabled for the tested apk.')

  if is_apk_or_module:
    manifest = AndroidManifest(params['android_manifest'])
    if not apk_under_test_params and manifest.GetInstrumentationElements():
      # This must then have instrumentation only for itself.
      manifest.CheckInstrumentationElements(manifest.GetPackageName())

  main_config = {}
  # Separate to prevent APK / bundle-related values from invalidating
  # compile_java.py, and to minimize the .json that compile_java.py needs to
  # parse.
  javac_config = {}
  # Separate to prevent transitive classpath changes invalidating turbine.py.
  turbine_config = {}
  # Separate because so few targets enable lint.
  lint_config = {}
  # Separate to prevent keys other than extra_android_manifests from
  # invalidating merge_manifest.py.
  manifest_config = {}
  # Separate to prevent .java changes invalidating compile_resources.py, and
  # new resource targets from invalidating java compiles.
  res_config = {}
  # Separate to prevent .java changes invalidating create_r_java.py, and new
  # resource targets from invalidating java compiles.
  rtxt_config = {}
  # Separate to save targets that don't need it from having to parse it.
  targets_config = {}

  if is_apk:
    main_config['apk_path'] = params['apk_path']
    if path := params.get('incremental_install_json_path'):
      main_config['incremental_install_json_path'] = path
      main_config['incremental_apk_path'] = params['incremental_apk_path']

  if is_bundle_module:
    main_config['unprocessed_jar_path'] = params['unprocessed_jar_path']
    main_config['res_size_info_path'] = params['res_size_info_path']

  if has_classpath:
    tv = _TransitiveValuesBuilder(params).Build()
    sdk_deps = params.deps().of_type('system_java_library')

    javac_full_classpath = (list(tv.all_unprocessed_jars) +
                            list(tv.all_input_jars_paths))

    if params.needs_full_javac_classpath():
      main_config['javac_full_classpath'] = javac_full_classpath
      main_config['sdk_jars'] = sdk_deps.collect('unprocessed_jar_path')

    if params.collects_processed_classpath():
      main_config['processed_classpath'] = list(tv.all_processed_jars)
      if trace_events_jar_dir := params.get('trace_events_jar_dir'):
        main_config['trace_event_rewritten_classpath'] = [
            _ToTraceEventRewrittenPath(trace_events_jar_dir, p)
            for p in tv.all_processed_jars
        ]

    if params.collects_dex_paths():
      main_config['all_dex_files'] = list(tv.all_dex_files)

    if params.needs_transitive_rtxt():
      rtxt_config['dependency_rtxt_files'] = (
          params.resource_deps().collect('rtxt_path'))

    if proguard_enabled or target_type == 'dist_aar':
      main_config['proguard_all_configs'] = sorted(tv.proguard_configs)

    if proguard_enabled:
      main_config['proguard_classpath_jars'] = sorted(tv.all_input_jars_paths)

    if is_apk_or_module:
      main_config['java_resources_jars'] = sorted(tv.java_resources_jars)

  if params.is_compile_type():
    # Needed by turbine.py and check_for_missing_direct_deps.py:
    turbine_config['interface_classpath'] = list(
        tv.direct_interface_jars) + list(tv.direct_input_jars_paths)
    # processor_configs will be of type 'java_annotation_processor', and so not
    # included in deps().recursive().of_type('java_library'). Annotation
    # processors run as part of the build, so need processed_jar_path.
    processor_deps = params.processor_deps()
    turbine_config['processor_classpath'] = _SortClasspath(
        processor_deps.recursive()).collect('processed_jar_path')
    turbine_config['processor_classes'] = sorted(
        processor_deps.collect('main_class'))

    sdk_interface_jars = sdk_deps.collect('interface_jar_path')
    turbine_config['sdk_interface_jars'] = sdk_interface_jars

    javac_config['javac_full_interface_classpath'] = (
        list(tv.all_interface_jars) + list(tv.all_input_jars_paths))
    # Duplicate so that compile_java.py does not need to read another .json.
    javac_config['sdk_interface_jars'] = sdk_interface_jars

  if params.is_dist_xar():
    if params.get('direct_deps_only'):
      if params.get('use_interface_jars'):
        dist_jars = tv.direct_interface_jars
      else:
        dist_jars = params.deps().collect('processed_jar_path')
    elif params.get('use_interface_jars'):
      dist_jars = tv.all_interface_jars
    else:
      dist_jars = tv.all_processed_jars

    main_config['dist_classpath'] = list(dist_jars)

  if params.collects_resources():
    main_config['assets'] = sorted(tv.assets)
    main_config['uncompressed_assets'] = sorted(tv.uncompressed_assets)

  if params.get('create_locales_java_list'):
    main_config['locales_java_list'] = _CreateJavaLocaleListFromAssets(
        tv.uncompressed_assets, tv.locale_paks)

  if params.collects_resources():
    # Safe to sort: Build checks that non-overlay resource have no overlap.
    res_config['dependency_zips'] = sorted(tv.dependency_zips)

    # Reverse overlay list so that dependents override dependencies
    # The topological walk puts dependencies before dependents, but for resource
    # overlays we need dependents to come after dependencies in AAPT2's -R list
    overlay_list = list(reversed(tv.dependency_zip_overlays))
    res_config['dependency_zip_overlays'] = overlay_list

    if params.compiles_resources():
      res_config['extra_package_names'] = sorted(tv.extra_package_names)
      if apk_under_test_params:
        assert is_apk
        res_config['arsc_package_name'] = (
            apk_under_test_params.build_config_json()['package_name'])

  if params.merges_manifests():
    manifest_config['extra_android_manifests'] = list(tv.android_manifests)

  if is_bundle:
    module_deps = params.module_deps()
    module_params_by_name = {m['module_name']: m for m in module_deps}
    module_configs_by_name = {
        m['module_name']: m.build_config_json()
        for m in module_deps
    }
    modules = {m: {} for m in module_params_by_name}

    child_to_ancestors = {n: [] for n in module_params_by_name}
    for name, module in module_params_by_name.items():
      while module := module.parent_module():
        child_to_ancestors[name].append(module['module_name'])

    per_module_fields = [
        'processed_classpath',
        'trace_event_rewritten_classpath',
        'all_dex_files',
        'assets',
        'uncompressed_assets',
    ]
    union_fields = {}
    if params.get('trace_events_jar_dir'):
      union_fields['processed_classpath'] = 'processed_classpath'
      union_fields['trace_event_rewritten_classpath'] = (
          'trace_event_rewritten_classpath')
    if proguard_enabled:
      union_fields['proguard_classpath_jars'] = 'proguard_classpath_jars'
      union_fields['proguard_all_configs'] = 'proguard_all_configs'
      union_fields['sdk_jars'] = 'sdk_jars'

    unioned_values = {k: OrderedSet() for k in union_fields}
    for n, c in module_configs_by_name.items():
      module_params = module_params_by_name[n]
      if n == 'base':
        assert 'base_module_config' not in main_config, (
            'Must have exactly 1 base module!')
        main_config['package_name'] = c['package_name']
        main_config['version_code'] = module_params['version_code']
        main_config['version_name'] = module_params['version_name']
        main_config['base_module_config'] = module_params.path
        main_config['android_manifest'] = module_params['android_manifest']

      for dst_key, src_key in union_fields.items():
        unioned_values[dst_key].update(c[src_key])

      module = modules[n]
      module['final_dex_path'] = module_params['final_dex_path']
      for f in per_module_fields:
        if f in c:
          module[f] = c[f]

    for dst_key in union_fields:
      main_config[dst_key] = list(unioned_values[dst_key])

    # Promote duplicates from siblings/cousins.
    for f in per_module_fields:
      _PromoteToCommonAncestor(modules, child_to_ancestors, f)
    main_config['modules'] = modules

  if is_apk_or_module:
    main_config['package_name'] = manifest.GetPackageName()
    main_config['android_manifest'] = params['android_manifest']
    main_config['merged_android_manifest'] = params['merged_android_manifest']

    if is_apk:
      main_config['version_code'] = params['version_code']
      main_config['version_name'] = params['version_name']

    # TrichromeLibrary has no dex.
    if final_dex_path := params.get('final_dex_path'):
      main_config['final_dex_path'] = final_dex_path

    library_paths = params.native_libraries()
    secondary_abi_libraries = params.secondary_abi_native_libraries()
    paths_without_parent_dirs = [
        p for p in secondary_abi_libraries if os.path.sep not in p
    ]
    if paths_without_parent_dirs:
      sys.stderr.write('Found secondary native libraries from primary '
                       'toolchain directory. This is a bug!\n')
      sys.stderr.write('\n'.join(paths_without_parent_dirs))
      sys.stderr.write('\n\nIt may be helpful to run: \n')
      sys.stderr.write('    gn path out/Default //chrome/android:'
                       'monochrome_secondary_abi_lib //base:base\n')
      sys.exit(1)

    main_config['native'] = {}
    main_config['native']['libraries'] = library_paths
    main_config['native']['secondary_abi_libraries'] = secondary_abi_libraries

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

      main_config['native']['loadable_modules'] = loadable_modules
      main_config['native']['placeholders'] = placeholder_paths
      main_config['native'][
          'secondary_abi_loadable_modules'] = secondary_abi_loadable_modules
      main_config['native'][
          'secondary_abi_placeholders'] = secondary_abi_placeholder_paths
      main_config['native']['library_always_compress'] = params.get(
          'library_always_compress')
      main_config['proto_resources_path'] = params['proto_resources_path']
      main_config['base_allowlist_rtxt_path'] = params[
          'base_allowlist_rtxt_path']
      main_config['rtxt_path'] = params['rtxt_path']
      main_config['module_pathmap_path'] = params['module_pathmap_path']

  if is_apk_or_module or target_type == 'robolectric_binary':
    if path := params.get('suffix_apk_assets_used_by_config'):
      if path == build_config_path:
        target_config = main_config
      else:
        target_config = params_json_util.get_build_config(path)
      _SuffixAssets(main_config, target_config)

  if params.get('enable_bytecode_checks'):
    jar_to_target = {}
    all_params = params.deps() + [params]
    if apk_under_test_params:
      all_params.append(apk_under_test_params)
    for d in all_params.recursive():
      _AddJarMapping(jar_to_target, d)

    # Used by check_for_missing_direct_deps.py to give better error message
    # when missing deps are found. Both javac_full_classpath_targets and
    # javac_full_interface_classpath must be in identical orders, as they get
    # passed as separate arrays and then paired up based on index.
    targets_config['javac_full_classpath_targets'] = [
        jar_to_target[x] for x in javac_full_classpath
    ]

  if params.get('enable_lint'):
    lint_config = _CreateLintConfig(params, javac_config, manifest_config)

  # Depfiles expect output order to match the order in GN.
  outputs = [
      (main_config, '.build_config.json'),
      (javac_config, '.javac.build_config.json'),
      (turbine_config, '.turbine.build_config.json'),
      (lint_config, '.lint.build_config.json'),
      (manifest_config, '.manifest.build_config.json'),
      (res_config, '.res.build_config.json'),
      (rtxt_config, '.rtxt.build_config.json'),
      (targets_config, '.targets.build_config.json'),
  ]

  first_output = None
  for config, extension in outputs:
    if config:
      path = build_config_path.replace('.build_config.json', extension)
      first_output = first_output or path
      build_utils.WriteJson(config, path, only_if_changed=True)

  if options.depfile:
    all_inputs = params_json_util.all_read_file_paths()
    if path := params.get('shared_libraries_runtime_deps_file'):
      # Path must be added to depfile because the GN template does not add this
      # input to avoid having to depend on the generated_file() target.
      all_inputs.append(path)
    if path := params.get('secondary_abi_shared_libraries_runtime_deps_file'):
      all_inputs.append(path)
    action_helpers.write_depfile(options.depfile, first_output, all_inputs)


if __name__ == '__main__':
  main()
