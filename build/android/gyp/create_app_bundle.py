#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create an Android application bundle from one or more bundle modules."""

import argparse
import json
import os
import shutil
import sys
import zipfile

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from pylib.utils import dexdump

from util import build_utils
from util import manifest_utils
from util import resource_utils
from xml.etree import ElementTree

import bundletool

# Location of language-based assets in bundle modules.
_LOCALES_SUBDIR = 'assets/locales/'

# The fallback locale should always have its .pak file included in
# the base apk, i.e. not use language-based asset targetting. This ensures
# that Chrome won't crash on startup if its bundle is installed on a device
# with an unsupported system locale (e.g. fur-rIT).
_FALLBACK_LOCALE = 'en-US'

# List of split dimensions recognized by this tool.
_ALL_SPLIT_DIMENSIONS = [ 'ABI', 'SCREEN_DENSITY', 'LANGUAGE' ]

# Due to historical reasons, certain languages identified by Chromium with a
# 3-letters ISO 639-2 code, are mapped to a nearly equivalent 2-letters
# ISO 639-1 code instead (due to the fact that older Android releases only
# supported the latter when matching resources).
#
# the same conversion as for Java resources.
_SHORTEN_LANGUAGE_CODE_MAP = {
  'fil': 'tl',  # Filipino to Tagalog.
}

# A list of extensions corresponding to files that should never be compressed
# in the bundle. This used to be handled by bundletool automatically until
# release 0.8.0, which required that this be passed to the BundleConfig
# file instead.
#
# This is the original list, which was taken from aapt2, with 'webp' added to
# it (which curiously was missing from the list).
_UNCOMPRESSED_FILE_EXTS = [
    '3g2', '3gp', '3gpp', '3gpp2', 'aac', 'amr', 'awb', 'git', 'imy', 'jet',
    'jpeg', 'jpg', 'm4a', 'm4v', 'mid', 'midi', 'mkv', 'mp2', 'mp3', 'mp4',
    'mpeg', 'mpg', 'ogg', 'png', 'rtttl', 'smf', 'wav', 'webm', 'webp', 'wmv',
    'xmf'
]


def _ParseArgs(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--out-bundle', required=True,
                      help='Output bundle zip archive.')
  parser.add_argument('--module-zips', required=True,
                      help='GN-list of module zip archives.')
  parser.add_argument(
      '--pathmap-in-paths',
      action='append',
      help='List of module pathmap files.')
  parser.add_argument(
      '--module-name',
      action='append',
      dest='module_names',
      help='List of module names.')
  parser.add_argument(
      '--pathmap-out-path', help='Path to combined pathmap file for bundle.')
  parser.add_argument(
      '--rtxt-in-paths', action='append', help='GN-list of module R.txt files.')
  parser.add_argument(
      '--rtxt-out-path', help='Path to combined R.txt file for bundle.')
  parser.add_argument('--uncompressed-assets', action='append',
                      help='GN-list of uncompressed assets.')
  parser.add_argument(
      '--compress-shared-libraries',
      action='store_true',
      help='Whether to store native libraries compressed.')
  parser.add_argument('--split-dimensions',
                      help="GN-list of split dimensions to support.")
  parser.add_argument(
      '--base-module-rtxt-path',
      help='Optional path to the base module\'s R.txt file, only used with '
      'language split dimension.')
  parser.add_argument(
      '--base-allowlist-rtxt-path',
      help='Optional path to an R.txt file, string resources '
      'listed there _and_ in --base-module-rtxt-path will '
      'be kept in the base bundle module, even if language'
      ' splitting is enabled.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')

  parser.add_argument(
      '--validate-services',
      action='store_true',
      help='Check if services are in base module if isolatedSplits is enabled.')

  options = parser.parse_args(args)
  options.module_zips = build_utils.ParseGnList(options.module_zips)
  options.rtxt_in_paths = build_utils.ParseGnList(options.rtxt_in_paths)
  options.pathmap_in_paths = build_utils.ParseGnList(options.pathmap_in_paths)

  if len(options.module_zips) == 0:
    raise Exception('The module zip list cannot be empty.')

  # Merge all uncompressed assets into a set.
  uncompressed_list = []
  if options.uncompressed_assets:
    for l in options.uncompressed_assets:
      for entry in build_utils.ParseGnList(l):
        # Each entry has the following format: 'zipPath' or 'srcPath:zipPath'
        pos = entry.find(':')
        if pos >= 0:
          uncompressed_list.append(entry[pos + 1:])
        else:
          uncompressed_list.append(entry)

  options.uncompressed_assets = set(uncompressed_list)

  # Check that all split dimensions are valid
  if options.split_dimensions:
    options.split_dimensions = build_utils.ParseGnList(options.split_dimensions)
    for dim in options.split_dimensions:
      if dim.upper() not in _ALL_SPLIT_DIMENSIONS:
        parser.error('Invalid split dimension "%s" (expected one of: %s)' % (
            dim, ', '.join(x.lower() for x in _ALL_SPLIT_DIMENSIONS)))

  # As a special case, --base-allowlist-rtxt-path can be empty to indicate
  # that the module doesn't need such a allowlist. That's because it is easier
  # to check this condition here than through GN rules :-(
  if options.base_allowlist_rtxt_path == '':
    options.base_module_rtxt_path = None

  # Check --base-module-rtxt-path and --base-allowlist-rtxt-path usage.
  if options.base_module_rtxt_path:
    if not options.base_allowlist_rtxt_path:
      parser.error(
          '--base-module-rtxt-path requires --base-allowlist-rtxt-path')
    if 'language' not in options.split_dimensions:
      parser.error('--base-module-rtxt-path is only valid with '
                   'language-based splits.')

  return options


