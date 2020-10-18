#!/usr/bin/env python2.7
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains tests which test monochrome's AndroidManifest.xml"""

import os
import sys

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


class MonochromeAndroidManifestCheckerTest(typ.TestCase):
  def testManifest(self):
    monochrome_apk = self.context.monochrome_apk
    cmd = [
        build_tools.GetPath('aapt'), 'dump', 'xmltree', monochrome_apk,
           'AndroidManifest.xml']
    status, manifest = cmd_helper.GetCmdStatusAndOutput(cmd)
    self.assertEquals(status, 0)
    # Check that AndroidManifest.xml does not have any <uses-library> tags.
    # crbug.com/115604
    self.assertNotIn('uses-library', manifest)
