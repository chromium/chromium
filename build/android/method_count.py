#! /usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import os
import re
import zipfile

from pylib.dex import dex_parser


class DexStatsCollector:
  """Tracks count of method/field/string/type as well as unique methods."""

  def __init__(self):
    # Signatures of all methods from all seen dex files.
    self._unique_methods = set()
    # Map of label -> { metric -> count }.
    self._counts_by_label = {}

  def _CollectFromDexfile(self, label, dexfile):
    assert label not in self._counts_by_label, 'exists: ' + label
    self._counts_by_label[label] = {
        'fields': dexfile.header.field_ids_size,
        'methods': dexfile.header.method_ids_size,
        'strings': dexfile.header.string_ids_size,
        'types': dexfile.header.type_ids_size,
    }
    self._unique_methods.update(dexfile.IterMethodSignatureParts())

  def CollectFromZip(self, label, path):
    """Add dex stats from an .apk/.jar/.aab/.zip."""
    with zipfile.ZipFile(path, 'r') as z:
      for subpath in z.namelist():
        if not re.match(r'.*classes\d*\.dex$', subpath):
          continue
        dexfile = dex_parser.DexFile(bytearray(z.read(subpath)))
        self._CollectFromDexfile('{}!{}'.format(label, subpath), dexfile)

  def CollectFromDex(self, label, path):
    """Add dex stats from a .dex file."""
    with open(path, 'rb') as f:
      dexfile = dex_parser.DexFile(bytearray(f.read()))
    self._CollectFromDexfile(label, dexfile)

  def MergeFrom(self, parent_label, other):
    """Add dex stats from another DexStatsCollector."""
    # pylint: disable=protected-access
    for label, other_counts in other._counts_by_label.items():
      new_label = '{}-{}'.format(parent_label, label)
      self._counts_by_label[new_label] = other_counts.copy()
    self._unique_methods.update(other._unique_methods)
    # pylint: enable=protected-access

  def GetUniqueMethodCount(self):
    """Returns total number of unique methods across encountered dex files."""
    return len(self._unique_methods)

  def GetCountsByLabel(self):
    """Returns dict of label -> {metric -> count}."""
    return self._counts_by_label

  def GetTotalCounts(self):
    """Returns dict of {metric -> count}, where |count| is sum(metric)."""
    ret = {}
    for metric in ('fields', 'methods', 'strings', 'types'):
      ret[metric] = sum(x[metric] for x in self._counts_by_label.values())
    return ret

  def GetDexCacheSize(self, pre_oreo):
    """Returns number of bytes of dirty RAM is consumed from all dex files."""
    # Dex Cache was optimized in Android Oreo:
    # https://source.android.com/devices/tech/dalvik/improvements#dex-cache-removal
    if pre_oreo:
      total = sum(self.GetTotalCounts().values())
    else:
      total = sum(c['methods'] for c in self._counts_by_label.values())
    return total * 4  # 4 bytes per entry.


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('paths', nargs='+')
  args = parser.parse_args()

  collector = DexStatsCollector()
  for path in args.paths:
    if os.path.splitext(path)[1] in ('.zip', '.apk', '.jar', '.aab'):
      collector.CollectFromZip(path, path)
    else:
      collector.CollectFromDex(path, path)

  counts_by_label = collector.GetCountsByLabel()
  for label, counts in sorted(counts_by_label.items()):
    print('{}:'.format(label))
    for metric, count in sorted(counts.items()):
      print('  {}:'.format(metric), count)
    print()

  if len(counts_by_label) > 1:
    print('Totals:')
    for metric, count in sorted(collector.GetTotalCounts().items()):
      print('  {}:'.format(metric), count)
    print()

  print('Unique Methods:', collector.GetUniqueMethodCount())
  print('DexCache (Pre-Oreo):', collector.GetDexCacheSize(pre_oreo=True),
        'bytes of dirty memory')
  print('DexCache (Oreo+):', collector.GetDexCacheSize(pre_oreo=False),
        'bytes of dirty memory')


if __name__ == '__main__':
  main()
