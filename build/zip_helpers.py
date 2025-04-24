# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for dealing with .zip files."""

import os
import pathlib
import posixpath
import stat
import time
import zipfile

_FIXED_ZIP_HEADER_LEN = 30


def _set_alignment(zip_obj, zip_info, alignment):
  """Sets a ZipInfo's extra field such that the file will be aligned.

  Args:
    zip_obj: The ZipFile object that is being written.
    zip_info: The ZipInfo object about to be written.
    alignment: The amount of alignment (e.g. 4, or 4*1024).
  """
  header_size = _FIXED_ZIP_HEADER_LEN + len(zip_info.filename)
  pos = zip_obj.fp.tell() + header_size
  padding_needed = (alignment - (pos % alignment)) % alignment

  # Python writes |extra| to both the local file header and the central
  # directory's file header. Android's zipalign tool writes only to the
  # local file header, so there is more overhead in using Python to align.
  zip_info.extra = b'\0' * padding_needed


def _hermetic_date_time(timestamp=None):
  if not timestamp:
    return (2001, 1, 1, 0, 0, 0)
  utc_time = time.gmtime(timestamp)
  return (utc_time.tm_year, utc_time.tm_mon, utc_time.tm_mday, utc_time.tm_hour,
          utc_time.tm_min, utc_time.tm_sec)


def add_to_zip_hermetic(zip_file,
                        zip_path,
                        *,
                        src_path=None,
                        data=None,
                        compress=None,
                        alignment=None,
                        timestamp=None):
  """Adds a file to the given ZipFile with a hard-coded modified time.

  Args:
    zip_file: ZipFile instance to add the file to.
    zip_path: Destination path within the zip file (or ZipInfo instance).
    src_path: Path of the source file. Mutually exclusive with |data|.
    data: File data as a string.
    compress: Whether to enable compression. Default is taken from ZipFile
        constructor.
    alignment: If set, align the data of the entry to this many bytes.
    timestamp: The last modification date and time for the archive member.
  """
  assert (src_path is None) != (data is None), (
      '|src_path| and |data| are mutually exclusive.')
  if isinstance(zip_path, zipfile.ZipInfo):
    zipinfo = zip_path
    zip_path = zipinfo.filename
  else:
    zipinfo = zipfile.ZipInfo(filename=zip_path)
    zipinfo.external_attr = 0o644 << 16

  zipinfo.date_time = _hermetic_date_time(timestamp)

  if alignment:
    _set_alignment(zip_file, zipinfo, alignment)

  # Filenames can contain backslashes, but it is more likely that we've
  # forgotten to use forward slashes as a directory separator.
  assert '\\' not in zip_path, 'zip_path should not contain \\: ' + zip_path
  assert not posixpath.isabs(zip_path), 'Absolute zip path: ' + zip_path
  assert not zip_path.startswith('..'), 'Should not start with ..: ' + zip_path
  assert posixpath.normpath(zip_path) == zip_path, (
      f'Non-canonical zip_path: {zip_path} vs: {posixpath.normpath(zip_path)}')
  assert zip_path not in zip_file.namelist(), (
      'Tried to add a duplicate zip entry: ' + zip_path)

  if src_path and os.path.islink(src_path):
    zipinfo.external_attr |= stat.S_IFLNK << 16  # mark as a symlink
    zip_file.writestr(zipinfo, os.readlink(src_path))
    return

  # Maintain the executable bit.
  if src_path:
    st = os.stat(src_path)
    for mode in (stat.S_IXUSR, stat.S_IXGRP, stat.S_IXOTH):
      if st.st_mode & mode:
        zipinfo.external_attr |= mode << 16

  if src_path:
    with open(src_path, 'rb') as f:
      data = f.read()

  # zipfile will deflate even when it makes the file bigger. To avoid
  # growing files, disable compression at an arbitrary cut off point.
  if len(data) < 16:
    compress = False

  # None converts to ZIP_STORED, when passed explicitly rather than the
  # default passed to the ZipFile constructor.
  compress_type = zip_file.compression
  if compress is not None:
    compress_type = zipfile.ZIP_DEFLATED if compress else zipfile.ZIP_STORED
  zip_file.writestr(zipinfo, data, compress_type)


