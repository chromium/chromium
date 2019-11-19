# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class SafeBrowsingEnabledTest(ChromeEnterpriseTestCase):
  """Test the SafeBrowsingEnabled policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=SafeBrowsingEnabled"""

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.EnableUITest('client2012')

  def isSafeBrowsingEnabled(self):
    dir = os.path.dirname(os.path.abspath(__file__))
    return self.RunUITest(
        'client2012',
        os.path.join(dir, 'safe_browsing_ui_test.py'),
        timeout=600)

  @test
  def test_SafeBrowsingDisabledNoWarning(self):
    self.SetPolicy('win2012-dc', r'SafeBrowsingEnabled', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    output = self.isSafeBrowsingEnabled()
    self.assertIn("RESULTS.unsafe_page: False", output)
    self.assertIn("RESULTS.unsafe_download: False", output)

  @test
  def test_SafeBrowsingEnabledShowsWarning(self):
    self.SetPolicy('win2012-dc', r'SafeBrowsingEnabled', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    output = self.isSafeBrowsingEnabled()
    self.assertIn("RESULTS.unsafe_page: True", output)
    self.assertIn("RESULTS.unsafe_download: True", output)
