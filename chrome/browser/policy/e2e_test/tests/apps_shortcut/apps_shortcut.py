# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class AppsShortcutEnabledTest(ChromeEnterpriseTestCase):
  """Test the ShowAppsShortcutInBookmarkBar policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ShowAppsShortcutInBookmarkBar"""

  Policy = 'ShowAppsShortcutInBookmarkBar'

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.EnableUITest('client2012')

    # Enable the bookmark bar so we can see the Apps Shortcut that lives there.
    self.SetPolicy('win2012-dc', 'BookmarkBarEnabled', 1, 'DWORD')

  def isAppsShortcutVisible(self, instance):
    local = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance,
                            os.path.join(local, 'is_apps_shortcut_visible.py'))
    return "TRUE" in output

  @test
  def test_AppShortcutEnabled(self):
    self.SetPolicy('win2012-dc', AppsShortcutEnabledTest.Policy, 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    visible = self.isAppsShortcutVisible('client2012')
    self.assertTrue(visible)

  @test
  def test_AppShortcutDisabled(self):
    self.SetPolicy('win2012-dc', AppsShortcutEnabledTest.Policy, 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    visible = self.isAppsShortcutVisible('client2012')
    self.assertFalse(visible)
