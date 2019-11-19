# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class ExtensionInstallBlacklistTest(ChromeEnterpriseTestCase):
  """Test the ExtensionInstallBlacklist policy.
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ExtensionInstallBlacklist"""

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  def installExtension(self, url):
    args = ['--url', url, '--text_only', '--wait', '5']

    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunWebDriverTest('client2012',
                                   os.path.join(dir, '../install_extension.py'),
                                   args)
    return output

  @test
  def test_ExtensionBlacklist_all(self):
    extension = '*'
    self.SetPolicy('win2012-dc', r'ExtensionInstallBlacklist\1', extension,
                   'String')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Disabled extension install for ' + extension)

    test_url = 'https://chrome.google.com/webstore/detail/google-hangouts/nckgahadagoaajjgafhacjanaoiihapd'
    output = self.installExtension(test_url)
    self.assertIn('blocked', output)

  @test
  def test_ExtensionBlacklist_hangout(self):
    extension = 'nckgahadagoaajjgafhacjanaoiihapd'
    self.SetPolicy('win2012-dc', r'ExtensionInstallBlacklist\1', extension,
                   'String')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Disabled extension install for ' + extension)

    test_url = 'https://chrome.google.com/webstore/detail/google-hangouts/nckgahadagoaajjgafhacjanaoiihapd'
    output = self.installExtension(test_url)
    self.assertIn('blocked', output)

    positive_test_url = 'https://chrome.google.com/webstore/detail/grammarly-for-chrome/kbfnbcaeplbcioakkpcpgfkobkghlhen'
    output = self.installExtension(positive_test_url)
    self.assertNotIn('blocked', output)