def _MakeSplitDimension(value, enabled):
  """Return dict modelling a BundleConfig splitDimension entry."""
  return {'value': value, 'negate': not enabled}


def _GenerateBundleConfigJson(uncompressed_assets, compress_shared_libraries,
                              split_dimensions, base_master_resource_ids):
  """Generate a dictionary that can be written to a JSON BuildConfig.

  Args:
    uncompressed_assets: A list or set of file paths under assets/ that always
      be stored uncompressed.
    compress_shared_libraries: Boolean, whether to compress native libs.
    split_dimensions: list of split dimensions.
    base_master_resource_ids: Optional list of 32-bit resource IDs to keep
      inside the base module, even when split dimensions are enabled.
  Returns:
    A dictionary that can be written as a json file.
  """
  # Compute splitsConfig list. Each item is a dictionary that can have
  # the following keys:
  #    'value': One of ['LANGUAGE', 'DENSITY', 'ABI']
  #    'negate': Boolean, True to indicate that the bundle should *not* be
  #              split (unused at the moment by this script).

  split_dimensions = [ _MakeSplitDimension(dim, dim in split_dimensions)
                       for dim in _ALL_SPLIT_DIMENSIONS ]

  # Native libraries loaded by the crazy linker.
  # Whether other .so files are compressed is controlled by
  # "uncompressNativeLibraries".
  uncompressed_globs = ['lib/*/crazy.*']
  # Locale-specific pak files stored in bundle splits need not be compressed.
  uncompressed_globs.extend(
      ['assets/locales#lang_*/*.pak', 'assets/fallback-locales/*.pak'])
  uncompressed_globs.extend('assets/' + x for x in uncompressed_assets)
  # NOTE: Use '**' instead of '*' to work through directories!
  uncompressed_globs.extend('**.' + ext for ext in _UNCOMPRESSED_FILE_EXTS)

  data = {
      'optimizations': {
          'splitsConfig': {
              'splitDimension': split_dimensions,
          },
          'uncompressNativeLibraries': {
              'enabled': not compress_shared_libraries,
          },
          'uncompressDexFiles': {
              'enabled': True,  # Applies only for P+.
          }
      },
      'compression': {
          'uncompressedGlob': sorted(uncompressed_globs),
      },
  }

  if base_master_resource_ids:
    data['master_resources'] = {
        'resource_ids': list(base_master_resource_ids),
    }

  return json.dumps(data, indent=2)


def _RewriteLanguageAssetPath(src_path):
  """Rewrite the destination path of a locale asset for language-based splits.

  Should only be used when generating bundles with language-based splits.
  This will rewrite paths that look like locales/<locale>.pak into
  locales#<language>/<locale>.pak, where <language> is the language code
  from the locale.

  Returns new path.
  """
  if not src_path.startswith(_LOCALES_SUBDIR) or not src_path.endswith('.pak'):
    return [src_path]

  locale = src_path[len(_LOCALES_SUBDIR):-4]
  android_locale = resource_utils.ToAndroidLocaleName(locale)

  # The locale format is <lang>-<region> or <lang> or BCP-47 (e.g b+sr+Latn).
  # Extract the language.
  pos = android_locale.find('-')
  if android_locale.startswith('b+'):
    # If locale is in BCP-47 the language is the second tag (e.g. b+sr+Latn)
    android_language = android_locale.split('+')[1]
  elif pos >= 0:
    android_language = android_locale[:pos]
  else:
    android_language = android_locale

  if locale == _FALLBACK_LOCALE:
    # Fallback locale .pak files must be placed in a different directory
    # to ensure they are always stored in the base module.
    result_path = 'assets/fallback-locales/%s.pak' % locale
  else:
    # Other language .pak files go into a language-specific asset directory
    # that bundletool will store in separate split APKs.
    result_path = 'assets/locales#lang_%s/%s.pak' % (android_language, locale)

  return result_path