def add_files_to_zip(inputs,
                     output,
                     *,
                     base_dir=None,
                     compress=None,
                     zip_prefix_path=None,
                     timestamp=None):
  """Creates a zip file from a list of files.

  Args:
    inputs: A list of paths to zip, or a list of (zip_path, fs_path) tuples.
    output: Path, fileobj, or ZipFile instance to add files to.
    base_dir: Prefix to strip from inputs.
    compress: Whether to compress
    zip_prefix_path: Path prepended to file path in zip file.
    timestamp: Unix timestamp to use for files in the archive.
  """
  if base_dir is None:
    base_dir = '.'
  input_tuples = []
  for tup in inputs:
    if isinstance(tup, str):
      src_path = tup
      zip_path = os.path.relpath(src_path, base_dir)
      # Zip files always use / as path separator.
      if os.path.sep != posixpath.sep:
        zip_path = str(pathlib.Path(zip_path).as_posix())
      tup = (zip_path, src_path)
    input_tuples.append(tup)

  # Sort by zip path to ensure stable zip ordering.
  input_tuples.sort(key=lambda tup: tup[0])

  out_zip = output
  if not isinstance(output, zipfile.ZipFile):
    out_zip = zipfile.ZipFile(output, 'w')

  try:
    for zip_path, fs_path in input_tuples:
      if zip_prefix_path:
        zip_path = posixpath.join(zip_prefix_path, zip_path)
      add_to_zip_hermetic(out_zip,
                          zip_path,
                          src_path=fs_path,
                          compress=compress,
                          timestamp=timestamp)
  finally:
    if output is not out_zip:
      out_zip.close()


def zip_directory(output, base_dir, **kwargs):
  """Zips all files in the given directory."""
  inputs = []
  for root, _, files in os.walk(base_dir):
    for f in files:
      inputs.append(os.path.join(root, f))

  add_files_to_zip(inputs, output, base_dir=base_dir, **kwargs)


def merge_zips(output, input_zips, path_transform=None, compress=None):
  """Combines all files from |input_zips| into |output|.

  Args:
    output: Path, fileobj, or ZipFile instance to add files to.
    input_zips: Iterable of paths to zip files to merge.
    path_transform: Called for each entry path. Returns a new path, or None to
        skip the file.
    compress: Overrides compression setting from origin zip entries.
  """
  assert not isinstance(input_zips, str)  # Easy mistake to make.
  if isinstance(output, zipfile.ZipFile):
    out_zip = output
    out_filename = output.filename
  else:
    assert isinstance(output, str), 'Was: ' + repr(output)
    out_zip = zipfile.ZipFile(output, 'w')
    out_filename = output

  # Include paths in the existing zip here to avoid adding duplicate files.
  crc_by_name = {i.filename: (out_filename, i.CRC) for i in out_zip.infolist()}

  try:
    for in_file in input_zips:
      with zipfile.ZipFile(in_file, 'r') as in_zip:
        for info in in_zip.infolist():
          # Ignore directories.
          if info.filename[-1] == '/':
            continue
          if path_transform:
            dst_name = path_transform(info.filename)
            if dst_name is None:
              continue
          else:
            dst_name = info.filename

          data = in_zip.read(info)

          # If there's a duplicate file, ensure contents is the same and skip
          # adding it multiple times.
          if dst_name in crc_by_name:
            orig_filename, orig_crc = crc_by_name[dst_name]
            new_crc = zipfile.crc32(data)
            if new_crc == orig_crc:
              continue
            msg = f"""File appeared in multiple inputs with differing contents.
File: {dst_name}
Input1: {orig_filename}
Input2: {in_file}"""
            raise Exception(msg)

          if compress is not None:
            compress_entry = compress
          else:
            compress_entry = info.compress_type != zipfile.ZIP_STORED
          add_to_zip_hermetic(out_zip,
                              dst_name,
                              data=data,
                              compress=compress_entry)
          crc_by_name[dst_name] = (in_file, out_zip.getinfo(dst_name).CRC)
  finally:
    if output is not out_zip:
      out_zip.close()
