# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class UserDataDirTest(ChromeEnterpriseTestCase):
  """Test the UserDataDir

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=UserDataDir.

    """

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  @test
  def test_user_data_dir(self):
    user_data_dir = r'C:\Temp\Browser\Google\Chrome\UserData'
    self.SetPolicy('win2012-dc', r'UserDataDir', user_data_dir, 'String')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Updated User data dir to: ' + user_data_dir)

    local_dir = os.path.dirname(os.path.abspath(__file__))
    args = ['--user_data_dir', user_data_dir]
    output = self.RunWebDriverTest(
        'client2012', os.path.join(local_dir, 'user_data_dir_webdriver.py'),
        args)

    # Verify user data dir not exsiting before chrome launch
    self.assertIn('User data before running chrome is False', output)
    # Verify policy in chrome://policy page
    self.assertIn('UserDataDir', output)
    self.assertIn(user_data_dir, output)
    # Verify profile path in chrome:// version
    self.assertIn("Profile path is " + user_data_dir, output)
    # Verify user data dir folder creation
    self.assertIn('User data dir creation is True', output)
