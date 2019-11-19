# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

from absl import flags

from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class RestoreOnStartupTest(ChromeEnterpriseTestCase):
  """Test the RestoreOnStartup policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=RestoreOnStartup."""

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  @test
  def test_RestoreTheLastSession(self):
    logging.info('RestoreOnStartup is set to RestoreTheLastSession')

    self.SetPolicy('win2012-dc', 'RestoreOnStartup', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    # delete the user data directory to make sure we start from a clean slate.
    user_data_dir = r'c:\temp\user1'
    self.RunCommand(
        'client2012',
        'cmd /C if exist %s rmdir /s /q %s' % (user_data_dir, user_data_dir))
    dir = os.path.dirname(os.path.abspath(__file__))
    user_data_dir_arg = '--user_data_dir=%s' % user_data_dir
    urls = ['https://www.cnn.com/', 'https://www.youtube.com/']
    list.sort(urls)

    # create a session: start Chrome and open several URLs.
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'restore_on_startup_webdriver_test.py'),
        [
            '--action=open_urls',
            user_data_dir_arg,
        ] + ['--urls=%s' % url for url in urls])
    output_urls = json.loads(output)
    self.assertEqual(urls, output_urls)

    # start Chrome. The last session should be restored.
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'restore_on_startup_webdriver_test.py'),
        [
            '--action=start_chrome',
            user_data_dir_arg,
        ])
    output_urls = json.loads(output)
    self.assertEqual(urls, output_urls)

  @test
  def test_OpenNewTabPage(self):
    logging.info('RestoreOnStartup is set to Open New Tab Page')

    self.SetPolicy('win2012-dc', 'RestoreOnStartup', 5, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')
    dir = os.path.dirname(os.path.abspath(__file__))
    user_data_dir_arg = r'--user_data_dir=c:\temp\user2'
    urls = ['https://www.cnn.com/', 'https://www.youtube.com/']
    list.sort(urls)

    # create a session: start Chrome and open several URLs.
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'restore_on_startup_webdriver_test.py'),
        [
            '--action=open_urls',
            user_data_dir_arg,
        ] + ['--urls=%s' % url for url in urls])
    output_urls = json.loads(output)
    self.assertEqual(urls, output_urls)

    # start Chrome. There should be just one New Tab page.
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'restore_on_startup_webdriver_test.py'),
        [
            '--action=start_chrome',
            user_data_dir_arg,
        ])
    output_urls = json.loads(output)
    self.assertEqual(len(output_urls), 1)

    # The URL of the new tab can be one of the following:
    # - https://www.google.com/_/chrome/newtab?ie=UTF-8
    # - chrome://newtab
    # - chrome-search://local-ntp/local-ntp.html
    self.assertTrue('/newtab' in output_urls[0] or
                    'local-ntp.html' in output_urls[0])

  @test
  def test_OpenListOfUrls(self):
    logging.info('RestoreOnStartup is set to Open a list of URLs')

    self.SetPolicy('win2012-dc', 'RestoreOnStartup', 4, 'DWORD')
    urls_to_open = ['https://www.wikipedia.org/']
    for i in range(len(urls_to_open)):
      self.SetPolicy('win2012-dc', r'RestoreOnStartupURLs\%s' % (i + 1),
                     '"%s"' % urls_to_open[i], 'String')

    self.RunCommand('client2012', 'gpupdate /force')
    dir = os.path.dirname(os.path.abspath(__file__))
    user_data_dir_arg = r'--user_data_dir=c:\temp\user3'
    urls = ['https://www.cnn.com/', 'https://www.youtube.com/']
    list.sort(urls)

    # start Chrome and open several URLs.
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'restore_on_startup_webdriver_test.py'),
        [
            '--action=open_urls',
            user_data_dir_arg,
        ] + ['--urls=%s' % url for url in urls])
    output_urls = json.loads(output)
    self.assertEqual(urls, output_urls)

    # start Chrome. Urls specified by RestoreOnStartupURLs should be opened
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'restore_on_startup_webdriver_test.py'),
        [
            '--action=start_chrome',
            user_data_dir_arg,
        ])
    output_urls = json.loads(output)
    self.assertEqual(urls_to_open, output_urls)
