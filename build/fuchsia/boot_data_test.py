#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import boot_data
import os
import unittest
from boot_data import _SSH_CONFIG_DIR, _SSH_DIR


class TestBootData(unittest.TestCase):
  def testProvisionSSHGeneratesFiles(self):
    fuchsia_authorized_keys_path = os.path.join(_SSH_DIR,
                                                'fuchsia_authorized_keys')
    fuchsia_id_key_path = os.path.join(_SSH_DIR, 'fuchsia_ed25519')
    pub_keys_path = os.path.join(_SSH_DIR, 'fuchsia_ed25519.pub')
    ssh_config_path = os.path.join(_SSH_CONFIG_DIR, 'ssh_config')
    # Check if the keys exists before generating. If they do, delete them
    # afterwards before asserting if ProvisionSSH works.
    authorized_key_before = os.path.exists(fuchsia_authorized_keys_path)
    id_keys_before = os.path.exists(fuchsia_id_key_path)
    pub_keys_before = os.path.exists(pub_keys_path)
    ssh_config_before = os.path.exists(ssh_config_path)
    ssh_dir_before = os.path.exists(_SSH_CONFIG_DIR)
    boot_data.ProvisionSSH()
    authorized_key_after = os.path.exists(fuchsia_authorized_keys_path)
    id_keys_after = os.path.exists(fuchsia_id_key_path)
    ssh_config_after = os.path.exists(ssh_config_path)
    if not authorized_key_before:
      os.remove(fuchsia_authorized_keys_path)
    if not id_keys_before:
      os.remove(fuchsia_id_key_path)
    if not pub_keys_before:
      os.remove(pub_keys_path)
    if not ssh_config_before:
      os.remove(ssh_config_path)
    if not ssh_dir_before:
      os.rmdir(_SSH_CONFIG_DIR)
    self.assertTrue(os.path.exists(authorized_key_after))
    self.assertTrue(os.path.exists(id_keys_after))
    self.assertTrue(os.path.exists(ssh_config_after))


if __name__ == '__main__':
  unittest.main()
