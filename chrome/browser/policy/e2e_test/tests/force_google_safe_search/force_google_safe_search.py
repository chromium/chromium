# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
from absl import flags
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class ForceGoogleSafeSearchTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  @test
  def test_ForceGoogleSafeSearchEnabled(self):
    # enable policy ForceGoogleSafeSearch
    self.SetPolicy('win2012-dc', 'ForceGoogleSafeSearch', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('ForceGoogleSafeSearch ENABLED')
    d = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2012',
        os.path.join(d, 'force_google_safe_search_webdriver_test.py'))
    logging.info('url used: %s', output)

    # assert that safe search is enabled
    self.assertIn('safe=active', output)
    self.assertIn('ssui=on', output)

  @test
  def test_ForceGoogleSafeSearchDisabled(self):
    # disable policy ForceGoogleSafeSearch
    self.SetPolicy('win2012-dc', 'ForceGoogleSafeSearch', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    logging.info('ForceGoogleSafeSearch DISABLED')
    output = self.RunWebDriverTest(
        'client2012',
        os.path.join(d, 'force_google_safe_search_webdriver_test.py'))
    logging.info('url used: %s', output)

    # assert that safe search is NOT enabled
    self.assertNotIn('safe=active', output)
    self.assertNotIn('ssui=on', output)
