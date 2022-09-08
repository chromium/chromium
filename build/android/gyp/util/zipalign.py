# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from util import build_utils

_FIXED_ZIP_HEADER_LEN = 30


def _SetAlignment(zip_obj, zip_info, alignment):
  """Sets a ZipInfo's extra field such that the file will be aligned.

  Args:
    zip_obj: The ZipFile object that is being written.
    zip_info: The ZipInfo object about to be written.
    alignment: The amount of alignment (e.g. 4, or 4*1024).
  """
  cur_offset = zip_obj.fp.tell()
  header_size = _FIXED_ZIP_HEADER_LEN + len(zip_info.filename)
  padding_needed = (alignment - (
      (cur_offset + header_size) % alignment)) % alignment


  # Python writes |extra| to both the local file header and the central
  # directory's file header. Android's zipalign tool writes only to the
  # local file header, so there is more overhead in using python to align.
  zip_info.extra = b'\0' * padding_needed


def AddToZipHermetic(zip_file,
                     zip_path,
                     src_path=None,
                     data=None,
                     compress=None,
                     alignment=None):
  """Same as build_utils.AddToZipHermetic(), but with alignment.

  Args:
    alignment: If set, align the data of the entry to this many bytes.
  """
  zipinfo = build_utils.HermeticZipInfo(filename=zip_path)
  if alignment:
    _SetAlignment(zip_file, zipinfo, alignment)
  build_utils.AddToZipHermetic(
      zip_file, zipinfo, src_path=src_path, data=data, compress=compress)
