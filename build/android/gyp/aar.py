#!/usr/bin/env python3
#
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Processes an Android AAR file."""

import argparse
import os
import posixpath
import re
import shutil
import sys
from xml.etree import ElementTree
import zipfile

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import gn_helpers


_PROGUARD_TXT = 'proguard.txt'


def _GetManifestPackage(doc):
  """Returns the package specified in the manifest.

  Args:
    doc: an XML tree parsed by ElementTree

  Returns:
    String representing the package name.
  """
  return doc.attrib['package']


def _IsManifestEmpty(doc):
  """Decides whether the given manifest has merge-worthy elements.

  E.g.: <activity>, <service>, etc.

  Args:
    doc: an XML tree parsed by ElementTree

  Returns:
    Whether the manifest has merge-worthy elements.
  """
  for node in doc:
    if node.tag == 'application':
      if list(node):
        return False
    elif node.tag != 'uses-sdk':
      return False

  return True


def _CreateInfo(aar_file, resource_exclusion_globs):
  """Extracts and return .info data from an .aar file.

  Args:
    aar_file: Path to an input .aar file.
    resource_exclusion_globs: List of globs that exclude res/ files.

  Returns:
    A dict containing .info data.
  """
  data = {}
  data['aidl'] = []
  data['assets'] = []
  data['resources'] = []
  data['subjars'] = []
  data['subjar_tuples'] = []
  data['has_classes_jar'] = False
  data['has_proguard_flags'] = False
  data['has_native_libraries'] = False
  data['has_r_text_file'] = False
  with zipfile.ZipFile(aar_file) as z:
    manifest_xml = ElementTree.fromstring(z.read('AndroidManifest.xml'))
    data['is_manifest_empty'] = _IsManifestEmpty(manifest_xml)
    manifest_package = _GetManifestPackage(manifest_xml)
    if manifest_package:
      data['manifest_package'] = manifest_package

    for name in z.namelist():
      if name.endswith('/'):
        continue
      if name.startswith('aidl/'):
        data['aidl'].append(name)
      elif name.startswith('res/'):
        if not build_utils.MatchesGlob(name, resource_exclusion_globs):
          data['resources'].append(name)
      elif name.startswith('libs/') and name.endswith('.jar'):
        label = posixpath.basename(name)[:-4]
        label = re.sub(r'[^a-zA-Z0-9._]', '_', label)
        data['subjars'].append(name)
        data['subjar_tuples'].append([label, name])
      elif name.startswith('assets/'):
        data['assets'].append(name)
      elif name.startswith('jni/'):
        data['has_native_libraries'] = True
        if 'native_libraries' in data:
          data['native_libraries'].append(name)
        else:
          data['native_libraries'] = [name]
      elif name == 'classes.jar':
        data['has_classes_jar'] = True
      elif name == _PROGUARD_TXT:
        data['has_proguard_flags'] = True
      elif name == 'R.txt':
        # Some AARs, e.g. gvr_controller_java, have empty R.txt. Such AARs
        # have no resources as well. We treat empty R.txt as having no R.txt.
        data['has_r_text_file'] = bool(z.read('R.txt').strip())

  return data


def _PerformExtract(aar_file, output_dir, name_allowlist):
  with build_utils.TempDir() as tmp_dir:
    tmp_dir = os.path.join(tmp_dir, 'staging')
    os.mkdir(tmp_dir)
    build_utils.ExtractAll(
        aar_file, path=tmp_dir, predicate=name_allowlist.__contains__)
    # Write a breadcrumb so that SuperSize can attribute files back to the .aar.
    with open(os.path.join(tmp_dir, 'source.info'), 'w') as f:
      f.write('source={}\n'.format(aar_file))

    shutil.rmtree(output_dir, ignore_errors=True)
    shutil.move(tmp_dir, output_dir)


def _AddCommonArgs(parser):
  parser.add_argument(
      'aar_file', help='Path to the AAR file.', type=os.path.normpath)
  parser.add_argument('--ignore-resources',
                      action='store_true',
                      help='Whether to skip extraction of res/')
  parser.add_argument('--resource-exclusion-globs',
                      help='GN list of globs for res/ files to ignore')


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  command_parsers = parser.add_subparsers(dest='command')
  subp = command_parsers.add_parser(
      'list', help='Output a GN scope describing the contents of the .aar.')
  _AddCommonArgs(subp)
  subp.add_argument('--output', help='Output file.', default='-')

  subp = command_parsers.add_parser('extract', help='Extracts the .aar')
  _AddCommonArgs(subp)
  subp.add_argument(
      '--output-dir',
      help='Output directory for the extracted files.',
      required=True,
      type=os.path.normpath)
  subp.add_argument(
      '--assert-info-file',
      help='Path to .info file. Asserts that it matches what '
      '"list" would output.',
      type=argparse.FileType('r'))

  args = parser.parse_args()

  args.resource_exclusion_globs = action_helpers.parse_gn_list(
      args.resource_exclusion_globs)
  if args.ignore_resources:
    args.resource_exclusion_globs.append('res/*')

  aar_info = _CreateInfo(args.aar_file, args.resource_exclusion_globs)
  formatted_info = """\
# Generated by //build/android/gyp/aar.py
# To regenerate, use "update_android_aar_prebuilts = true" and run "gn gen".

""" + gn_helpers.ToGNString(aar_info, pretty=True)

  if args.command == 'extract':
    if args.assert_info_file:
      cached_info = args.assert_info_file.read()
      if formatted_info != cached_info:
        raise Exception('android_aar_prebuilt() cached .info file is '
                        'out-of-date. Run gn gen with '
                        'update_android_aar_prebuilts=true to update it.')

    # Extract all files except for filtered res/ files.
    with zipfile.ZipFile(args.aar_file) as zf:
      names = {n for n in zf.namelist() if not n.startswith('res/')}
    names.update(aar_info['resources'])

    _PerformExtract(args.aar_file, args.output_dir, names)

  elif args.command == 'list':
    aar_output_present = args.output != '-' and os.path.isfile(args.output)
    if aar_output_present:
      # Some .info files are read-only, for examples the cipd-controlled ones
      # under third_party/android_deps/repository. To deal with these, first
      # that its content is correct, and if it is, exit without touching
      # the file system.
      file_info = open(args.output, 'r').read()
      if file_info == formatted_info:
        return

    # Try to write the file. This may fail for read-only ones that were
    # not updated.
    try:
      with open(args.output, 'w') as f:
        f.write(formatted_info)
    except IOError as e:
      if not aar_output_present:
        raise e
      raise Exception('Could not update output file: %s\n' % args.output) from e


if __name__ == '__main__':
  sys.exit(main())
