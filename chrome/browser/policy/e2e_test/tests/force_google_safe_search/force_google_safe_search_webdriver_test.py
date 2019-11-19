# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait

import test_util

driver = test_util.create_chrome_webdriver()
driver.get('http://www.google.com/xhtml')

# wait for page to be loaded
wait = WebDriverWait(driver, 10)
wait.until(EC.visibility_of_element_located((By.NAME, 'q')))

search_box = driver.find_element_by_name('q')
search_box.send_keys('searchTerm')
search_box.submit()

# wait for the search result page to be loaded
wait.until(EC.visibility_of_element_located((By.ID, 'search')))

print driver.current_url
driver.quit()
