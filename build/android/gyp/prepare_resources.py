#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process Android resource directories to generate .resources.zip and R.txt
files."""

import argparse
import collections
import os
import re
import shutil
import sys
import zipfile

from util import build_utils
from util import jar_info_utils
from util import resources_parser
from util import resource_utils


def _ParseArgs(args):
  """Parses command line options.

  Returns:
    An options object as from argparse.ArgumentParser.parse_args()
  """
  parser = argparse.ArgumentParser(description=__doc__)
  build_utils.AddDepfileOption(parser)

  parser.add_argument('--res-sources-path',
                      required=True,
                      help='Path to a list of input resources for this target.')

  parser.add_argument(
      '--r-text-in',
      help='Path to pre-existing R.txt. Its resource IDs override those found '
      'in the generated R.txt when generating R.java.')

  parser.add_argument(
      '--shared-resources',
      action='store_true',
      help='Make resources shareable by generating an onResourcesLoaded() '
      'method in the R.java source file.')

  parser.add_argument(
      '--resource-zip-out',
      help='Path to a zip archive containing all resources from '
      '--resource-dirs, merged into a single directory tree.')

  parser.add_argument('--r-text-out',
                      help='Path to store the generated R.txt file.')

  parser.add_argument('--strip-drawables',
                      action="store_true",
                      help='Remove drawables from the resources.')

  options = parser.parse_args(args)

  with open(options.res_sources_path) as f:
    options.sources = f.read().splitlines()
  options.resource_dirs = resource_utils.DeduceResourceDirsFromFileList(
      options.sources)

  return options


def _CheckAllFilesListed(resource_files, resource_dirs):
  resource_files = set(resource_files)
  missing_files = []
  for path, _ in resource_utils.IterResourceFilesInDirectories(resource_dirs):
    if path not in resource_files:
      missing_files.append(path)

  if missing_files:
    sys.stderr.write('Error: Found files not listed in the sources list of '
                     'the BUILD.gn target:\n')
    for path in missing_files:
      sys.stderr.write('{}\n'.format(path))
    sys.exit(1)


def _ZipResources(resource_dirs, zip_path, ignore_pattern):
  # ignore_pattern is a string of ':' delimited list of globs used to ignore
  # files that should not be part of the final resource zip.
  files_to_zip = []
  path_info = resource_utils.ResourceInfoFile()
  for index, resource_dir in enumerate(resource_dirs):
    attributed_aar = None
    if not resource_dir.startswith('..'):
      aar_source_info_path = os.path.join(
          os.path.dirname(resource_dir), 'source.info')
      if os.path.exists(aar_source_info_path):
        attributed_aar = jar_info_utils.ReadAarSourceInfo(aar_source_info_path)

    for path, archive_path in resource_utils.IterResourceFilesInDirectories(
        [resource_dir], ignore_pattern):
      attributed_path = path
      if attributed_aar:
        attributed_path = os.path.join(attributed_aar, 'res',
                                       path[len(resource_dir) + 1:])
      # Use the non-prefixed archive_path in the .info file.
      path_info.AddMapping(archive_path, attributed_path)

      resource_dir_name = os.path.basename(resource_dir)
      archive_path = '{}_{}/{}'.format(index, resource_dir_name, archive_path)
      files_to_zip.append((archive_path, path))

  path_info.Write(zip_path + '.info')

  with zipfile.ZipFile(zip_path, 'w') as z:
    # This magic comment signals to resource_utils.ExtractDeps that this zip is
    # not just the contents of a single res dir, without the encapsulating res/
    # (like the outputs of android_generated_resources targets), but instead has
    # the contents of possibly multiple res/ dirs each within an encapsulating
    # directory within the zip.
    z.comment = resource_utils.MULTIPLE_RES_MAGIC_STRING
    build_utils.DoZip(files_to_zip, z)


def _GenerateRTxt(options, r_txt_path):
  """Generate R.txt file.

  Args:
    options: The command-line options tuple.
    r_txt_path: Locates where the R.txt file goes.
  """
  ignore_pattern = resource_utils.AAPT_IGNORE_PATTERN
  if options.strip_drawables:
    ignore_pattern += ':*drawable*'

  resources_parser.RTxtGenerator(options.resource_dirs,
                                 ignore_pattern).WriteRTxtFile(r_txt_path)


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  # Resource files aren't explicitly listed in GN. Listing them in the depfile
  # ensures the target will be marked stale when resource files are removed.
  depfile_deps = []
  for resource_dir in options.resource_dirs:
    for resource_file in build_utils.FindInDirectory(resource_dir, '*'):
      # Don't list the empty .keep file in depfile. Since it doesn't end up
      # included in the .zip, it can lead to -w 'dupbuild=err' ninja errors
      # if ever moved.
      if not resource_file.endswith(os.path.join('empty', '.keep')):
        depfile_deps.append(resource_file)

  with resource_utils.BuildContext() as build:
    if options.sources:
      _CheckAllFilesListed(options.sources, options.resource_dirs)
    if options.r_text_in:
      r_txt_path = options.r_text_in
    else:
      _GenerateRTxt(options, build.r_txt_path)
      r_txt_path = build.r_txt_path

    if options.r_text_out:
      shutil.copyfile(r_txt_path, options.r_text_out)

    if options.resource_zip_out:
      ignore_pattern = resource_utils.AAPT_IGNORE_PATTERN
      if options.strip_drawables:
        ignore_pattern += ':*drawable*'
      _ZipResources(options.resource_dirs, options.resource_zip_out,
                    ignore_pattern)

  if options.depfile:
    # Order of output must match order specified in GN so that the correct one
    # appears first in the depfile.
    build_utils.WriteDepfile(options.depfile, options.resource_zip_out
                             or options.r_text_out, depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])
