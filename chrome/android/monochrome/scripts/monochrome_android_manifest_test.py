# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains tests which test monochrome's AndroidManifest.xml"""

from devil.android.sdk import build_tools
from devil.utils import cmd_helper
import typ


class MonochromeAndroidManifestCheckerTest(typ.TestCase):
  def testManifest(self):
    monochrome_apk = self.context.monochrome_apk
    cmd = [
        build_tools.GetPath('aapt2'), 'dump', 'xmltree', monochrome_apk,
           '--file', 'AndroidManifest.xml']
    status, manifest = cmd_helper.GetCmdStatusAndOutput(cmd)
    self.assertEquals(status, 0)
    # Check that AndroidManifest.xml does not have any <uses-library> tags.
    # crbug.com/1115604
    self.assertNotIn('uses-library', manifest)
