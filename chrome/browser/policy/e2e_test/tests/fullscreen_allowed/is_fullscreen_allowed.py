# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import test_util
from absl import app
from pywinauto.application import Application


def main(argv):
  driver = test_util.create_chrome_webdriver()
  try:
    application = Application(backend="uia")
    application.connect(title_re='.*Chrome|.*Chromium')
    w = application.top_window()

    for desc in w.descendants():
      print "item: %s" % desc

    print "Closing info bar."
    container = w.child_window(best_match="Infobar Container")
    container.child_window(best_match="Close").click_input()

    print "Clicking on the Fullscreen button."
    button = w.child_window(title_re="^Chrom(e|ium)$", control_type="Button")
    button.click_input()
    w.child_window(best_match="Full screen").click_input()

    window_rect = w.rectangle()
    window_width = window_rect.width()
    window_height = window_rect.height()
    content_width = driver.execute_script("return window.innerWidth")
    content_height = driver.execute_script("return window.innerHeight")

    # The content area should be the same size as the full window.
    print "window_rect: %s" % window_rect
    print "window_width: %s" % window_width
    print "window_height: %s" % window_height
    print "content_width: %s" % content_width
    print "content_height: %s" % content_height

    fs = window_width == content_width and window_height == content_height
    print "FullscreenAllowed: %s" % fs
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
