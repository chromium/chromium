// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_TEST_CROS_APPS_APITEST_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_TEST_CROS_APPS_APITEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

// Base test class used for writing ChromeOS App API tests. Supports running
// in both Ash and Lacros.
class CrosAppsApiTest : public InProcessBrowserTest {
 public:
  CrosAppsApiTest();

  CrosAppsApiTest(const CrosAppsApiTest&) = delete;
  CrosAppsApiTest& operator=(const CrosAppsApiTest&) = delete;

  // InProcessBrowserTest
  ~CrosAppsApiTest() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_TEST_CROS_APPS_APITEST_H_
