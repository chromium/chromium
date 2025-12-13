# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions that modify resources in protobuf format.

Format reference:
https://cs.android.com/search?q=f:aapt2.*Resources.proto
"""

import logging
import os
import struct
import sys
import zipfile

from util import build_utils

sys.path[1:1] = [
    # `Resources_pb2` module imports `descriptor`, which imports `six`.
    os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party', 'six', 'src'),
    # Make sure the pb2 files are able to import google.protobuf
    os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party', 'protobuf',
                 'python'),
]

from proto import Resources_pb2

# First bytes in an .flat.arsc file.
# uint32: Magic ("ARSC"), version (1), num_entries (1), type (0)
_FLAT_ARSC_HEADER = b'AAPT\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00'


def _ProcessZip(zip_path, process_func):
  """Filters a .zip file via: new_bytes = process_func(filename, data)."""
  has_changes = False
  zip_entries = []
  with zipfile.ZipFile(zip_path) as src_zip:
    for info in src_zip.infolist():
      data = src_zip.read(info)
      new_data = process_func(info.filename, data)
      if new_data is not data:
        has_changes = True
        data = new_data
      zip_entries.append((info, data))

  # Overwrite the original zip file.
  if has_changes:
    with zipfile.ZipFile(zip_path, 'w') as f:
      for info, data in zip_entries:
        f.writestr(info, data)


def _ProcessProtoXmlNode(xml_node):
  if not xml_node.HasField('element'):
    return

  for child in xml_node.element.child:
    _ProcessProtoXmlNode(child)


def _SplitLocaleResourceType(_type, allowed_resource_names):
  """Splits locale specific resources out of |_type| and returns them.

  Any locale specific resources will be removed from |_type|, and a new
  Resources_pb2.Type value will be returned which contains those resources.

  Args:
    _type: A Resources_pb2.Type value
    allowed_resource_names: Names of locale resources that should be kept in the
        main type.
  """
  locale_entries = []
  for entry in _type.entry:
    if entry.name in allowed_resource_names:
      continue

    # First collect all resources values with a locale set.
    config_values_with_locale = []
    for config_value in entry.config_value:
      if config_value.config.locale:
        config_values_with_locale.append(config_value)

    if config_values_with_locale:
      # Remove the locale resources from the original entry
      for value in config_values_with_locale:
        entry.config_value.remove(value)

      # Add locale resources to a new Entry, and save for later.
      locale_entry = Resources_pb2.Entry()
      locale_entry.CopyFrom(entry)
      del locale_entry.config_value[:]
      locale_entry.config_value.extend(config_values_with_locale)
      locale_entries.append(locale_entry)

  if not locale_entries:
    return None

  # Copy the original type and replace the entries with |locale_entries|.
  locale_type = Resources_pb2.Type()
  locale_type.CopyFrom(_type)
  del locale_type.entry[:]
  locale_type.entry.extend(locale_entries)
  return locale_type


class _ResourceStripper:
  def __init__(self, partial_path, keep_predicate):
    self.partial_path = partial_path
    self.keep_predicate = keep_predicate
    self._has_changes = False

  @staticmethod
  def _IterStyles(entry):
    for config_value in entry.config_value:
      value = config_value.value
      if value.HasField('compound_value'):
        compound_value = value.compound_value
        if compound_value.HasField('style'):
          yield compound_value.style

  def _StripStyles(self, entry, type_and_name):
    # Strip style entries that refer to attributes that have been stripped.
    for style in self._IterStyles(entry):
      entries = style.entry
      new_entries = []
      for e in entries:
        full_name = '{}/{}'.format(type_and_name, e.key.name)
        if not self.keep_predicate(full_name):
          logging.debug('Stripped %s/%s', self.partial_path, full_name)
        else:
          new_entries.append(e)

      if len(new_entries) != len(entries):
        self._has_changes = True
        del entries[:]
        entries.extend(new_entries)

  def _StripEntries(self, entries, type_name):
    new_entries = []
    for entry in entries:
      type_and_name = '{}/{}'.format(type_name, entry.name)
      if not self.keep_predicate(type_and_name):
        logging.debug('Stripped %s/%s', self.partial_path, type_and_name)
      else:
        new_entries.append(entry)
        self._StripStyles(entry, type_and_name)

    if len(new_entries) != len(entries):
      self._has_changes = True
      del entries[:]
      entries.extend(new_entries)

  def StripTable(self, table):
    self._has_changes = False
    for package in table.package:
      for _type in package.type:
        self._StripEntries(_type.entry, _type.name)
    return self._has_changes


def _TableFromFlatBytes(filename, data):
  # https://cs.android.com/search?q=f:aapt2.*Container.cpp
  size_idx = len(_FLAT_ARSC_HEADER)
  proto_idx = size_idx + 8
  if data[:size_idx] != _FLAT_ARSC_HEADER:
    raise Exception(f'Wrong header bytes in {filename}')
  # Size is stored as uint64.
  size = struct.unpack('<Q', data[size_idx:proto_idx])[0]
  table = Resources_pb2.ResourceTable()
  proto_bytes = data[proto_idx:proto_idx + size]
  table.ParseFromString(proto_bytes)
  return table


def _FlatBytesFromTable(table):
  proto_bytes = table.SerializeToString()
  size = struct.pack('<Q', len(proto_bytes))
  overage = len(proto_bytes) % 4
  padding = b'\0' * (4 - overage) if overage else b''
  return b''.join((_FLAT_ARSC_HEADER, size, proto_bytes, padding))


def StripUnwantedResources(partial_path, keep_predicate):
  """Removes resources from .arsc.flat files inside of a .zip.

  Args:
    partial_path: Path to a .zip containing .arsc.flat entries
    keep_predicate: Given "$partial_path/$res_type/$res_name", returns
      whether to keep the resource.
  """
  stripper = _ResourceStripper(partial_path, keep_predicate)

  def process_file(filename, data):
    if filename.endswith('.arsc.flat'):
      table = _TableFromFlatBytes(filename, data)
      if stripper.StripTable(table):
        data = _FlatBytesFromTable(table)
    return data

  _ProcessZip(partial_path, process_file)
