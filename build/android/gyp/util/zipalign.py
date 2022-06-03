# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import struct
import sys
import zipfile

from util import build_utils

_FIXED_ZIP_HEADER_LEN = 30


def _PatchedDecodeExtra(self):
  # Try to decode the extra field.
  extra = self.extra
  unpack = struct.unpack
  while len(extra) >= 4:
    tp, ln = unpack('<HH', extra[:4])
    if tp == 1:
      if ln >= 24:
        counts = unpack('<QQQ', extra[4:28])
      elif ln == 16:
        counts = unpack('<QQ', extra[4:20])
      elif ln == 8:
        counts = unpack('<Q', extra[4:12])
      elif ln == 0:
        counts = ()
      else:
        raise RuntimeError("Corrupt extra field %s" % (ln, ))

      idx = 0

      # ZIP64 extension (large files and/or large archives)
      if self.file_size in (0xffffffffffffffff, 0xffffffff):
        self.file_size = counts[idx]
        idx += 1

      if self.compress_size == 0xffffffff:
        self.compress_size = counts[idx]
        idx += 1

      if self.header_offset == 0xffffffff:
        self.header_offset = counts[idx]
        idx += 1

    extra = extra[ln + 4:]


def ApplyZipFileZipAlignFix():
  """Fix zipfile.ZipFile() to be able to open zipaligned .zip files.

  Android's zip alignment uses not-quite-valid zip headers to perform alignment.
  Python < 3.4 crashes when trying to load them.
  https://bugs.python.org/issue14315
  """
  if sys.version_info < (3, 4):
    zipfile.ZipInfo._decodeExtra = (  # pylint: disable=protected-access
        _PatchedDecodeExtra)


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
