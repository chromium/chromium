// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CROS_APPS_API_CROS_APPS_API_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_LACROS_CROS_APPS_API_CROS_APPS_API_BROWSERTEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

class CrosAppsApiBrowserTestBase : public InProcessBrowserTest {
 public:
  CrosAppsApiBrowserTestBase();

  CrosAppsApiBrowserTestBase(const CrosAppsApiBrowserTestBase&) = delete;
  CrosAppsApiBrowserTestBase& operator=(const CrosAppsApiBrowserTestBase&) =
      delete;

  // InProcessBrowserTest
  ~CrosAppsApiBrowserTestBase() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_LACROS_CROS_APPS_API_CROS_APPS_API_BROWSERTEST_BASE_H_
