#!/usr/bin/env python2.7
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains tests which use dexdump of monochrome's dex files."""

import os
import shutil
import sys
import tempfile
import zipfile

CUR_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(CUR_DIR))))
TYP_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')
DEVIL_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'devil')
DEVIL_CHROMIUM_DIR = os.path.join(SRC_DIR, 'build', 'android')

if TYP_DIR not in sys.path:
  sys.path.insert(0, TYP_DIR)
if DEVIL_DIR not in sys.path:
  sys.path.insert(0, DEVIL_DIR)
if DEVIL_CHROMIUM_DIR not in sys.path:
  sys.path.insert(0, DEVIL_CHROMIUM_DIR)

import devil_chromium
from devil.android.sdk import build_tools
from devil.utils import cmd_helper
import typ


def _extract_dex_dumps(apk):
  build_dir = tempfile.mkdtemp()
  extracted_dex_files = _extract_dex_files(apk, build_dir)
  cmd = [ build_tools.GetPath('dexdump'), '-d' ] + extracted_dex_files
  status, out = cmd_helper.GetCmdStatusAndOutput(cmd)
  shutil.rmtree(build_dir)
  return (status, out)

def _extract_dex_files(apk, dest_dir):
  extracted_files = []
  with zipfile.ZipFile(apk) as z:
    for info in z.infolist():
      if info.filename.endswith('.dex'):
        extracted_files.append(z.extract(info.filename, dest_dir))

  return extracted_files

class MonochromeDexDumpTest(typ.TestCase):
  def testMain(self):
    monochrome_apk = self.context.monochrome_apk
    status, dump = _extract_dex_dumps(monochrome_apk)
    self.assertEquals(status, 0)
    # Check that the dexdump does not have any calls to org.apache.http
    # crbug.com/115604
    self.assertNotIn('org/apache/http', dump)