def _SplitModuleForAssetTargeting(src_module_zip, tmp_dir, split_dimensions):
  """Splits assets in a module if needed.

  Args:
    src_module_zip: input zip module path.
    tmp_dir: Path to temporary directory, where the new output module might
      be written to.
    split_dimensions: list of split dimensions.

  Returns:
    If the module doesn't need asset targeting, doesn't do anything and
    returns src_module_zip. Otherwise, create a new module zip archive under
    tmp_dir with the same file name, but which contains assets paths targeting
    the proper dimensions.
  """
  split_language = 'LANGUAGE' in split_dimensions
  if not split_language:
    # Nothing to target, so return original module path.
    return src_module_zip

  with zipfile.ZipFile(src_module_zip, 'r') as src_zip:
    language_files = [
      f for f in src_zip.namelist() if f.startswith(_LOCALES_SUBDIR)]

    if not language_files:
      # Not language-based assets to split in this module.
      return src_module_zip

    tmp_zip = os.path.join(tmp_dir, os.path.basename(src_module_zip))
    with zipfile.ZipFile(tmp_zip, 'w') as dst_zip:
      for info in src_zip.infolist():
        src_path = info.filename
        is_compressed = info.compress_type != zipfile.ZIP_STORED

        dst_path = src_path
        if src_path in language_files:
          dst_path = _RewriteLanguageAssetPath(src_path)

        build_utils.AddToZipHermetic(
            dst_zip,
            dst_path,
            data=src_zip.read(src_path),
            compress=is_compressed)

    return tmp_zip


def _GenerateBaseResourcesAllowList(base_module_rtxt_path,
                                    base_allowlist_rtxt_path):
  """Generate a allowlist of base master resource ids.

  Args:
    base_module_rtxt_path: Path to base module R.txt file.
    base_allowlist_rtxt_path: Path to base allowlist R.txt file.
  Returns:
    list of resource ids.
  """
  ids_map = resource_utils.GenerateStringResourcesAllowList(
      base_module_rtxt_path, base_allowlist_rtxt_path)
  return ids_map.keys()


def _ConcatTextFiles(in_paths, out_path):
  """Concatenate the contents of multiple text files into one.

  The each file contents is preceded by a line containing the original filename.

  Args:
    in_paths: List of input file paths.
    out_path: Path to output file.
  """
  with open(out_path, 'w') as out_file:
    for in_path in in_paths:
      if not os.path.exists(in_path):
        continue
      with open(in_path, 'r') as in_file:
        out_file.write('-- Contents of {}\n'.format(os.path.basename(in_path)))
        out_file.write(in_file.read())


def _LoadPathmap(pathmap_path):
  """Load the pathmap of obfuscated resource paths.

  Returns: A dict mapping from obfuscated paths to original paths or an
           empty dict if passed a None |pathmap_path|.
  """
  if pathmap_path is None:
    return {}

  pathmap = {}
  with open(pathmap_path, 'r') as f:
    for line in f:
      line = line.strip()
      if line.startswith('--') or line == '':
        continue
      original, renamed = line.split(' -> ')
      pathmap[renamed] = original
  return pathmap


def _WriteBundlePathmap(module_pathmap_paths, module_names,
                        bundle_pathmap_path):
  """Combine the contents of module pathmaps into a bundle pathmap.

  This rebases the resource paths inside the module pathmap before adding them
  to the bundle pathmap. So res/a.xml inside the base module pathmap would be
  base/res/a.xml in the bundle pathmap.
  """
  with open(bundle_pathmap_path, 'w') as bundle_pathmap_file:
    for module_pathmap_path, module_name in zip(module_pathmap_paths,
                                                module_names):
      if not os.path.exists(module_pathmap_path):
        continue
      module_pathmap = _LoadPathmap(module_pathmap_path)
      for short_path, long_path in module_pathmap.items():
        rebased_long_path = '{}/{}'.format(module_name, long_path)
        rebased_short_path = '{}/{}'.format(module_name, short_path)
        line = '{} -> {}\n'.format(rebased_long_path, rebased_short_path)
        bundle_pathmap_file.write(line)


def _GetManifestForModule(bundle_path, module_name):
  return ElementTree.fromstring(
      bundletool.RunBundleTool([
          'dump', 'manifest', '--bundle', bundle_path, '--module', module_name
      ]))


def _GetComponentNames(manifest, tag_name):
  android_name = '{%s}name' % manifest_utils.ANDROID_NAMESPACE
  return [s.attrib.get(android_name) for s in manifest.iter(tag_name)]


