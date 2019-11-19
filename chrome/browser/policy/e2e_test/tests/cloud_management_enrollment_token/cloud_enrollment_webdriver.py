# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from absl import app
import time
from selenium import webdriver
import test_util


def main(argv):
  options = webdriver.ChromeOptions()
  # Add option for enrolling to the dev DMServer
  options.add_argument(
      "device-management-url=https://crosman-qa.sandbox.google.com/devicemanagement/data/api"
  )
  os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"
  driver = test_util.create_chrome_webdriver(chrome_options=options)

  # Give some time for browser to enroll
  time.sleep(10)

  try:
    # Verify Policy status legend in chrome://policy page
    policy_url = "chrome://policy"
    driver.get(policy_url)
    driver.find_element_by_id('reload-policies').click
    print driver.find_element_by_class_name('legend').text
    print driver.find_element_by_class_name('machine-enrollment-name').text
    print driver.find_element_by_class_name('machine-enrollment-token').text
    print driver.find_element_by_class_name('status').text
  except Exception as error:
    print error
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
