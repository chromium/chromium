// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/chromeos/app_mode/app_session_browser_window_handler.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using policy::DeveloperToolsPolicyHandler::Availability::kAllowed;
using policy::DeveloperToolsPolicyHandler::Availability::kDisallowed;

namespace ash {
namespace {

// Test kiosk troubleshooting tools on web kiosk.
class KioskTroubleshootingToolsTest : public WebKioskBaseTest {
 public:
  KioskTroubleshootingToolsTest() = default;

  KioskTroubleshootingToolsTest(const KioskTroubleshootingToolsTest&) = delete;
  KioskTroubleshootingToolsTest& operator=(
      const KioskTroubleshootingToolsTest&) = delete;

  // TODO(b/269316430): once devtools window stops being created by default,
  // fix this browser test.
  void DisableDevTools() const {
    initial_browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDevToolsAvailability, static_cast<int>(kDisallowed));
  }

  void ExpectOpenDevTools() const {
    EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
    histogram.ExpectBucketCount(
        chromeos::kKioskNewBrowserWindowHistogram,
        chromeos::KioskBrowserWindowType::kOpenedDevToolsBrowser, 1);
    histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
  }

  void ExpectOnlyKioskAppOpen() const {
    // The initial browser should exist in the web kiosk session.
    EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  }

  Browser* initial_browser() const {
    return BrowserList::GetInstance()->get(0);
  }

 protected:
  base::HistogramTester histogram;
};

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       DevToolsBasicShowAndShutdown) {
  InitializeRegularOnlineKiosk();
  ExpectOnlyKioskAppOpen();

  initial_browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kKioskTroubleshootingToolsEnabled, true);

  DevToolsWindowTesting::OpenDevToolsWindowSync(initial_browser(),
                                                /* is_docked= */ false);
  ExpectOpenDevTools();

  // Shut down the session when kiosk troubleshooting tools get disabled.
  initial_browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kKioskTroubleshootingToolsEnabled, false);
  auto* app_session = WebKioskAppManager::Get()->app_session();
  EXPECT_TRUE(app_session->is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       DevToolsDefaultShowAndDisallowed) {
  InitializeRegularOnlineKiosk();
  ExpectOnlyKioskAppOpen();

  DisableDevTools();
  initial_browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kKioskTroubleshootingToolsEnabled, true);

  // Devtools are not enabled, but disabled by default.
  DevToolsWindowTesting::OpenDevToolsWindowSync(initial_browser(),
                                                /* is_docked= */ false);

  ExpectOnlyKioskAppOpen();
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       DevToolsDefaultTroubleshootingDisabled) {
  InitializeRegularOnlineKiosk();
  ExpectOnlyKioskAppOpen();

  DevToolsWindowTesting::OpenDevToolsWindowSync(initial_browser(),
                                                /* is_docked= */ false);
  ExpectOnlyKioskAppOpen();

  // Since the devtools are allowed, the devtools window is open, but
  // immediately gets closed, since the kiosk troubleshooting tools are
  // disabled by the kiosk policy.
  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kClosedRegularBrowser, 1);
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
}

}  // namespace
}  // namespace ash
