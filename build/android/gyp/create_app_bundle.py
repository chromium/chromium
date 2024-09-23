#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create an Android application bundle from one or more bundle modules."""

import argparse
import concurrent.futures
import json
import logging
import os
import posixpath
import shutil
import sys
from xml.etree import ElementTree
import zipfile

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from pylib.utils import dexdump

import bundletool
from util import build_utils
from util import manifest_utils
from util import resource_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


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

_COMPONENT_TYPES = ('activity', 'provider', 'receiver', 'service')
_DEDUPE_ENTRY_TYPES = _COMPONENT_TYPES + ('activity-alias', 'meta-data')

_ROTATION_METADATA_KEY = 'com.google.play.apps.signing/RotationConfig.textproto'


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
  parser.add_argument('--compress-dex',
                      action='store_true',
                      help='Compress .dex files')
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
  parser.add_argument('--rotation-config',
                      help='Path to a RotationConfig.textproto')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')

  parser.add_argument(
      '--validate-services',
      action='store_true',
      help='Check if services are in base module if isolatedSplits is enabled.')

  options = parser.parse_args(args)
  options.module_zips = action_helpers.parse_gn_list(options.module_zips)

  if len(options.module_zips) == 0:
    parser.error('The module zip list cannot be empty.')
  if len(options.module_zips) != len(options.module_names):
    parser.error('# module zips != # names.')
  if 'base' not in options.module_names:
    parser.error('Missing base module.')

  # Sort modules for more stable outputs.
  per_module_values = list(
      zip(options.module_names, options.module_zips,
          options.uncompressed_assets, options.rtxt_in_paths,
          options.pathmap_in_paths))
  per_module_values.sort(key=lambda x: (x[0] != 'base', x[0]))
  options.module_names = [x[0] for x in per_module_values]
  options.module_zips = [x[1] for x in per_module_values]
  options.uncompressed_assets = [x[2] for x in per_module_values]
  options.rtxt_in_paths = [x[3] for x in per_module_values]
  options.pathmap_in_paths = [x[4] for x in per_module_values]

  options.rtxt_in_paths = action_helpers.parse_gn_list(options.rtxt_in_paths)
  options.pathmap_in_paths = action_helpers.parse_gn_list(
      options.pathmap_in_paths)

  # Merge all uncompressed assets into a set.
  uncompressed_list = []
  for entry in action_helpers.parse_gn_list(options.uncompressed_assets):
    # Each entry has the following format: 'zipPath' or 'srcPath:zipPath'
    pos = entry.find(':')
    if pos >= 0:
      uncompressed_list.append(entry[pos + 1:])
    else:
      uncompressed_list.append(entry)

  options.uncompressed_assets = set(uncompressed_list)

  # Check that all split dimensions are valid
  if options.split_dimensions:
    options.split_dimensions = action_helpers.parse_gn_list(
        options.split_dimensions)
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


