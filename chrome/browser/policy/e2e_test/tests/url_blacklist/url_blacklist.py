# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class UrlBlacklistTest(ChromeEnterpriseTestCase):
  """Test the URLBlacklist policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=URLBlacklist"""

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  def openPage(self, url, incognito=False):
    args = ['--url', url, '--text_only']
    if incognito:
      args += ['--incognito']

    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunWebDriverTest('client2012',
                                   os.path.join(dir, '../open_page.py'), args)
    return output

  @test
  def test_BlacklistAllCantVisit(self, incognito=False):
    self.SetPolicy('win2012-dc', r'URLBlacklist\1', '*', 'String')
    self.RunCommand('client2012', 'gpupdate /force')

    # Verify that we can't visit any site.
    output = self.openPage('https://youtube.com/yt/about/', incognito=incognito)
    self.assertIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

    output = self.openPage('https://google.com', incognito=incognito)
    self.assertIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

  @test
  def test_BlacklistYouTubeCantVisit(self, incognito=False):
    self.SetPolicy('win2012-dc', r'URLBlacklist\1', 'https://youtube.com',
                   'String')
    self.RunCommand('client2012', 'gpupdate /force')

    # Verify that we can't visit YouTube, but can still visit other sites.
    output = self.openPage('https://youtube.com/yt/about/', incognito=incognito)
    self.assertIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

    output = self.openPage('https://google.com', incognito=incognito)
    self.assertNotIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

  @test
  def test_BlacklistAllCantVisitIncognito(self):
    self.test_BlacklistAllCantVisit(incognito=True)

  @test
  def test_BlacklistYouTubeCantVisitIncognito(self):
    self.test_BlacklistYouTubeCantVisit(incognito=True)
