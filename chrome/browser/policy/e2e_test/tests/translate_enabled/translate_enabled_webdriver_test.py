# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import test_util
from absl import app, flags
from pywinauto.application import Application
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

# A URL that is in a different language than our Chrome language.
URL = "https://zh.wikipedia.org/wiki/Chromium"

FLAGS = flags.FLAGS

flags.DEFINE_bool('incognito', False,
                  'Set flag to open Chrome in incognito mode.')


def main(argv):
  driver = test_util.create_chrome_webdriver(incognito=FLAGS.incognito)
  driver.get(URL)

  time.sleep(10)

  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')

  translatePopupVisible = False
  for desc in app.top_window().descendants():
    if 'Translate this page?' in desc.window_text():
      translatePopupVisible = True
      break

  if translatePopupVisible:
    print "TRUE"
  else:
    print "FALSE"

  driver.quit()


if __name__ == '__main__':
  app.run(main)
