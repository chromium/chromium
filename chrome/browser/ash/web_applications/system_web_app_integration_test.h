// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_INTEGRATION_TEST_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_INTEGRATION_TEST_H_

#include <string>

#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "url/gurl.h"

namespace web_app {
enum class SystemAppType;
}

class Profile;

// Test harness for how ChromeOS System Web Apps integrate with the System Web
// App platform.
class SystemWebAppIntegrationTest
    : public web_app::SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppIntegrationTest();
  ~SystemWebAppIntegrationTest() override;

  // Runs basic tests on a System Web App. E.g. ensures it exists, and
  // loads/navigates with an expected title that matches the manifest app name.
  void ExpectSystemWebAppValid(web_app::SystemAppType app_type,
                               const GURL& url,
                               const std::string& title);

  // Helper to obtain browser()->profile().
  Profile* profile();
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_INTEGRATION_TEST_H_
