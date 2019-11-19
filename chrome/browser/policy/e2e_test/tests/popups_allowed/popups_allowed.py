# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from absl import flags

from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class PopupsAllowedForUrlsTest(ChromeEnterpriseTestCase):
  """Test the PopupsAllowedForUrls

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=PopupsAllowedForUrls.
    """

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  @test
  def test_popup_allow_for_url(self):
    # Enable "Allow popups on these sites" with testing URL
    # TODO(jxiang, crbug/1020231)
    test_site = 'www.dummysoftware.com'
    self.SetPolicy('win2012-dc', r'PopupsAllowedForUrls\1', test_site, 'String')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Enabled Allow pop-ups on' + test_site)

    # Run webdriver test
    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2012', os.path.join(local_dir,
                                   'popup_allowed_webdriver_test.py'))
    # Check if new pop up window comes up
    self.assertTrue(int(output) > 1)

  @test
  def test_allow_for_other_url(self):
    # Set the allow popup site using google.com, so popuptest.com is disabled
    test_site = 'www.google.com'
    self.SetPolicy('win2012-dc', r'PopupsAllowedForUrls\1', test_site, 'String')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Enabled Allow pop-ups on' + test_site)

    # Run webdriver test
    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2012', os.path.join(local_dir,
                                   'popup_allowed_webdriver_test.py'))
    # Check if the new pop-up windows are blocked
    self.assertEquals(int(output), 1)
