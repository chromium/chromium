# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
import re
from absl import flags
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class HomepageTest(ChromeEnterpriseTestCase):
  """Test HomepageIsNewTabPage and HomepageLocation policies.

  See:
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=HomepageLocation
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=HomepageIsNewTabPage
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ShowHomeButton
  """

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.EnableUITest('client2012')

  def _getHomepageLocation(self, instance_name):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance_name,
                            os.path.join(dir, 'get_homepage_url.py'))
    m = re.search(r"homepage:([^ \r\n]+)", output)
    return m.group(1)

  def _isHomeButtonShown(self, instance_name):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance_name,
                            os.path.join(dir, 'get_home_button.py'))
    return 'home button exists' in output

  @test
  def test_HomepageLocation(self):
    # Test the case where
    # -  HomepageIsNewTabPage is false
    # -  HomepageLocation is set
    # In this case, when a home page is opened, the HomepageLocation is used
    self.SetPolicy('win2012-dc', 'HomepageIsNewTabPage', 0, 'DWORD')
    self.SetPolicy('win2012-dc', 'HomepageLocation',
                   '"http://www.example.com/"', 'String')
    self.RunCommand('client2012', 'gpupdate /force')

    # verify the home page is the value of HomepageLocation
    homepage = self._getHomepageLocation('client2012')
    self.assertEqual(homepage, 'http://www.example.com/')

  @test
  def test_HomepageIsNewTab(self):
    # Test the case when HomepageIsNewTabPage is true
    # In this case, when a home page is opened, the new tab page will be used.
    self.SetPolicy('win2012-dc', 'HomepageIsNewTabPage', 1, 'DWORD')
    self.SetPolicy('win2012-dc', 'HomepageLocation',
                   '"http://www.example.com/"', 'String')
    self.RunCommand('client2012', 'gpupdate /force')

    # verify that the home page is the new tab page.
    homepage = self._getHomepageLocation('client2012')

    # The URL of the new tab can be one of the following:
    # - https://www.google.com/_/chrome/newtab?ie=UTF-8
    # - chrome://newtab
    # - chrome-search://local-ntp/local-ntp.html
    if ('newtab' in homepage
       ) or homepage == 'chrome-search://local-ntp/local-ntp.html':
      pass
    else:
      self.fail('homepage url is not new tab: %s' % homepage)

  @test
  def test_ShowHomeButton(self):
    # Test the case when ShowHomeButton is true
    self.SetPolicy('win2012-dc', 'ShowHomeButton', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    isHomeButtonShown = self._isHomeButtonShown('client2012')
    self.assertTrue(isHomeButtonShown)

    # Test the case when ShowHomeButton is false
    self.SetPolicy('win2012-dc', 'ShowHomeButton', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    isHomeButtonShown = self._isHomeButtonShown('client2012')
    self.assertFalse(isHomeButtonShown)