def _GenerateBundleConfigJson(uncompressed_assets, compress_dex,
                              split_dimensions, base_master_resource_ids):
  """Generate a dictionary that can be written to a JSON BuildConfig.

  Args:
    uncompressed_assets: A list or set of file paths under assets/ that always
      be stored uncompressed.
    compressed_dex: Boolean, whether to compress .dex.
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

  # Locale-specific pak files stored in bundle splits need not be compressed.
  uncompressed_globs = [
      'assets/locales#lang_*/*.pak', 'assets/fallback-locales/*.pak'
  ]
  # normpath to allow for ../ prefix.
  uncompressed_globs.extend(
      posixpath.normpath('assets/' + x) for x in uncompressed_assets)
  # NOTE: Use '**' instead of '*' to work through directories!
  uncompressed_globs.extend('**.' + ext for ext in _UNCOMPRESSED_FILE_EXTS)
  if not compress_dex:
    # Explicit glob required only when using bundletool to create .apks files.
    # Play Store looks for and respects "uncompressDexFiles" set below.
    # b/176198991
    # This is added as a placeholder entry in order to have no effect unless
    # processed with app_bundle_utils.GenerateBundleApks().
    uncompressed_globs.append('classesX.dex')

  data = {
      'optimizations': {
          'splitsConfig': {
              'splitDimension': split_dimensions,
          },
          'uncompressNativeLibraries': {
              'enabled': True,
              'alignment': 'PAGE_ALIGNMENT_16K'
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

        zip_helpers.add_to_zip_hermetic(dst_zip,
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
  data = bundletool.RunBundleTool(
      ['dump', 'manifest', '--bundle', bundle_path, '--module', module_name])
  try:
    return ElementTree.fromstring(data)
  except ElementTree.ParseError:
    sys.stderr.write('Failed to parse:\n')
    sys.stderr.write(data)
    raise


def _GetComponentNames(manifest, tag_name):
  android_name = '{%s}name' % manifest_utils.ANDROID_NAMESPACE
  return [
      s.attrib.get(android_name)
      for s in manifest.iterfind(f'application/{tag_name}')
  ]


def _ClassesFromZip(module_zip):
  classes = set()
  for package in dexdump.Dump(module_zip):
    for java_package, package_dict in package.items():
      java_package += '.' if java_package else ''
      classes.update(java_package + c for c in package_dict['classes'])
  return classes


def _ValidateSplits(bundle_path, module_zips):
  logging.info('Reading manifests and running dexdump')
  base_zip = next(p for p in module_zips if os.path.basename(p) == 'base.zip')
  module_names = sorted(os.path.basename(p)[:-len('.zip')] for p in module_zips)
  # Using threads makes these step go from 7s -> 1s on my machine.
  with concurrent.futures.ThreadPoolExecutor() as executor:
    # Create list of classes from the base module's dex.
    classes_future = executor.submit(_ClassesFromZip, base_zip)

    # Create xmltrees of all module manifests.
    manifest_futures = [
        executor.submit(_GetManifestForModule, bundle_path, n)
        for n in module_names
    ]
    manifests_by_name = dict(
        zip(module_names, (f.result() for f in manifest_futures)))
    base_classes = classes_future.result()

  # Collect service names from all split manifests.
  logging.info('Performing checks')
  errors = []

  # Ensure there are no components defined in multiple splits.
  splits_by_component = {}
  for module_name, cur_manifest in manifests_by_name.items():
    for kind in _DEDUPE_ENTRY_TYPES:
      for component in _GetComponentNames(cur_manifest, kind):
        owner_module_name = splits_by_component.setdefault((kind, component),
                                                           module_name)
        # Allow services that exist only to keep <meta-data> out of
        # ApplicationInfo.
        if (owner_module_name != module_name
            and not component.endswith('HolderService')):
          errors.append(f'The {kind} "{component}" appeared in both '
                        f'{owner_module_name} and {module_name}.')

  # Ensure components defined in base manifest exist in base dex.
  for (kind, component), module_name in splits_by_component.items():
    if module_name == 'base' and kind in _COMPONENT_TYPES:
      if component not in base_classes:
        errors.append(f"{component} is defined in the base manfiest, "
                      f"but the class does not exist in the base splits' dex")

  # Remaining checks apply only when isolatedSplits="true".
  isolated_splits = manifests_by_name['base'].get(
      f'{manifest_utils.ANDROID_NAMESPACE}isolatedSplits')
  if isolated_splits != 'true':
    return errors

  # Ensure all providers are present in base module. We enforce this because
  # providers are loaded early in startup, and keeping them in the base module
  # gives more time for the chrome split to load.
  for module_name, cur_manifest in manifests_by_name.items():
    if module_name == 'base':
      continue
    provider_names = _GetComponentNames(cur_manifest, 'provider')
    if provider_names:
      errors.append('Providers should all be declared in the base manifest.'
                    ' "%s" module declared: %s' % (module_name, provider_names))

  # Ensure all services are present in base module because service classes are
  # not found if they are not present in the base module. b/169196314
  # It is fine if they are defined in split manifests though.
  for cur_manifest in manifests_by_name.values():
    for service_name in _GetComponentNames(cur_manifest, 'service'):
      if service_name not in base_classes:
        errors.append("Service %s should be present in the base module's dex."
                      " See b/169196314 for more details." % service_name)

  return errors


def main(args):
  build_utils.InitLogging('AAB_DEBUG')
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  split_dimensions = []
  if options.split_dimensions:
    split_dimensions = [x.upper() for x in options.split_dimensions]


  with build_utils.TempDir() as tmp_dir:
    logging.info('Splitting locale assets')
    module_zips = [
        _SplitModuleForAssetTargeting(module, tmp_dir, split_dimensions) \
        for module in options.module_zips]

    base_master_resource_ids = None
    if options.base_module_rtxt_path:
      logging.info('Creating R.txt allowlist')
      base_master_resource_ids = _GenerateBaseResourcesAllowList(
          options.base_module_rtxt_path, options.base_allowlist_rtxt_path)

    logging.info('Creating BundleConfig.pb.json')
    bundle_config = _GenerateBundleConfigJson(options.uncompressed_assets,
                                              options.compress_dex,
                                              split_dimensions,
                                              base_master_resource_ids)

    tmp_bundle = os.path.join(tmp_dir, 'tmp_bundle')

    # Important: bundletool requires that the bundle config file is
    # named with a .pb.json extension.
    tmp_bundle_config = tmp_bundle + '.BundleConfig.pb.json'

    with open(tmp_bundle_config, 'w') as f:
      f.write(bundle_config)

    logging.info('Running bundletool')
    cmd_args = build_utils.JavaCmd() + [
        '-jar',
        bundletool.BUNDLETOOL_JAR_PATH,
        'build-bundle',
        '--modules=' + ','.join(module_zips),
        '--output=' + tmp_bundle,
        '--config=' + tmp_bundle_config,
    ]

    if options.rotation_config:
      cmd_args += [
          f'--metadata-file={_ROTATION_METADATA_KEY}:{options.rotation_config}'
      ]

    build_utils.CheckOutput(
        cmd_args,
        print_stdout=True,
        print_stderr=True,
        stderr_filter=build_utils.FilterReflectiveAccessJavaWarnings,
        fail_on_output=options.warnings_as_errors)

    if options.validate_services:
      # TODO(crbug.com/40148088): This step takes 0.4s locally for bundles with
      # isolated splits disabled and 2s for bundles with isolated splits
      # enabled.  Consider making this run in parallel or move into a separate
      # step before enabling isolated splits by default.
      logging.info('Validating isolated split manifests')
      errors = _ValidateSplits(tmp_bundle, module_zips)
      if errors:
        sys.stderr.write('Bundle failed sanity checks:\n  ')
        sys.stderr.write('\n  '.join(errors))
        sys.stderr.write('\n')
        sys.exit(1)

    logging.info('Writing final output artifacts')
    shutil.move(tmp_bundle, options.out_bundle)

  if options.rtxt_out_path:
    _ConcatTextFiles(options.rtxt_in_paths, options.rtxt_out_path)

  if options.pathmap_out_path:
    _WriteBundlePathmap(options.pathmap_in_paths, options.module_names,
                        options.pathmap_out_path)


if __name__ == '__main__':
  main(sys.argv[1:])
