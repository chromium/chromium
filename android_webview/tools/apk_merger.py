#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Merges a 32-bit APK into a 32/64-bit APK.

This script is used to make the 32-bit parts of a pure 32-bit APK identical to
those of an APK built on a 64-bit configuration (whether adding 32-bit parts to
a pure 64-bit build, or replacing 32-bit parts in a multi-architecture build to
ensurce consistency with pure 32-bit builds).

In an ideal world, the libraries and assets in a pure 32-bit APK would be
identical to the 32-bit equivalents in a 64-bit-generated APK (with secondary
ABI). However, this isn't reality. For example, subtle differences due to paths
yield different native libraries (a benign difference). Accidental differences
in build configuration yield legitimate differences (impacting functionality and
binary size). This script overwrites parts of the 64-bit APK with pieces from
the 32-bit APK, so that the two versions are identical from a 32-bit
perspective.

Longer term, the 64-bit build configuration should be updated to generate pure
32-bit APKs. While slightly counter-intuitive, this would ensure that 32-bit
pieces are identical across the different versions without having to merge
anything.

To use this script, you need to
1. Build 32-bit APK as usual.
2. Build 64-bit APK with GN variable build_apk_secondary_abi=false OR true.
3. Use this script to merge the 2 APKs.

"""

import argparse
import collections
import filecmp
import logging
import os
import pprint
import shutil
import sys
import tempfile
import zipfile

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..')
SRC_DIR = os.path.abspath(SRC_DIR)
BUILD_ANDROID_DIR = os.path.join(SRC_DIR, 'build', 'android')
BUILD_ANDROID_GYP_DIR = os.path.join(BUILD_ANDROID_DIR, 'gyp')
sys.path.append(BUILD_ANDROID_GYP_DIR)

import finalize_apk # pylint: disable=import-error,wrong-import-position
from util import build_utils # pylint: disable=import-error,wrong-import-position

sys.path.append(BUILD_ANDROID_DIR)

from pylib import constants  # pylint: disable=import-error,wrong-import-position

DEFAULT_ZIPALIGN_PATH = os.path.join(
    SRC_DIR, 'third_party', 'android_sdk', 'public', 'build-tools',
    constants.ANDROID_SDK_BUILD_TOOLS_VERSION, 'zipalign')


class ApkMergeFailure(Exception):
  pass


def UnpackApk(file_name, dst, ignore_paths=()):
  zippy = zipfile.ZipFile(file_name)
  files_to_extract = [f for f in zippy.namelist() if f not in ignore_paths]
  zippy.extractall(dst, files_to_extract)


def GetNonDirFiles(top, base_dir):
  """ Return a list containing all (non-directory) files in tree with top as
  root.

  Each file is represented by the relative path from base_dir to that file.
  If top is a file (not a directory) then a list containing only top is
  returned.
  """
  if os.path.isdir(top):
    ret = []
    for dirpath, _, filenames in os.walk(top):
      for filename in filenames:
        ret.append(
            os.path.relpath(os.path.join(dirpath, filename), base_dir))
    return ret
  else:
    return [os.path.relpath(top, base_dir)]


def GetDiffFiles(dcmp, base_dir):
  """ Return the list of files contained only in the right directory of dcmp.

  The files returned are represented by relative paths from base_dir.
  """
  copy_files = []
  for file_name in dcmp.right_only:
    copy_files.extend(
        GetNonDirFiles(os.path.join(dcmp.right, file_name), base_dir))

  # we cannot merge APKs with files with similar names but different contents
  if len(dcmp.diff_files) > 0:
    raise ApkMergeFailure('found differing files: %s in %s and %s' %
                          (dcmp.diff_files, dcmp.left, dcmp.right))

  if len(dcmp.funny_files) > 0:
    raise ApkMergeFailure('found uncomparable files: %s in %s and %s' %
                          (dcmp.funny_files, dcmp.left, dcmp.right))

  for sub_dcmp in dcmp.subdirs.itervalues():
    copy_files.extend(GetDiffFiles(sub_dcmp, base_dir))
  return copy_files


def CheckFilesExpected(actual_files, expected_files):
  """ Check that the lists of actual and expected files are the same. """
  actual_file_names = collections.defaultdict(int)
  for f in actual_files:
    actual_file_names[f] += 1
  actual_file_set = set(actual_file_names.iterkeys())
  expected_file_set = set(expected_files)

  unexpected_file_set = actual_file_set.difference(expected_file_set)
  missing_file_set = expected_file_set.difference(actual_file_set)
  duplicate_file_set = set(
      f for f, n in actual_file_names.iteritems() if n > 1)

  errors = []
  if unexpected_file_set:
    errors.append(
        '  unexpected files: %s' % pprint.pformat(unexpected_file_set))
  if missing_file_set:
    errors.append('  missing files: %s' % pprint.pformat(missing_file_set))
  if duplicate_file_set:
    errors.append('  duplicate files: %s' % pprint.pformat(duplicate_file_set))

  if errors:
    raise ApkMergeFailure(
        "Files don't match expectations:\n%s" % '\n'.join(errors))


def AddDiffFiles(diff_files, tmp_dir_32, out_zip, uncompress_shared_libraries):
  """ Insert files only present in 32-bit APK into 64-bit APK (tmp_apk). """
  for diff_file in diff_files:
    compress = not uncompress_shared_libraries and diff_file.endswith('.so')
    build_utils.AddToZipHermetic(out_zip,
                                 diff_file,
                                 os.path.join(tmp_dir_32, diff_file),
                                 compress=compress)


def MergeApk(args, tmp_apk, tmp_dir_32, tmp_dir_64):
  # expected_files is the set of 32-bit related files that we expect to differ
  # between a 32- and 64-bit build. Hence, they will be skipped when seeding the
  # generated APK with the original 64-bit version, and explicitly copied in
  # from the 32-bit version.
  expected_files = []

  assets_path = 'base/assets' if args.bundle else 'assets'
  expected_files.append('%s/snapshot_blob_32.bin' % assets_path)

  if args.has_unwind_cfi:
    expected_files.append('%s/unwind_cfi_32' % assets_path)

  # All native libraries are assumed to differ, and will be merged.
  with zipfile.ZipFile(args.apk_32bit) as z:
    expected_files.extend([p for p in z.namelist() if p.endswith('.so')])

  UnpackApk(args.apk_32bit, tmp_dir_32)
  UnpackApk(args.apk_64bit, tmp_dir_64, expected_files)

  # These are files that we know will be different, and we will hence ignore in
  # the file comparison.
  ignores = ['META-INF', 'AndroidManifest.xml']
  if args.ignore_classes_dex:
    ignores += ['classes.dex', 'classes2.dex', 'classes3.dex']
  if args.debug:
    # see http://crbug.com/648720
    ignores += ['webview_licenses.notice']
  if args.bundle:
    # if merging a bundle we must ignore the bundle specific
    # proto files as they will always be different.
    ignores += ['BundleConfig.pb', 'native.pb']

  dcmp = filecmp.dircmp(
      tmp_dir_64,
      tmp_dir_32,
      ignore=ignores)

  diff_files = GetDiffFiles(dcmp, tmp_dir_32)

  # Check that diff_files match exactly those files we want to insert into
  # the 64-bit APK.
  CheckFilesExpected(diff_files, expected_files)

  with zipfile.ZipFile(tmp_apk, 'w') as out_zip:
    exclude_patterns = ['META-INF/*'] + expected_files

    # Build the initial merged APK from the 64-bit APK, excluding all files we
    # will pull from the 32-bit APK.
    path_transform = (
        lambda p: None if build_utils.MatchesGlob(p, exclude_patterns) else p)
    build_utils.MergeZips(
        out_zip, [args.apk_64bit], path_transform=path_transform)

    # Add the files from the 32-bit APK.
    AddDiffFiles(diff_files, tmp_dir_32, out_zip,
                 args.uncompress_shared_libraries)


def main():
  parser = argparse.ArgumentParser(
      description='Merge a 32-bit APK into a 64-bit APK')
  # Using type=os.path.abspath converts file paths to absolute paths so that
  # we can change working directory without affecting these paths
  parser.add_argument('--apk_32bit', required=True, type=os.path.abspath)
  parser.add_argument('--apk_64bit', required=True, type=os.path.abspath)
  parser.add_argument('--out_apk', required=True, type=os.path.abspath)
  parser.add_argument('--zipalign_path', type=os.path.abspath)
  parser.add_argument('--keystore_path', required=True, type=os.path.abspath)
  parser.add_argument('--key_name', required=True)
  parser.add_argument('--key_password', required=True)
  parser.add_argument('--uncompress-shared-libraries', action='store_true')
  parser.add_argument('--bundle', action='store_true')
  parser.add_argument('--debug', action='store_true')
  # This option shall only used in debug build, see http://crbug.com/631494.
  parser.add_argument('--ignore-classes-dex', action='store_true')
  parser.add_argument('--has-unwind-cfi', action='store_true',
                      help='Specifies if the 32-bit apk has unwind_cfi file')
  args = parser.parse_args()

  if (args.zipalign_path is not None and
      not os.path.isfile(args.zipalign_path)):
    # If given an invalid path, fall back to try the default.
    logging.warning('zipalign path not found: %s', args.zipalign_path)
    logging.warning('falling back to: %s', DEFAULT_ZIPALIGN_PATH)
    args.zipalign_path = None

  if args.zipalign_path is None:
    # When no path given, try the default.
    if not os.path.isfile(DEFAULT_ZIPALIGN_PATH):
      return 'ERROR: zipalign path not found: %s' % DEFAULT_ZIPALIGN_PATH
    args.zipalign_path = DEFAULT_ZIPALIGN_PATH

  tmp_dir = tempfile.mkdtemp()
  tmp_dir_64 = os.path.join(tmp_dir, '64_bit')
  tmp_dir_32 = os.path.join(tmp_dir, '32_bit')
  tmp_apk = os.path.join(tmp_dir, 'tmp.apk')
  new_apk = args.out_apk

  try:
    MergeApk(args, tmp_apk, tmp_dir_32, tmp_dir_64)

    apksigner_path = os.path.join(
        os.path.dirname(args.zipalign_path), 'apksigner')
    finalize_apk.FinalizeApk(apksigner_path, args.zipalign_path,
                             tmp_apk, new_apk, args.keystore_path,
                             args.key_password, args.key_name)
  finally:
    shutil.rmtree(tmp_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
