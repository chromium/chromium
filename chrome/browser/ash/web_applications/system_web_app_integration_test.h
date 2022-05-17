// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_INTEGRATION_TEST_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_INTEGRATION_TEST_H_

#include <string>

#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "url/gurl.h"

namespace ash {
enum class SystemWebAppType;
}

class Profile;

// Test harness for how ChromeOS System Web Apps integrate with the System Web
// App platform.
class SystemWebAppIntegrationTest
    : public web_app::SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppIntegrationTest();
  SystemWebAppIntegrationTest(const SystemWebAppIntegrationTest&) = delete;
  SystemWebAppIntegrationTest& operator=(const SystemWebAppIntegrationTest&) =
      delete;
  ~SystemWebAppIntegrationTest() override;

  // Runs basic tests on a System Web App. E.g. ensures it exists, and
  // loads/navigates with an expected title that matches the manifest app name.
  void ExpectSystemWebAppValid(ash::SystemWebAppType app_type,
                               const GURL& url,
                               const std::string& title);

  // Helper to obtain browser()->profile().
  Profile* profile();

  // Launch the given System App |type| with the given |file_path| as a launch
  // file, and wait for the application to finish loading.
  content::WebContents* LaunchAppWithFile(ash::SystemWebAppType type,
                                          const base::FilePath& file_path);

  // Launch the given System App |type| with the given |file_path| as a launch
  // file, without waiting for the application to finish loading.
  void LaunchAppWithFileWithoutWaiting(ash::SystemWebAppType type,
                                       const base::FilePath& file_path);
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_INTEGRATION_TEST_H_
