#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import boot_data
import os
import tempfile
import unittest


class TestBootData(unittest.TestCase):
  def testProvisionSSHGeneratesFiles(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      boot_data.ProvisionSSH(temp_dir)
      fuchsia_authorized_keys_path = os.path.join(temp_dir,
                                                  'fuchsia_authorized_keys')
      self.assertTrue(os.path.exists(fuchsia_authorized_keys_path))
      fuchsia_id_key_path = os.path.join(temp_dir, 'fuchsia_ed25519')
      self.assertTrue(os.path.exists(fuchsia_id_key_path))
      ssh_config_path = os.path.join(temp_dir, 'ssh_config')
      self.assertTrue(os.path.exists(ssh_config_path))


if __name__ == '__main__':
  unittest.main()
