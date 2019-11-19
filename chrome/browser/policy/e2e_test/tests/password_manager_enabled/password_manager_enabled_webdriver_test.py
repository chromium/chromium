# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import test_util
from absl import app


def getShadowDom(driver, root, selector):
  el = root.find_element_by_css_selector(selector)
  return driver.execute_script("return arguments[0].shadowRoot", el)


def getNestedShadowDom(driver, selectors):
  el = driver
  for selector in selectors:
    el = getShadowDom(driver, el, selector)
  return el


def main(argv):
  driver = test_util.create_chrome_webdriver()
  driver.get("chrome://settings/passwords")

  # The settings is nested within multiple shadow doms - extract it.
  el = getNestedShadowDom(driver, [
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-autofill-page", "passwords-section", "#passwordToggle"
  ])

  if el.find_element_by_css_selector("cr-toggle").get_attribute("checked"):
    print "TRUE"
  else:
    print "FALSE"

  driver.quit()


if __name__ == '__main__':
  app.run(main)
