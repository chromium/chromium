# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class FullscreenAllowedTest(ChromeEnterpriseTestCase):
  """Test the FullscreenAllowed policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=FullscreenAllowed"""

  Policy = 'FullscreenAllowed'

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.EnableUITest('client2012')

    # Enable the bookmark bar so we can see the Apps Shortcut that lives there.
    self.SetPolicy('win2012-dc', 'BookmarkBarEnabled', 1, 'DWORD')

  def isFullscreenAllowed(self, instance):
    local = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance,
                            os.path.join(local, 'is_fullscreen_allowed.py'))
    return "FullscreenAllowed: True" in output

  @test
  def test_FullscreenAllowed(self):
    self.SetPolicy('win2012-dc', FullscreenAllowedTest.Policy, 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    allowed = self.isFullscreenAllowed('client2012')
    self.assertTrue(allowed)

  @test
  def test_FullscreenNotAllowed(self):
    self.SetPolicy('win2012-dc', FullscreenAllowedTest.Policy, 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    allowed = self.isFullscreenAllowed('client2012')
    self.assertFalse(allowed)
