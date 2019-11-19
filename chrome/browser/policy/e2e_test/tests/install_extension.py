# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from absl import app, flags
from selenium import webdriver

import test_util

FLAGS = flags.FLAGS

flags.DEFINE_string('url', None, 'The url to open in Chrome.')
flags.mark_flag_as_required('url')

flags.DEFINE_integer(
    'wait', 0,
    'How many seconds to wait between loading the page and printing the source.'
)

flags.DEFINE_bool('incognito', False,
                  'Set flag to open Chrome in incognito mode.')

flags.DEFINE_bool(
    'text_only', False,
    'Set flag to print only page text (defaults to full source).')


def main(argv):
  chrome_options = webdriver.ChromeOptions()

  if FLAGS.incognito:
    chrome_options.add_argument('incognito')

  #Always set useAutomationExtension as false to avoid failing launch Chrome
  #https://bugs.chromium.org/p/chromedriver/issues/detail?id=2930
  chrome_options.add_experimental_option("useAutomationExtension", False)

  driver = test_util.create_chrome_webdriver(chrome_options=chrome_options)
  driver.implicitly_wait(FLAGS.wait)
  driver.get(FLAGS.url)

  driver.find_element_by_xpath("//div[@aria-label='Add to Chrome']").click()
  if FLAGS.wait > 0:
    time.sleep(FLAGS.wait)

  if FLAGS.text_only:
    print driver.find_element_by_css_selector('html').text.encode('utf-8')
  else:
    print driver.page_source.encode('utf-8')

  driver.quit()


if __name__ == '__main__':
  app.run(main)
