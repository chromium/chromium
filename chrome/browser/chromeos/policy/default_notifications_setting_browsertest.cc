// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/features.h"
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
  DefaultNotificationsSettingBrowserTest() = default;

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

  // Wait for 'settings-notifications-page' custom element to be defined.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      "customElements.whenDefined('settings-notifications-page')"));

  // The UI has 6 radio buttons in two groups, and are inside several layers of
  // shadow DOM. The buttons are:
  // [Never visible] (0) Sites can send notifications
  // (1) Sites can ask to send notifications
  // (2) Don't allow sites to send notifications
  //
  // (0) Collapse all requests
  // (1) Collapse unwanted requests
  // (2) Expand all requests
  // Query the checked and disabled state of the radio buttons.
  std::string kGetCategorySettingRadios =
      "let mainRadios = "
      "  document.querySelector('settings-ui').shadowRoot."
      "  querySelector('settings-main').shadowRoot."
      "  querySelector('settings-privacy-page-index').shadowRoot."
      "  querySelector('settings-notifications-page').shadowRoot."
      "  querySelector('settings-category-default-radio-group').shadowRoot."
      "  querySelectorAll('settings-collapse-radio-button');";
  std::string kGetCPSSRadios =
      "let cpssRadios = "
      "  document.querySelector('settings-ui').shadowRoot."
      "  querySelector('settings-main').shadowRoot."
      "  querySelector('settings-privacy-page-index').shadowRoot."
      "  querySelector('settings-notifications-page').shadowRoot."
      "  querySelectorAll('settings-collapse-radio-button');";
  std::string kGetRadiosChecked = kGetCategorySettingRadios + kGetCPSSRadios +
                                  "let radiosChecked = [];"
                                  "radiosChecked.push(mainRadios[0].checked);"
                                  "radiosChecked.push(mainRadios[1].checked);"
                                  "radiosChecked.push(mainRadios[2].checked);"
                                  "radiosChecked.push(cpssRadios[0].checked);"
                                  "radiosChecked.push(cpssRadios[1].checked);"
                                  "radiosChecked.push(cpssRadios[2].checked);"
                                  "radiosChecked;";
  base::ListValue radios_checked_list =
      content::EvalJs(web_contents, kGetRadiosChecked).TakeValue().TakeList();

  std::string kGetRadiosEnabled = kGetCategorySettingRadios + kGetCPSSRadios +
                                  "let radiosEnabled = [];"
                                  "radiosEnabled.push(!mainRadios[0].disabled);"
                                  "radiosEnabled.push(!mainRadios[1].disabled);"
                                  "radiosEnabled.push(!mainRadios[2].disabled);"
                                  "radiosEnabled.push(!cpssRadios[0].disabled);"
                                  "radiosEnabled.push(!cpssRadios[1].disabled);"
                                  "radiosEnabled.push(!cpssRadios[2].disabled);"
                                  "radiosEnabled;";
  base::ListValue radios_enabled_list =
      content::EvalJs(web_contents, kGetRadiosEnabled).TakeValue().TakeList();

  std::string kIsVisible =
      "function isVisible(element) {"
      "  let rect = element.getBoundingClientRect();"
      "  return rect.width * rect.height > 0;"
      "}";
  std::string kGetRadiosVisible =
      kGetCategorySettingRadios + kGetCPSSRadios + kIsVisible +
      "let radiosVisible = [];"
      "radiosVisible.push(isVisible(mainRadios[0]));"
      "radiosVisible.push(isVisible(mainRadios[1]));"
      "radiosVisible.push(isVisible(mainRadios[2]));"
      "radiosVisible.push(isVisible(cpssRadios[0]));"
      "radiosVisible.push(isVisible(cpssRadios[1]));"
      "radiosVisible.push(isVisible(cpssRadios[2]));"
      "radiosVisible;";

  base::ListValue radios_visible_list =
      content::EvalJs(web_contents, kGetRadiosVisible).TakeValue().TakeList();

  EXPECT_FALSE(radios_visible_list[0].GetBool());
  EXPECT_TRUE(radios_visible_list[1].GetBool());
  EXPECT_TRUE(radios_visible_list[2].GetBool());

  switch (GetParam()) {
    case 0:
      // Policy not set.
      EXPECT_TRUE(radios_checked_list[1].GetBool());
      EXPECT_TRUE(radios_enabled_list[1].GetBool());

      EXPECT_FALSE(radios_checked_list[2].GetBool());
      EXPECT_TRUE(radios_enabled_list[2].GetBool());

      EXPECT_FALSE(radios_checked_list[3].GetBool());
      EXPECT_TRUE(radios_enabled_list[3].GetBool());
      EXPECT_TRUE(radios_visible_list[3].GetBool());

      EXPECT_TRUE(radios_checked_list[4].GetBool());
      EXPECT_TRUE(radios_enabled_list[4].GetBool());
      EXPECT_TRUE(radios_visible_list[4].GetBool());

      EXPECT_FALSE(radios_checked_list[5].GetBool());
      EXPECT_TRUE(radios_enabled_list[5].GetBool());
      EXPECT_TRUE(radios_visible_list[5].GetBool());
      break;
    case 1:
      // Allow sites to show desktop notifications.
      EXPECT_FALSE(radios_checked_list[1].GetBool());
      EXPECT_FALSE(radios_enabled_list[1].GetBool());

      EXPECT_FALSE(radios_checked_list[2].GetBool());
      EXPECT_FALSE(radios_enabled_list[2].GetBool());

      EXPECT_FALSE(radios_visible_list[3].GetBool());
      EXPECT_FALSE(radios_visible_list[4].GetBool());
      EXPECT_FALSE(radios_visible_list[5].GetBool());
      break;
    case 2:
      // Don't allow sites to show desktop notifications.
      EXPECT_FALSE(radios_checked_list[1].GetBool());
      EXPECT_FALSE(radios_enabled_list[1].GetBool());

      EXPECT_TRUE(radios_checked_list[2].GetBool());
      EXPECT_FALSE(radios_enabled_list[2].GetBool());

      EXPECT_FALSE(radios_visible_list[3].GetBool());
      EXPECT_FALSE(radios_visible_list[4].GetBool());
      EXPECT_FALSE(radios_visible_list[5].GetBool());
      break;
    case 3:
      // Ask every time a site wants to show desktop notifications.
      EXPECT_TRUE(radios_checked_list[1].GetBool());
      EXPECT_FALSE(radios_enabled_list[1].GetBool());

      EXPECT_FALSE(radios_checked_list[2].GetBool());
      EXPECT_FALSE(radios_enabled_list[2].GetBool());

      EXPECT_FALSE(radios_checked_list[3].GetBool());
      EXPECT_FALSE(radios_enabled_list[3].GetBool());
      EXPECT_TRUE(radios_visible_list[3].GetBool());

      EXPECT_FALSE(radios_checked_list[4].GetBool());
      EXPECT_FALSE(radios_enabled_list[4].GetBool());
      EXPECT_TRUE(radios_visible_list[4].GetBool());

      EXPECT_TRUE(radios_checked_list[5].GetBool());
      EXPECT_FALSE(radios_enabled_list[5].GetBool());
      EXPECT_TRUE(radios_visible_list[5].GetBool());
      break;
  }
}

}  // namespace
