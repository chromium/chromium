# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class UrlWhitelistTest(ChromeEnterpriseTestCase):
  """Test the URLWhitelist policy.

  This policy provides exceptions to the URLBlacklist policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=URLBlacklist
  and https://cloud.google.com/docs/chrome-enterprise/policies/?policy=URLWhitelist"""

  @before_all
  def setup(self):
    client = 'client2012'
    dc = 'win2012-dc'
    self.InstallChrome(client)
    self.InstallWebDriver(client)

    # Blacklist all sites and add an exception with URLWhitelist.
    self.SetPolicy(dc, r'URLBlacklist\1', '*', 'String')
    self.SetPolicy(dc, r'URLWhitelist\1', 'https://youtube.com', 'String')
    self.RunCommand(client, 'gpupdate /force')

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
  def test_AllowedUrlCanVisit(self):
    output = self.openPage('https://youtube.com/yt/about/')
    self.assertNotIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

  @test
  def test_NotAllowedUrlCantVisit(self):
    output = self.openPage('https://google.com')
    self.assertIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

  @test
  def test_AllowedUrlCanVisitIncognito(self):
    output = self.openPage('https://youtube.com/yt/about/', incognito=True)
    self.assertNotIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)

  @test
  def test_NotAllowedUrlCantVisitIncognito(self):
    output = self.openPage('https://google.com', incognito=True)
    self.assertIn("ERR_BLOCKED_BY_ADMINISTRATOR", output)
