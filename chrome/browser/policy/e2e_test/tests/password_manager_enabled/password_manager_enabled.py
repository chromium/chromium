# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class PasswordManagerEnabledTest(ChromeEnterpriseTestCase):
  """Test the PasswordManagerEnabled policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=PasswordManagerEnabled"""

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  def isPasswordManagerEnabled(self):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2012',
        os.path.join(dir, 'password_manager_enabled_webdriver_test.py'))
    return "TRUE" in output

  @test
  def test_PasswordManagerDisabled(self):
    self.SetPolicy('win2012-dc', 'PasswordManagerEnabled', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    enabled = self.isPasswordManagerEnabled()
    self.assertFalse(enabled)

  @test
  def test_PasswordManagerEnabled(self):
    self.SetPolicy('win2012-dc', 'PasswordManagerEnabled', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    enabled = self.isPasswordManagerEnabled()
    self.assertTrue(enabled)
