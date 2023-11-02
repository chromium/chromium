# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains tests which use dexdump of monochrome's dex files."""

import shutil
import tempfile
import zipfile

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
