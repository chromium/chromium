#!/usr/bin/env python3
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adds the code parts to a resource APK."""

import argparse
import logging
import os
import shutil
import sys
import tempfile
import zipfile
import zlib

import finalize_apk

from util import build_utils
from util import diff_utils
from util import zipalign

# Input dex.jar files are zipaligned.
zipalign.ApplyZipFileZipAlignFix()


# Taken from aapt's Package.cpp:
_NO_COMPRESS_EXTENSIONS = ('.jpg', '.jpeg', '.png', '.gif', '.wav', '.mp2',
                           '.mp3', '.ogg', '.aac', '.mpg', '.mpeg', '.mid',
                           '.midi', '.smf', '.jet', '.rtttl', '.imy', '.xmf',
                           '.mp4', '.m4a', '.m4v', '.3gp', '.3gpp', '.3g2',
                           '.3gpp2', '.amr', '.awb', '.wma', '.wmv', '.webm')


def _ParseArgs(args):
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument(
      '--assets',
      help='GYP-list of files to add as assets in the form '
      '"srcPath:zipPath", where ":zipPath" is optional.')
  parser.add_argument(
      '--java-resources', help='GYP-list of java_resources JARs to include.')
  parser.add_argument('--write-asset-list',
                      action='store_true',
                      help='Whether to create an assets/assets_list file.')
  parser.add_argument(
      '--uncompressed-assets',
      help='Same as --assets, except disables compression.')
  parser.add_argument('--resource-apk',
                      help='An .ap_ file built using aapt',
                      required=True)
  parser.add_argument('--output-apk',
                      help='Path to the output file',
                      required=True)
  parser.add_argument('--format', choices=['apk', 'bundle-module'],
                      default='apk', help='Specify output format.')
  parser.add_argument('--dex-file',
                      help='Path to the classes.dex to use')
  parser.add_argument(
      '--jdk-libs-dex-file',
      help='Path to classes.dex created by dex_jdk_libs.py')
  parser.add_argument('--uncompress-dex', action='store_true',
                      help='Store .dex files uncompressed in the APK')
  parser.add_argument('--native-libs',
                      action='append',
                      help='GYP-list of native libraries to include. '
                           'Can be specified multiple times.',
                      default=[])
  parser.add_argument('--secondary-native-libs',
                      action='append',
                      help='GYP-list of native libraries for secondary '
                           'android-abi. Can be specified multiple times.',
                      default=[])
  parser.add_argument('--android-abi',
                      help='Android architecture to use for native libraries')
  parser.add_argument('--secondary-android-abi',
                      help='The secondary Android architecture to use for'
                           'secondary native libraries')
  parser.add_argument(
      '--is-multi-abi',
      action='store_true',
      help='Will add a placeholder for the missing ABI if no native libs or '
      'placeholders are set for either the primary or secondary ABI. Can only '
      'be set if both --android-abi and --secondary-android-abi are set.')
  parser.add_argument(
      '--native-lib-placeholders',
      help='GYP-list of native library placeholders to add.')
  parser.add_argument(
      '--secondary-native-lib-placeholders',
      help='GYP-list of native library placeholders to add '
      'for the secondary ABI')
  parser.add_argument('--uncompress-shared-libraries', default='False',
      choices=['true', 'True', 'false', 'False'],
      help='Whether to uncompress native shared libraries. Argument must be '
           'a boolean value.')
  parser.add_argument(
      '--apksigner-jar', help='Path to the apksigner executable.')
  parser.add_argument('--zipalign-path',
                      help='Path to the zipalign executable.')
  parser.add_argument('--key-path',
                      help='Path to keystore for signing.')
  parser.add_argument('--key-passwd',
                      help='Keystore password')
  parser.add_argument('--key-name',
                      help='Keystore name')
  parser.add_argument(
      '--min-sdk-version', required=True, help='Value of APK\'s minSdkVersion')
  parser.add_argument(
      '--best-compression',
      action='store_true',
      help='Use zip -9 rather than zip -1')
  parser.add_argument(
      '--library-always-compress',
      action='append',
      help='The list of library files that we always compress.')
  parser.add_argument(
      '--library-renames',
      action='append',
      help='The list of library files that we prepend crazy. to their names.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  diff_utils.AddCommandLineFlags(parser)
  options = parser.parse_args(args)
  options.assets = build_utils.ParseGnList(options.assets)
  options.uncompressed_assets = build_utils.ParseGnList(
      options.uncompressed_assets)
  options.native_lib_placeholders = build_utils.ParseGnList(
      options.native_lib_placeholders)
  options.secondary_native_lib_placeholders = build_utils.ParseGnList(
      options.secondary_native_lib_placeholders)
  options.java_resources = build_utils.ParseGnList(options.java_resources)
  options.native_libs = build_utils.ParseGnList(options.native_libs)
  options.secondary_native_libs = build_utils.ParseGnList(
      options.secondary_native_libs)
  options.library_always_compress = build_utils.ParseGnList(
      options.library_always_compress)
  options.library_renames = build_utils.ParseGnList(options.library_renames)

  # --apksigner-jar, --zipalign-path, --key-xxx arguments are
  # required when building an APK, but not a bundle module.
  if options.format == 'apk':
    required_args = [
        'apksigner_jar', 'zipalign_path', 'key_path', 'key_passwd', 'key_name'
    ]
    for required in required_args:
      if not vars(options)[required]:
        raise Exception('Argument --%s is required for APKs.' % (
            required.replace('_', '-')))

  options.uncompress_shared_libraries = \
      options.uncompress_shared_libraries in [ 'true', 'True' ]

  if not options.android_abi and (options.native_libs or
                                  options.native_lib_placeholders):
    raise Exception('Must specify --android-abi with --native-libs')
  if not options.secondary_android_abi and (options.secondary_native_libs or
      options.secondary_native_lib_placeholders):
    raise Exception('Must specify --secondary-android-abi with'
                    ' --secondary-native-libs')
  if options.is_multi_abi and not (options.android_abi
                                   and options.secondary_android_abi):
    raise Exception('Must specify --is-multi-abi with both --android-abi '
                    'and --secondary-android-abi.')
  return options


def _SplitAssetPath(path):
  """Returns (src, dest) given an asset path in the form src[:dest]."""
  path_parts = path.split(':')
  src_path = path_parts[0]
  if len(path_parts) > 1:
    dest_path = path_parts[1]
  else:
    dest_path = os.path.basename(src_path)
  return src_path, dest_path


def _ExpandPaths(paths):
  """Converts src:dst into tuples and enumerates files within directories.

  Args:
    paths: Paths in the form "src_path:dest_path"

  Returns:
    A list of (src_path, dest_path) tuples sorted by dest_path (for stable
    ordering within output .apk).
  """
  ret = []
  for path in paths:
    src_path, dest_path = _SplitAssetPath(path)
    if os.path.isdir(src_path):
      for f in build_utils.FindInDirectory(src_path, '*'):
        ret.append((f, os.path.join(dest_path, f[len(src_path) + 1:])))
    else:
      ret.append((src_path, dest_path))
  ret.sort(key=lambda t:t[1])
  return ret


def _GetAssetsToAdd(path_tuples,
                    fast_align,
                    disable_compression=False,
                    allow_reads=True):
  """Returns the list of file_detail tuples for assets in the apk.

  Args:
    path_tuples: List of src_path, dest_path tuples to add.
    fast_align: Whether to perform alignment in python zipfile (alternatively
                alignment can be done using the zipalign utility out of band).
    disable_compression: Whether to disable compression.
    allow_reads: If false, we do not try to read the files from disk (to find
                 their size for example).

  Returns: A list of (src_path, apk_path, compress, alignment) tuple
  representing what and how assets are added.
  """
  assets_to_add = []

  # Group all uncompressed assets together in the hope that it will increase
  # locality of mmap'ed files.
  for target_compress in (False, True):
    for src_path, dest_path in path_tuples:
      compress = not disable_compression and (
          os.path.splitext(src_path)[1] not in _NO_COMPRESS_EXTENSIONS)

      if target_compress == compress:
        # AddToZipHermetic() uses this logic to avoid growing small files.
        # We need it here in order to set alignment correctly.
        if allow_reads and compress and os.path.getsize(src_path) < 16:
          compress = False

        apk_path = 'assets/' + dest_path
        alignment = 0 if compress and not fast_align else 4
        assets_to_add.append((apk_path, src_path, compress, alignment))
  return assets_to_add


def _AddFiles(apk, details):
  """Adds files to the apk.

  Args:
    apk: path to APK to add to.
    details: A list of file detail tuples (src_path, apk_path, compress,
    alignment) representing what and how files are added to the APK.
  """
  for apk_path, src_path, compress, alignment in details:
    # This check is only relevant for assets, but it should not matter if it is
    # checked for the whole list of files.
    try:
      apk.getinfo(apk_path)
      # Should never happen since write_build_config.py handles merging.
      raise Exception(
          'Multiple targets specified the asset path: %s' % apk_path)
    except KeyError:
      zipalign.AddToZipHermetic(
          apk,
          apk_path,
          src_path=src_path,
          compress=compress,
          alignment=alignment)


def _GetNativeLibrariesToAdd(native_libs, android_abi, uncompress, fast_align,
                             lib_always_compress, lib_renames):
  """Returns the list of file_detail tuples for native libraries in the apk.

  Returns: A list of (src_path, apk_path, compress, alignment) tuple
  representing what and how native libraries are added.
  """
  libraries_to_add = []


  for path in native_libs:
    basename = os.path.basename(path)
    compress = not uncompress or any(lib_name in basename
                                     for lib_name in lib_always_compress)
    rename = any(lib_name in basename for lib_name in lib_renames)
    if rename:
      basename = 'crazy.' + basename

    lib_android_abi = android_abi
    if path.startswith('android_clang_arm64_hwasan/'):
      lib_android_abi = 'arm64-v8a-hwasan'

    apk_path = 'lib/%s/%s' % (lib_android_abi, basename)
    alignment = 0 if compress and not fast_align else 0x1000
    libraries_to_add.append((apk_path, path, compress, alignment))

  return libraries_to_add


def _CreateExpectationsData(native_libs, assets):
  """Creates list of native libraries and assets."""
  native_libs = sorted(native_libs)
  assets = sorted(assets)

  ret = []
  for apk_path, _, compress, alignment in native_libs + assets:
    ret.append('apk_path=%s, compress=%s, alignment=%s\n' %
               (apk_path, compress, alignment))
  return ''.join(ret)


def main(args):
  build_utils.InitLogging('APKBUILDER_DEBUG')
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  # Until Python 3.7, there's no better way to set compression level.
  # The default is 6.
  if options.best_compression:
    # Compresses about twice as slow as the default.
    zlib.Z_DEFAULT_COMPRESSION = 9
  else:
    # Compresses about twice as fast as the default.
    zlib.Z_DEFAULT_COMPRESSION = 1

  # Manually align only when alignment is necessary.
  # Python's zip implementation duplicates file comments in the central
  # directory, whereas zipalign does not, so use zipalign for official builds.
  fast_align = options.format == 'apk' and not options.best_compression

  native_libs = sorted(options.native_libs)

  # Include native libs in the depfile_deps since GN doesn't know about the
  # dependencies when is_component_build=true.
  depfile_deps = list(native_libs)

  # For targets that depend on static library APKs, dex paths are created by
  # the static library's dexsplitter target and GN doesn't know about these
  # paths.
  if options.dex_file:
    depfile_deps.append(options.dex_file)

  secondary_native_libs = []
  if options.secondary_native_libs:
    secondary_native_libs = sorted(options.secondary_native_libs)
    depfile_deps += secondary_native_libs

  if options.java_resources:
    # Included via .build_config.json, so need to write it to depfile.
    depfile_deps.extend(options.java_resources)

  assets = _ExpandPaths(options.assets)
  uncompressed_assets = _ExpandPaths(options.uncompressed_assets)

  # Included via .build_config.json, so need to write it to depfile.
  depfile_deps.extend(x[0] for x in assets)
  depfile_deps.extend(x[0] for x in uncompressed_assets)
  depfile_deps.append(options.resource_apk)

  # Bundle modules have a structure similar to APKs, except that resources
  # are compiled in protobuf format (instead of binary xml), and that some
  # files are located into different top-level directories, e.g.:
  #  AndroidManifest.xml -> manifest/AndroidManifest.xml
  #  classes.dex -> dex/classes.dex
  #  res/ -> res/  (unchanged)
  #  assets/ -> assets/  (unchanged)
  #  <other-file> -> root/<other-file>
  #
  # Hence, the following variables are used to control the location of files in
  # the final archive.
  if options.format == 'bundle-module':
    apk_manifest_dir = 'manifest/'
    apk_root_dir = 'root/'
    apk_dex_dir = 'dex/'
  else:
    apk_manifest_dir = ''
    apk_root_dir = ''
    apk_dex_dir = ''

  def _GetAssetDetails(assets, uncompressed_assets, fast_align, allow_reads):
    ret = _GetAssetsToAdd(assets,
                          fast_align,
                          disable_compression=False,
                          allow_reads=allow_reads)
    ret.extend(
        _GetAssetsToAdd(uncompressed_assets,
                        fast_align,
                        disable_compression=True,
                        allow_reads=allow_reads))
    return ret

  libs_to_add = _GetNativeLibrariesToAdd(
      native_libs, options.android_abi, options.uncompress_shared_libraries,
      fast_align, options.library_always_compress, options.library_renames)
  if options.secondary_android_abi:
    libs_to_add.extend(
        _GetNativeLibrariesToAdd(
            secondary_native_libs, options.secondary_android_abi,
            options.uncompress_shared_libraries, fast_align,
            options.library_always_compress, options.library_renames))

  if options.expected_file:
    # We compute expectations without reading the files. This allows us to check
    # expectations for different targets by just generating their build_configs
    # and not have to first generate all the actual files and all their
    # dependencies (for example by just passing --only-verify-expectations).
    asset_details = _GetAssetDetails(assets,
                                     uncompressed_assets,
                                     fast_align,
                                     allow_reads=False)

    actual_data = _CreateExpectationsData(libs_to_add, asset_details)
    diff_utils.CheckExpectations(actual_data, options)

    if options.only_verify_expectations:
      if options.depfile:
        build_utils.WriteDepfile(options.depfile,
                                 options.actual_file,
                                 inputs=depfile_deps)
      return

  # If we are past this point, we are going to actually create the final apk so
  # we should recompute asset details again but maybe perform some optimizations
  # based on the size of the files on disk.
  assets_to_add = _GetAssetDetails(
      assets, uncompressed_assets, fast_align, allow_reads=True)

  # Targets generally do not depend on apks, so no need for only_if_changed.
  with build_utils.AtomicOutput(options.output_apk, only_if_changed=False) as f:
    with zipfile.ZipFile(options.resource_apk) as resource_apk, \
         zipfile.ZipFile(f, 'w') as out_apk:

      def add_to_zip(zip_path, data, compress=True, alignment=4):
        zipalign.AddToZipHermetic(
            out_apk,
            zip_path,
            data=data,
            compress=compress,
            alignment=0 if compress and not fast_align else alignment)

      def copy_resource(zipinfo, out_dir=''):
        add_to_zip(
            out_dir + zipinfo.filename,
            resource_apk.read(zipinfo.filename),
            compress=zipinfo.compress_type != zipfile.ZIP_STORED)

      # Make assets come before resources in order to maintain the same file
      # ordering as GYP / aapt. http://crbug.com/561862
      resource_infos = resource_apk.infolist()

      # 1. AndroidManifest.xml
      logging.debug('Adding AndroidManifest.xml')
      copy_resource(
          resource_apk.getinfo('AndroidManifest.xml'), out_dir=apk_manifest_dir)

      # 2. Assets
      logging.debug('Adding assets/')
      _AddFiles(out_apk, assets_to_add)

      # 3. Dex files
      logging.debug('Adding classes.dex')
      if options.dex_file:
        with open(options.dex_file, 'rb') as dex_file_obj:
          if options.dex_file.endswith('.dex'):
            max_dex_number = 1
            # This is the case for incremental_install=true.
            add_to_zip(
                apk_dex_dir + 'classes.dex',
                dex_file_obj.read(),
                compress=not options.uncompress_dex)
          else:
            max_dex_number = 0
            with zipfile.ZipFile(dex_file_obj) as dex_zip:
              for dex in (d for d in dex_zip.namelist() if d.endswith('.dex')):
                max_dex_number += 1
                add_to_zip(
                    apk_dex_dir + dex,
                    dex_zip.read(dex),
                    compress=not options.uncompress_dex)

      if options.jdk_libs_dex_file:
        with open(options.jdk_libs_dex_file, 'rb') as dex_file_obj:
          add_to_zip(
              apk_dex_dir + 'classes{}.dex'.format(max_dex_number + 1),
              dex_file_obj.read(),
              compress=not options.uncompress_dex)

      # 4. Native libraries.
      logging.debug('Adding lib/')
      _AddFiles(out_apk, libs_to_add)

      # Add a placeholder lib if the APK should be multi ABI but is missing libs
      # for one of the ABIs.
      native_lib_placeholders = options.native_lib_placeholders
      secondary_native_lib_placeholders = (
          options.secondary_native_lib_placeholders)
      if options.is_multi_abi:
        if ((secondary_native_libs or secondary_native_lib_placeholders)
            and not native_libs and not native_lib_placeholders):
          native_lib_placeholders += ['libplaceholder.so']
        if ((native_libs or native_lib_placeholders)
            and not secondary_native_libs
            and not secondary_native_lib_placeholders):
          secondary_native_lib_placeholders += ['libplaceholder.so']

      # Add placeholder libs.
      for name in sorted(native_lib_placeholders):
        # Note: Empty libs files are ignored by md5check (can cause issues
        # with stale builds when the only change is adding/removing
        # placeholders).
        apk_path = 'lib/%s/%s' % (options.android_abi, name)
        add_to_zip(apk_path, '', alignment=0x1000)

      for name in sorted(secondary_native_lib_placeholders):
        # Note: Empty libs files are ignored by md5check (can cause issues
        # with stale builds when the only change is adding/removing
        # placeholders).
        apk_path = 'lib/%s/%s' % (options.secondary_android_abi, name)
        add_to_zip(apk_path, '', alignment=0x1000)

      # 5. Resources
      logging.debug('Adding res/')
      for info in sorted(resource_infos, key=lambda i: i.filename):
        if info.filename != 'AndroidManifest.xml':
          copy_resource(info)

      # 6. Java resources that should be accessible via
      # Class.getResourceAsStream(), in particular parts of Emma jar.
      # Prebuilt jars may contain class files which we shouldn't include.
      logging.debug('Adding Java resources')
      for java_resource in options.java_resources:
        with zipfile.ZipFile(java_resource, 'r') as java_resource_jar:
          for apk_path in sorted(java_resource_jar.namelist()):
            apk_path_lower = apk_path.lower()

            if apk_path_lower.startswith('meta-inf/'):
              continue
            if apk_path_lower.endswith('/'):
              continue
            if apk_path_lower.endswith('.class'):
              continue

            add_to_zip(apk_root_dir + apk_path,
                       java_resource_jar.read(apk_path))

    if options.format == 'apk':
      zipalign_path = None if fast_align else options.zipalign_path
      finalize_apk.FinalizeApk(options.apksigner_jar,
                               zipalign_path,
                               f.name,
                               f.name,
                               options.key_path,
                               options.key_passwd,
                               options.key_name,
                               int(options.min_sdk_version),
                               warnings_as_errors=options.warnings_as_errors)
    logging.debug('Moving file into place')

    if options.depfile:
      build_utils.WriteDepfile(options.depfile,
                               options.output_apk,
                               inputs=depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])
