# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class DefaultSearchProviderTest(ChromeEnterpriseTestCase):
  """Test the DefaultSearchProviderEnabled,
              DefaultSearchProviderName,
              DefaultSearchProviderSearchURL

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=DefaultSearchProviderEnabled
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=DefaultSearchProviderName
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=DefaultSearchProviderSearchURL

    """

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.EnableUITest('client2012')

  def _get_search_url(self, instance_name):
    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(
        instance_name,
        os.path.join(local_dir, 'default_search_provider_webdriver.py'))
    return output

  @test
  def test_default_search_provider_bing(self):
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderEnabled', 1, 'DWORD')
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderName', 'Bing', 'String')
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderSearchURL',
                   '"https://www.bing.com/search?q={searchTerms}"', 'String')
    self.RunCommand('client2012', 'gpupdate /force')

    output = self._get_search_url('client2012')
    self.assertIn('www.bing.com', output)

  @test
  def test_default_search_provider_yahoo(self):
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderEnabled', 1, 'DWORD')
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderName', 'Yahoo', 'String')
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderSearchURL',
                   '"https://search.yahoo.com/search?p={searchTerms}"',
                   'String')
    self.RunCommand('client2012', 'gpupdate /force')

    output = self._get_search_url('client2012')
    self.assertIn('search.yahoo.com', output)

  @test
  def test_default_search_provider_disabled(self):
    self.SetPolicy('win2012-dc', 'DefaultSearchProviderEnabled', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    output = self._get_search_url('client2012')
    self.assertIn('http://anything', output)
