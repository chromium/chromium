# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import app
from selenium import webdriver

import test_util


def main(argv):
  testSite = "http://www.dummysoftware.com/popupdummy_testpage.html"
  options = webdriver.ChromeOptions()
  options.add_experimental_option('excludeSwitches', ['disable-popup-blocking'])
  driver = test_util.create_chrome_webdriver(chrome_options=options)
  driver.implicitly_wait(5)
  driver.get(testSite)
  handles = driver.window_handles
  print len(handles)
  driver.quit()


if __name__ == '__main__':
  app.run(main)
