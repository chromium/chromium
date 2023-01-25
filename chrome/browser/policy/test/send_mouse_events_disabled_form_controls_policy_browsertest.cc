// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SendMouseEventsdisabledFormControlsPolicyTest \
  DISABLED_SendMouseEventsdisabledFormControlsPolicyTest
#else
#define MAYBE_SendMouseEventsdisabledFormControlsPolicyTest SendMouseEventsdisabledFormControlsPolicyTest
#endif
// "All/DISABLED_SendMouseEventsdisabledFormControlsPolicyTest" does not work. We need
// "DISABLED_All/...". TODO(https://crbug.com/1096416) delete when fixed.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif

namespace policy {

enum class SendMouseEventsDisabledFormControlsPolicyValue {
  kUnset,
  kEnabled,
  kDisabled,
};

class MAYBE_SendMouseEventsDisabledFormControlsPolicyTest
    : public testing::WithParamInterface<
          SendMouseEventsDisabledFormControlsPolicyValue>,
      public PolicyTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() == SendMouseEventsDisabledFormControlsPolicyValue::kUnset)
      return;
    PolicyMap policies;
    SetPolicy(
        &policies, policy::key::kSendMouseEventsDisabledFormControlsEnabled,
        base::Value(GetParam() ==
                    SendMouseEventsDisabledFormControlsPolicyValue::kEnabled));
    provider_.UpdateChromePolicy(policies);
  }

  void AssertSendMouseEventsDisabledFormControlsEnabled(bool enabled) {
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL(
        "/sendmouseeventsdisabledformcontrols.html"));
    ASSERT_TRUE(NavigateToUrl(url, this));

    content::DOMMessageQueue message_queue(
        chrome_test_utils::GetActiveWebContents(this));

    // Wait for page to load for a bit, otherwise input events won't get
    // dispatched.
    content::ExecuteScriptAsync(
        chrome_test_utils::GetActiveWebContents(this),
        "window.loadDonePromise.then(() => "
        "window.domAutomationController.send('load is done'))");
    std::string load_message;
    ASSERT_TRUE(message_queue.WaitForMessage(&load_message));
    ASSERT_EQ("\"load is done\"", load_message);

    content::ExecuteScriptAsync(
        chrome_test_utils::GetActiveWebContents(this),
        "window.testDonePromise.then(() => "
        "window.domAutomationController.send(window.targetparentGotClick))");
    content::SimulateMouseClick(chrome_test_utils::GetActiveWebContents(this),
                                0, blink::WebMouseEvent::Button::kLeft);

    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(enabled ? "false" : "true", message);
  }
};

IN_PROC_BROWSER_TEST_P(MAYBE_SendMouseEventsDisabledFormControlsPolicyTest, Test) {
  bool expected_enabled =
      GetParam() == SendMouseEventsDisabledFormControlsPolicyValue::kEnabled;
  AssertSendMouseEventsDisabledFormControlsEnabled(expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    MAYBE_All,
    MAYBE_SendMouseEventsDisabledFormControlsPolicyTest,
    ::testing::Values(
        SendMouseEventsDisabledFormControlsPolicyValue::kEnabled,
        SendMouseEventsDisabledFormControlsPolicyValue::kDisabled));

}  // namespace policy
