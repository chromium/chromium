# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from chrome_ent_test.infra.core import before_all, environment, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class AllowDeletingBrowserHistory(ChromeEnterpriseTestCase):
  """Test the AllowDeletingBrowserHistory policy:

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=AllowDeletingBrowserHistory.
    """

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  def allowDeletingBrowserHistoryEnabled(self, instance_name):
    """Returns true if AllowDeletingBrowserHistory is enabled."""
    directory = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2012',
        os.path.join(directory,
                     'allow_deleting_browser_history_webdriver_test.py'))
    return 'ENABLED' in output

  @test
  def test_allow_deleting_browser_history_enabled(self):
    self.SetPolicy('win2012-dc', r'AllowDeletingBrowserHistory', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    policy_enabled = self.allowDeletingBrowserHistoryEnabled('client2012')
    self.assertTrue(policy_enabled)

  @test
  def test_allow_deleting_browser_history_disabled(self):
    self.SetPolicy('win2012-dc', r'AllowDeletingBrowserHistory', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    policy_enabled = self.allowDeletingBrowserHistoryEnabled('client2012')
    self.assertFalse(policy_enabled)