def _MaybeCheckServicesAndProvidersPresentInBase(bundle_path, module_zips):
  """Checks bundles with isolated splits define all services in the base module.

  Due to b/169196314, service classes are not found if they are not present in
  the base module. Providers are also checked because they are loaded early in
  startup, and keeping them in the base module gives more time for the chrome
  split to load.
  """
  base_manifest = _GetManifestForModule(bundle_path, 'base')
  isolated_splits = base_manifest.get('{%s}isolatedSplits' %
                                      manifest_utils.ANDROID_NAMESPACE)
  if isolated_splits != 'true':
    return

  # Collect service names from all split manifests.
  base_zip = None
  service_names = _GetComponentNames(base_manifest, 'service')
  provider_names = _GetComponentNames(base_manifest, 'provider')
  for module_zip in module_zips:
    name = os.path.basename(module_zip)[:-len('.zip')]
    if name == 'base':
      base_zip = module_zip
    else:
      service_names.extend(
          _GetComponentNames(_GetManifestForModule(bundle_path, name),
                             'service'))
      module_providers = _GetComponentNames(
          _GetManifestForModule(bundle_path, name), 'provider')
      if module_providers:
        raise Exception("Providers should all be declared in the base manifest."
                        " '%s' module declared: %s" % (name, module_providers))

  # Extract classes from the base module's dex.
  classes = set()
  base_package_name = manifest_utils.GetPackage(base_manifest)
  for package in dexdump.Dump(base_zip):
    for name, package_dict in package.items():
      if not name:
        name = base_package_name
      classes.update('%s.%s' % (name, c)
                     for c in package_dict['classes'].keys())

  ignored_service_names = {
      # Defined in the chime DFM manifest, but unused.
      # org.chromium.chrome.browser.chime.ScheduledTaskService is used instead.
      ("com.google.android.libraries.notifications.entrypoints.scheduled."
       "ScheduledTaskService"),

      # Defined in the chime DFM manifest, only used pre-O (where isolated
      # splits are not supported).
      ("com.google.android.libraries.notifications.executor.impl.basic."
       "ChimeExecutorApiService"),
  }

  # Ensure all services are present in base module.
  for service_name in service_names:
    if service_name not in classes:
      if service_name in ignored_service_names:
        continue
      raise Exception("Service %s should be present in the base module's dex."
                      " See b/169196314 for more details." % service_name)

  # Ensure all providers are present in base module.
  for provider_name in provider_names:
    if provider_name not in classes:
      raise Exception(
          "Provider %s should be present in the base module's dex." %
          provider_name)


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  split_dimensions = []
  if options.split_dimensions:
    split_dimensions = [x.upper() for x in options.split_dimensions]


  with build_utils.TempDir() as tmp_dir:
    module_zips = [
        _SplitModuleForAssetTargeting(module, tmp_dir, split_dimensions) \
        for module in options.module_zips]

    base_master_resource_ids = None
    if options.base_module_rtxt_path:
      base_master_resource_ids = _GenerateBaseResourcesAllowList(
          options.base_module_rtxt_path, options.base_allowlist_rtxt_path)

    bundle_config = _GenerateBundleConfigJson(
        options.uncompressed_assets, options.compress_shared_libraries,
        split_dimensions, base_master_resource_ids)

    tmp_bundle = os.path.join(tmp_dir, 'tmp_bundle')

    # Important: bundletool requires that the bundle config file is
    # named with a .pb.json extension.
    tmp_bundle_config = tmp_bundle + '.BundleConfig.pb.json'

    with open(tmp_bundle_config, 'w') as f:
      f.write(bundle_config)

    cmd_args = build_utils.JavaCmd(options.warnings_as_errors) + [
        '-jar',
        bundletool.BUNDLETOOL_JAR_PATH,
        'build-bundle',
        '--modules=' + ','.join(module_zips),
        '--output=' + tmp_bundle,
        '--config=' + tmp_bundle_config,
    ]

    build_utils.CheckOutput(
        cmd_args,
        print_stdout=True,
        print_stderr=True,
        stderr_filter=build_utils.FilterReflectiveAccessJavaWarnings,
        fail_on_output=options.warnings_as_errors)

    if options.validate_services:
      # TODO(crbug.com/1126301): This step takes 0.4s locally for bundles with
      # isolated splits disabled and 2s for bundles with isolated splits
      # enabled.  Consider making this run in parallel or move into a separate
      # step before enabling isolated splits by default.
      _MaybeCheckServicesAndProvidersPresentInBase(tmp_bundle, module_zips)

    shutil.move(tmp_bundle, options.out_bundle)

  if options.rtxt_out_path:
    _ConcatTextFiles(options.rtxt_in_paths, options.rtxt_out_path)

  if options.pathmap_out_path:
    _WriteBundlePathmap(options.pathmap_in_paths, options.module_names,
                        options.pathmap_out_path)


if __name__ == '__main__':
  main(sys.argv[1:])
