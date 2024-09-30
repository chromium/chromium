// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Tests the behavior of the DefaultNotificationsSetting policy, including the
// notification permission in JavaScript and the state of the browser settings
// UI.
// Contacts:
// * permissions-core@google.com
// * engedy@google.com - TL permissions team
// * gabormagda@google.com - Tast test author
// * jamescook@google.com - Ported Tast test to browser test
// Bug Component: "crbug:Internals>Permissions"
class DefaultNotificationsSettingBrowserTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<int> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    // Use param 0 to test the policy unset case.
    if (GetParam() != 0) {
      policy::PolicyMap policy_map;
      SetPolicy(&policy_map, policy::key::kDefaultNotificationsSetting,
                base::Value(GetParam()));
      UpdateProviderPolicy(policy_map);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(IntPolicy,
                         DefaultNotificationsSettingBrowserTest,
                         testing::Values(0, 1, 2, 3));

IN_PROC_BROWSER_TEST_P(DefaultNotificationsSettingBrowserTest, Policy) {
  // Load the browser settings notifications page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://settings/content/notifications")));

  // Query the notification permission state.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const char kGetPermissionJs[] = "Notification.permission";
  std::string permission =
      content::EvalJs(web_contents, kGetPermissionJs).ExtractString();
  switch (GetParam()) {
    case 0:
      // Policy not set.
      EXPECT_EQ(permission, "default");
      break;
    case 1:
      // Allow sites to show desktop notifications.
      EXPECT_EQ(permission, "granted");
      break;
    case 2:
      // Don't allow sites to show desktop notifications.
      EXPECT_EQ(permission, "denied");
      break;
    case 3:
      // Ask every time a site wants to show desktop notifications.
      EXPECT_EQ(permission, "default");
      break;
  }

  // The UI has 3 radio buttons which are inside several layers of shadow DOM.
  // The buttons are:
  // (0) Sites can ask to send notifications
  // (1) Use quieter messaging
  // (2) Don't allow sites to send notifications
  // Query the checked and disabled state of the radio buttons.
  std::string kGetRadios =
      "let radios = "
      "  document.querySelector('settings-ui').shadowRoot."
      "  querySelector('settings-main').shadowRoot."
      "  querySelector('settings-basic-page').shadowRoot."
      "  querySelector('settings-privacy-page').shadowRoot."
      "  querySelectorAll('cr-radio-button');";
  std::string kGetRadiosChecked = kGetRadios +
                                  "let radiosChecked = [];"
                                  "radiosChecked.push(radios[0].checked);"
                                  "radiosChecked.push(radios[1].checked);"
                                  "radiosChecked.push(radios[2].checked);"
                                  "radiosChecked;";
  base::Value radios_checked =
      content::EvalJs(web_contents, kGetRadiosChecked).ExtractList();
  ASSERT_TRUE(radios_checked.is_list());
  const base::Value::List& radios_checked_list = radios_checked.GetList();

  std::string kGetRadiosEnabled = kGetRadios +
                                  "let radiosEnabled = [];"
                                  "radiosEnabled.push(!radios[0].disabled);"
                                  "radiosEnabled.push(!radios[1].disabled);"
                                  "radiosEnabled.push(!radios[2].disabled);"
                                  "radiosEnabled;";
  base::Value radios_enabled =
      content::EvalJs(web_contents, kGetRadiosEnabled).ExtractList();
  ASSERT_TRUE(radios_enabled.is_list());
  const base::Value::List& radios_enabled_list = radios_enabled.GetList();

  switch (GetParam()) {
    case 0:
      // Policy not set.
      EXPECT_TRUE(radios_checked_list[0].GetBool());
      EXPECT_FALSE(radios_checked_list[1].GetBool());
      EXPECT_TRUE(radios_checked_list[2].GetBool());
      EXPECT_TRUE(radios_enabled_list[0].GetBool());
      EXPECT_TRUE(radios_enabled_list[1].GetBool());
      EXPECT_TRUE(radios_enabled_list[2].GetBool());
      break;
    case 1:
      // Allow sites to show desktop notifications.
      EXPECT_FALSE(radios_checked_list[0].GetBool());
      EXPECT_FALSE(radios_checked_list[1].GetBool());
      EXPECT_FALSE(radios_checked_list[2].GetBool());
      EXPECT_TRUE(radios_enabled_list[0].GetBool());
      EXPECT_TRUE(radios_enabled_list[1].GetBool());
      EXPECT_TRUE(radios_enabled_list[2].GetBool());
      break;
    case 2:
      // Don't allow sites to show desktop notifications.
      EXPECT_FALSE(radios_checked_list[0].GetBool());
      EXPECT_FALSE(radios_checked_list[1].GetBool());
      EXPECT_FALSE(radios_checked_list[2].GetBool());
      EXPECT_TRUE(radios_enabled_list[0].GetBool());
      EXPECT_TRUE(radios_enabled_list[1].GetBool());
      EXPECT_TRUE(radios_enabled_list[2].GetBool());
      break;
    case 3:
      // Ask every time a site wants to show desktop notifications.
      EXPECT_TRUE(radios_checked_list[0].GetBool());
      EXPECT_FALSE(radios_checked_list[1].GetBool());
      EXPECT_FALSE(radios_checked_list[2].GetBool());
      EXPECT_TRUE(radios_enabled_list[0].GetBool());
      EXPECT_TRUE(radios_enabled_list[1].GetBool());
      EXPECT_TRUE(radios_enabled_list[2].GetBool());
      break;
  }
}

}  // namespace
