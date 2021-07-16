# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests different flags to see if they are being used correctly"""

import common
import fvdl_target
import os
import subprocess
import sys
import tempfile
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock


class TestBuildCommandFvdlTarget(unittest.TestCase):
  def setUp(self):
    self.target = fvdl_target.FvdlTarget(out_dir='/output/dir',
                                         target_cpu='x64',
                                         system_log_file=None,
                                         require_kvm=True,
                                         enable_graphics=False,
                                         hardware_gpu=False)

  def testBasicEmuCommand(self):
    build_command = self.target._BuildCommand()
    self.assertIn(fvdl_target._FVDL_PATH, build_command)
    self.assertIn('--sdk', build_command)
    self.assertIn('start', build_command)
    fuchsia_authorized_keys_path = os.path.join(fvdl_target._SSH_KEY_DIR,
                                                'fuchsia_authorized_keys')
    self.assertTrue(fuchsia_authorized_keys_path)
    fuchsia_id_key_path = os.path.join(fvdl_target._SSH_KEY_DIR,
                                       'fuchsia_ed25519')
    self.assertTrue(fuchsia_id_key_path)

  def testBuildCommandCheckIfNotRequireKVMSetNoAcceleration(self):
    self.target._require_kvm = True
    self.assertNotIn('--noacceleration', self.target._BuildCommand())
    self.target._require_kvm = False
    self.assertIn('--noacceleration', self.target._BuildCommand())

  def testBuildCommandCheckIfNotEnableGraphicsSetHeadless(self):
    self.target._enable_graphics = True
    self.assertNotIn('--headless', self.target._BuildCommand())
    self.target._enable_graphics = False
    self.assertIn('--headless', self.target._BuildCommand())

  def testBuildCommandCheckIfHardwareGpuSetHostGPU(self):
    self.target._hardware_gpu = False
    self.assertNotIn('--host-gpu', self.target._BuildCommand())
    self.target._hardware_gpu = True
    self.assertIn('--host-gpu', self.target._BuildCommand())


if __name__ == '__main__':
  unittest.main()
