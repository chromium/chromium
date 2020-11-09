#!/usr/bin/env python
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Implements Chrome-Fuchsia package binary size checks.'''

from __future__ import print_function

import argparse
import collections
import copy
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

from common import GetHostToolPathFromPlatform, SDK_ROOT, DIR_SOURCE_ROOT

# Import SendResults() from the results_dashboard.py in tools/perf/core instead
# of the deprecated one in the chrome infra build recipes folder on the recipes
# module path.
sys.path.insert(0, os.path.join(DIR_SOURCE_ROOT, 'tools', 'perf'))
sys.path.insert(0, os.path.join(DIR_SOURCE_ROOT, 'tools', 'perf', 'core'))
from results_dashboard import SendResults
sys.path.pop(0)
sys.path.pop(0)

# Structure representing the compressed and uncompressed sizes for a Fuchsia
# package.
PackageSizes = collections.namedtuple('PackageSizes',
                                      ['compressed', 'uncompressed'])


def CreateBinarySizeHistogram(name, commit_position, size):
  """Create a performance dashboard histogram from the histogram template and
  binary size data."""

  # Chromium performance dashboard histogram containing binary size data.
  histogram = {
      'name': name,
      'sampleValues': [size],
      'maxNumSampleValues': 1,
      'running': [1, size, math.log(size), size, size, size, 0],
      'unit': 'sizeInBytes_smallerIsBetter',
      'description': 'chrome-fuchsia package binary sizes',
      'diagnostics': {
          'chromiumCommitPositions': {
              'type': 'GenericSet',
              'values': [commit_position],
          },
          'bots': {
              'type': 'GenericSet',
              'values': ['fuchsia-fyi-arm64-size']
          },
          'benchmarks': {
              'type': 'GenericSet',
              'values': ['sizes']
          },
          'masters': {
              'type': 'GenericSet',
              'values': ['ChromiumLinux']
          },
      },
      'summaryOptions': {
          'avg': True,
          'count': False,
          'max': False,
          'min': False,
          'std': False,
          'sum': False,
      },
  }

  return histogram


def CompressedSize(file_path, compression_args):
  """Calculates size file after zstd compression.  Uses non-chunked compression
  (Fuchsia uses chunked compression which is not available in the zstd command
  line tool).  The compression level can be set using compression_args."""

  # Path to zstd compression utility.
  zstd_path = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'zstd', 'bin',
                           'zstd')

  devnull = open(os.devnull)
  proc = subprocess.Popen([zstd_path, '-f', file_path, '-c'] + compression_args,
                          stdout=open(os.devnull, 'w'),
                          stderr=subprocess.PIPE)
  proc.wait()
  zstd_stats = proc.stderr.readline()

  # Match a compressed bytes total from zstd stderr output like
  # test                 : 14.04%   (  3890 =>    546 bytes, test.zst)
  zstd_compressed_bytes_re = r'\d+\s+=>\s+(?P<bytes>\d+) bytes,'

  match = re.search(zstd_compressed_bytes_re, zstd_stats)
  if not match:
    print(zstd_stats)
    raise Exception('Could not get compressed bytes for %s' % file_path)

  return int(match.group('bytes'))


def ExtractFarFile(file_path, extract_dir):
  """Extracts contents of a Fuchsia archive file to the specified directory."""

  far_tool = GetHostToolPathFromPlatform('far')
  subprocess.check_call([
      far_tool, 'extract',
      '--archive=%s' % file_path,
      '--output=%s' % extract_dir
  ])


def GetBlobNameHashes(meta_dir):
  """Returns mapping from Fuchsia pkgfs paths to blob hashes.  The mapping is
  read from the extracted meta.far archive contained in an extracted package
  archive."""

  blob_name_hashes = {}
  contents_path = os.path.join(meta_dir, 'meta', 'contents')
  with open(contents_path) as lines:
    for line in lines:
      (pkgfs_path, blob_hash) = line.strip().split('=')
      blob_name_hashes[pkgfs_path] = blob_hash
  return blob_name_hashes


def CommitPositionFromGitShow():
  """Returns the chromium commit position for the current workspace."""

  # Match a commit position from a "git show" string like
  # "    Cr-Commit-Position: refs/heads/master@{#819438}"
  git_commit_position_re = r'^\s*Cr-Commit-Position:.*@\{#(?P<position>\d+)\}'

  show_output = subprocess.check_output(['git', 'show', 'origin/master'])
  match = re.search(git_commit_position_re, show_output, re.MULTILINE)
  if match:
    return int(match.group('position'))
  raise RuntimeError('could not get chromium commit position from git show')


def CommitPositionFromBuildProperty(value):
  """Extracts the chromium commit position from a builders got_revision_cp
  property."""

  # Match a commit position from a build properties commit string like
  # "refs/heads/master@{#819458}"
  test_arg_commit_position_re = r'\{#(?P<position>\d+)\}'

  match = re.search(test_arg_commit_position_re, value)
  if match:
    return int(match.group('position'))
  raise RuntimeError('Could not get chromium commit position from test arg.')


def GetSDKLibs():
  """Finds shared objects (.so) under the Fuchsia SDK arch directory in dist or
  lib subdirectories.
  """

  # Fuchsia SDK arch directory path (contains all shared object files).
  sdk_arch_dir = os.path.join(SDK_ROOT, 'arch')
  # Leaf subdirectories containing shared object files.
  sdk_so_leaf_dirs = ['dist', 'lib']
  # Match a shared object file name.
  sdk_so_filename_re = r'\.so(\.\d+)?$'

  lib_names = set()
  for dirpath, _, file_names in os.walk(sdk_arch_dir):
    if os.path.basename(dirpath) in sdk_so_leaf_dirs:
      for name in file_names:
        if re.search(sdk_so_filename_re, name):
          lib_names.add(name)
  return lib_names


def FarBaseName(name):
  _, name = os.path.split(name)
  name = re.sub(r'\.far$', '', name)
  return name


def GetBlobSizes(far_file, build_out_dir, extract_dir, excluded_paths,
                 compression_args):
  """Calculates compressed and uncompressed blob sizes for specified FAR file.
  Does not count blobs from SDK libraries."""

  #TODO(crbug.com/1126177): Use partial sizes for blobs shared by packages.
  base_name = FarBaseName(far_file)

  # Extract files and blobs from the specified Fuchsia archive.
  far_file_path = os.path.join(build_out_dir, far_file)
  far_extract_dir = os.path.join(extract_dir, base_name)
  ExtractFarFile(far_file_path, far_extract_dir)

  # Extract the meta.far archive contained in the specified Fuchsia archive.
  meta_far_file_path = os.path.join(far_extract_dir, 'meta.far')
  meta_far_extract_dir = os.path.join(extract_dir, '%s_meta' % base_name)
  ExtractFarFile(meta_far_file_path, meta_far_extract_dir)

  # Map Linux filesystem blob names to blob hashes.
  blob_name_hashes = GetBlobNameHashes(meta_far_extract_dir)

  # Sum compresses and uncompressed blob sizes, except for SDK blobs.
  blob_sizes = {}
  for blob_name in blob_name_hashes:
    _, blob_base_name = os.path.split(blob_name)
    if blob_base_name not in excluded_paths:
      blob_path = os.path.join(far_extract_dir, blob_name_hashes[blob_name])
      compressed_size = CompressedSize(blob_path, compression_args)
      uncompressed_size = os.path.getsize(blob_path)
      blob_sizes[blob_name] = PackageSizes(compressed_size, uncompressed_size)

  return blob_sizes


def GetPackageSizes(far_files, build_out_dir, extract_dir, excluded_paths,
                    compression_args, print_sizes):
  """Calculates compressed and uncompressed package sizes from blob sizes.
  Does not count blobs from SDK libraries."""

  #TODO(crbug.com/1126177): Use partial sizes for blobs shared by
  # non Chrome-Fuchsia packages.

  # Get sizes for blobs contained in packages.
  package_blob_sizes = {}
  for far_file in far_files:
    package_name = FarBaseName(far_file)
    package_blob_sizes[package_name] = GetBlobSizes(far_file, build_out_dir,
                                                    extract_dir, excluded_paths,
                                                    compression_args)

  # Optionally print package blob sizes (does not count sharing).
  if print_sizes:
    for package_name in sorted(package_blob_sizes.keys()):
      print('Package: %s' % package_name)
      for blob_name in sorted(package_blob_sizes[package_name].keys()):
        size = package_blob_sizes[package_name][blob_name]
        print('blob: %s %d %d' %
              (blob_name, size.compressed, size.uncompressed))

  # Count number of packages sharing blobs (a count of 1 is not shared).
  blob_counts = collections.defaultdict(int)
  for package_name in package_blob_sizes:
    for blob_name in package_blob_sizes[package_name]:
      blob_counts[blob_name] += 1

  # Package sizes are the sum of blob sizes divided by their share counts.
  package_sizes = {}
  for package_name in package_blob_sizes:
    compressed_size = 0
    uncompressed_size = 0
    for blob_name in package_blob_sizes[package_name]:
      count = blob_counts[blob_name]
      size = package_blob_sizes[package_name][blob_name]
      compressed_size += size.compressed / count
      uncompressed_size += size.uncompressed / count
    package_sizes[package_name] = PackageSizes(compressed_size,
                                               uncompressed_size)

  return package_sizes


def GetBinarySizeHistogramsData(args):
  """Get binary size histogram data for packages specified in args."""

  # Calculate compressed and uncompressed package sizes.
  sdk_libs = GetSDKLibs()
  extract_dir = args.extract_dir if args.extract_dir else tempfile.mkdtemp()
  package_sizes = GetPackageSizes(args.far_file, args.build_out_dir,
                                  extract_dir, sdk_libs, args.compression_args,
                                  args.verbose)
  if not args.extract_dir:
    shutil.rmtree(extract_dir)

  # Optionally calculate total compressed and uncompressed package sizes.
  if args.total_size_name:
    compressed = sum([a.compressed for a in package_sizes.values()])
    uncompressed = sum([a.uncompressed for a in package_sizes.values()])
    package_sizes[args.total_size_name] = PackageSizes(compressed, uncompressed)

  # Determine chromium commit position from builder property or workspace.
  if args.test_revision_cp:
    commit_position = CommitPositionFromBuildProperty(args.test_revision_cp)
  else:
    commit_position = CommitPositionFromGitShow()

  print('Chromium commit position: %d' % commit_position)
  for name, size in package_sizes.items():
    print('%s: compressed %d, uncompressed %d' %
          (name, size.compressed, size.uncompressed))

  # Generate chromium performance dashboard histogram data for each binary size.
  histograms_data = []
  for name, size in package_sizes.items():
    histograms_data.append(
        CreateBinarySizeHistogram('%s_%s' % (name, 'compressed'),
                                  commit_position, size.compressed))
    histograms_data.append(
        CreateBinarySizeHistogram('%s_%s' % (name, 'uncompressed'),
                                  commit_position, size.uncompressed))

  return histograms_data


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--build-out-dir',
      required=True,
      help='Location of the build artifacts.',
  )
  parser.add_argument('--compression-args',
                      '--zstd_args',
                      action='append',
                      default=[],
                      help='Arguments to pass to zstd compression utility.')
  parser.add_argument(
      '--extract-dir',
      help='Debugging option, specifies directory for extracted FAR files.'
      'If present, extracted files will not be deleted after use.')
  parser.add_argument(
      '--far-file',
      required=True,
      action='append',
      help='Name of Fuchsia package FAR file (may be used more than once.)')
  parser.add_argument('--histogram-path',
                      help='Optional output file for histogram data')
  parser.add_argument(
      '--server-url',
      help='Write data to performance dashboard server URL instead of to a file'
  )
  parser.add_argument(
      '--output-dir',
      help='Optional directory for histogram output file.  This argument is '
      'automatically supplied by the recipe infrastructure when this script '
      'is invoked by a recipe call to api.chromium.runtest().')
  parser.add_argument(
      '--test-revision-cp',
      help='Set the chromium commit point NNNNNN from a build property value '
      'like "refs/heads/master@{#NNNNNNN}".  Intended for use in recipes with '
      'the build property got_revision_cp',
  )
  parser.add_argument('--total-size-name',
                      help='Enable a total sizes metric and specify its name')
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable verbose output')
  args = parser.parse_args()

  # Optionally prefix the output_dir to the histogram_path.
  if args.output_dir and args.histogram_path:
    args.histogram_path = os.path.join(args.output_dir, args.histogram_path)

  # If the zstd compression level is not specified, use Fuchsia's default level.
  compression_level_args = [
      value for value in args.compression_args if re.match(r'-\d+$', value)
  ]
  if not compression_level_args:
    args.compression_args.append('-14')

  if args.verbose:
    print('Fuchsia binary sizes')
    print('Working directory', os.getcwd())
    print('Args:')
    for var in vars(args):
      print('  {}: {}'.format(var, getattr(args, var) or ''))

  if not os.path.isdir(args.build_out_dir):
    raise Exception('Could not find build output directory "%s".' %
                    args.build_out_dir)

  if args.extract_dir and not os.path.isdir(args.extract_dir):
    raise Exception(
        'Could not find FAR file extraction output directory "%s".' %
        args.extract_dir)

  for far_rel_path in args.far_file:
    far_abs_path = os.path.join(args.build_out_dir, far_rel_path)
    if not os.path.isfile(far_abs_path):
      raise Exception('Could not find FAR file "%s".' % far_abs_path)

  histograms_data = GetBinarySizeHistogramsData(args)

  if args.server_url:
    # Send histograms to the performance dashboard.
    for data in histograms_data:
      SendResults([data],
                  'chrome_fuchsia_package_size',
                  args.server_url,
                  send_as_histograms=True)

  if args.histogram_path:
    # Save histogram data to a file.
    histogram_dir = os.path.dirname(os.path.abspath(args.histogram_path))
    if not os.path.isdir(histogram_dir):
      raise Exception('Could not find histogram file output directory "%s".' %
                      histogram_dir)
    json_out = open(args.histogram_path, 'w')
    json.dump(histograms_data, json_out)

  return 0


if __name__ == '__main__':
  sys.exit(main())
