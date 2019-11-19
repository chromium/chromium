#! /usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import zipfile

from pylib.constants import host_paths
from pylib.dex import dex_parser

sys.path.append(os.path.join(host_paths.DIR_SOURCE_ROOT, 'build', 'util', 'lib',
                             'common'))
import perf_tests_results_helper # pylint: disable=import-error


_CONTRIBUTORS_TO_DEX_CACHE = {
    'type_ids_size': 'types',
    'string_ids_size': 'strings',
    'method_ids_size': 'methods',
    'field_ids_size': 'fields'
}


def _ExtractSizesFromDexFile(dexfile):
  count_by_item = {}
  for item_name, readable_name in _CONTRIBUTORS_TO_DEX_CACHE.iteritems():
    count_by_item[readable_name] = getattr(dexfile.header, item_name)
  return count_by_item, sum(
      count_by_item[x] for x in _CONTRIBUTORS_TO_DEX_CACHE.itervalues()) * 4


def ExtractSizesFromZip(path):
  dex_counts_by_file = {}
  dexcache_size = 0
  dexfiles = {}
  with zipfile.ZipFile(path, 'r') as z:
    for subpath in z.namelist():
      if not re.match(r'.*classes[0-9]*\.dex$', subpath):
        continue
      dexfile_name = os.path.basename(subpath)
      dexfiles[dexfile_name] = dex_parser.DexFile(bytearray(z.read(subpath)))

  for dexfile_name, dexfile in dexfiles.iteritems():
    cur_dex_counts, cur_dexcache_size = _ExtractSizesFromDexFile(dexfile)
    dex_counts_by_file[dexfile_name] = cur_dex_counts
    dexcache_size += cur_dexcache_size
  num_unique_methods = dex_parser.CountUniqueDexMethods(dexfiles.values())
  return dex_counts_by_file, dexcache_size, num_unique_methods


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('filename')

  args = parser.parse_args()

  if os.path.splitext(args.filename)[1] in ('.zip', '.apk', '.jar'):
    sizes, total_size, num_unique_methods = ExtractSizesFromZip(args.filename)
  else:
    with open(args.filename) as f:
      dexfile = dex_parser.DexFile(bytearray(f.read()))
    single_set_of_sizes, total_size = _ExtractSizesFromDexFile(dexfile)
    sizes = {"": single_set_of_sizes}
    num_unique_methods = single_set_of_sizes['methods']

  file_basename = os.path.basename(args.filename)
  for classes_dex_file, classes_dex_sizes in sizes.iteritems():
    for readable_name in _CONTRIBUTORS_TO_DEX_CACHE.itervalues():
      if readable_name in classes_dex_sizes:
        perf_tests_results_helper.PrintPerfResult(
            '%s_%s_%s' % (file_basename, classes_dex_file, readable_name),
            'total', [classes_dex_sizes[readable_name]], readable_name)

  perf_tests_results_helper.PrintPerfResult('%s_unique_methods' % file_basename,
                                            'total', [num_unique_methods],
                                            'unique methods')

  perf_tests_results_helper.PrintPerfResult(
      '%s_DexCache_size' % (file_basename), 'total', [total_size],
      'bytes of permanent dirty memory')
  return 0

if __name__ == '__main__':
  sys.exit(main())
