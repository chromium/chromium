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
import logging
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
import uuid

from common import GetHostToolPathFromPlatform, GetHostArchFromPlatform
from common import SDK_ROOT, DIR_SOURCE_ROOT

# Structure representing the compressed and uncompressed sizes for a Fuchsia
# package.
PackageSizes = collections.namedtuple('PackageSizes',
                                      ['compressed', 'uncompressed'])


def CreateSizesExternalDiagnostic(sizes_guid):
  """Creates a histogram external sizes diagnostic."""

  benchmark_diagnostic = {
      'type': 'GenericSet',
      'guid': str(sizes_guid),
      'values': ['sizes'],
  }

  return benchmark_diagnostic


def CreateSizesHistogramItem(name, size, sizes_guid):
  """Create a performance dashboard histogram from the histogram template and
  binary size data."""

  # Chromium performance dashboard histogram containing binary size data.
  histogram = {
      'name': name,
      'unit': 'sizeInBytes_smallerIsBetter',
      'diagnostics': {
          'benchmarks': str(sizes_guid),
      },
      'sampleValues': [size],
      'running': [1, size, math.log(size), size, size, size, 0],
      'description': 'chrome-fuchsia package binary sizes',
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


def CreateSizesHistogram(package_sizes):
  """Create a performance dashboard histogram from binary size data."""

  sizes_guid = uuid.uuid1()
  histogram = [CreateSizesExternalDiagnostic(sizes_guid)]
  for name, size in package_sizes.items():
    histogram.append(
        CreateSizesHistogramItem('%s_%s' % (name, 'compressed'),
                                 size.compressed, sizes_guid))
    histogram.append(
        CreateSizesHistogramItem('%s_%s' % (name, 'uncompressed'),
                                 size.uncompressed, sizes_guid))
  return histogram


def GetZstdPathFromPlatform():
  """Returns path to zstd compression utility based on the current platform."""

  arch = GetHostArchFromPlatform()
  if arch == 'arm64':
    zstd_arch_dir = 'zstd-linux-arm64'
  elif arch == 'x64':
    zstd_arch_dir = 'zstd-linux-x64'
  else:
    raise Exception('zstd path unknown for architecture "%s"' % arch)

  return os.path.join(DIR_SOURCE_ROOT, 'third_party', zstd_arch_dir, 'bin',
                      'zstd')


def CompressedSize(file_path, compression_args):
  """Calculates size file after zstd compression.  Uses non-chunked compression
  (Fuchsia uses chunked compression which is not available in the zstd command
  line tool).  The compression level can be set using compression_args."""

  zstd_path = GetZstdPathFromPlatform()
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

  if not os.path.isfile(far_tool):
    raise Exception('Could not find FAR host tool "%s".' % far_tool)
  if not os.path.isfile(file_path):
    raise Exception('Could not find FAR file "%s".' % file_path)
  if os.path.isdir(extract_dir):
    raise Exception('Could not find extraction directory "%s".' % extract_dir)

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


def GetBinarySizes(args):
  """Get binary size data for packages specified in args.

  If "total_size_name" is set, then computes a synthetic package size which is
  the aggregated sizes across all blobs."""

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

  for name, size in package_sizes.items():
    print('%s: compressed %d, uncompressed %d' %
          (name, size.compressed, size.uncompressed))

  return package_sizes


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--build-out-dir',
      '--output-directory',
      type=os.path.realpath,
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
  parser.add_argument(
      '--isolated-script-test-output',
      type=os.path.realpath,
      help='File to which simplified JSON results will be written.')
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
  # Accepted to conform to the isolated script interface, but ignored.
  parser.add_argument('--isolated-script-test-filter', help=argparse.SUPPRESS)
  parser.add_argument('--isolated-script-test-perf-output',
                      help=argparse.SUPPRESS)
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

  isolated_script_output = {
      'valid': False,
      'failures': [],
      'version': 'simplified'
  }
  test_name = 'sizes'

  results_directory = None
  if args.isolated_script_test_output:
    results_directory = os.path.join(
        os.path.dirname(args.isolated_script_test_output), test_name)
    if not os.path.exists(results_directory):
      os.makedirs(results_directory)

  try:
    package_sizes = GetBinarySizes(args)
    sizes_histogram = CreateSizesHistogram(package_sizes)
    isolated_script_output = {
        'valid': True,
        'failures': [],
        'version': 'simplified',
    }
  except:
    _, value, trace = sys.exc_info()
    traceback.print_tb(trace)
    print(str(value))
    return 1
  finally:
    if results_directory:
      results_path = os.path.join(results_directory, 'test_results.json')
      with open(results_path, 'w') as output_file:
        json.dump(isolated_script_output, output_file)

      histogram_path = os.path.join(results_directory, 'perf_results.json')
      with open(histogram_path, 'w') as f:
        json.dump(sizes_histogram, f)

  return 0


if __name__ == '__main__':
  sys.exit(main())
