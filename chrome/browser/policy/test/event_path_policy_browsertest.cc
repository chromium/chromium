// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace policy {

enum class EventPathPolicyValue {
  kUnset,
  kEnabled,
  kDisabled,
};

class EventPathPolicyTest
    : public testing::WithParamInterface<EventPathPolicyValue>,
      public PolicyTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() == EventPathPolicyValue::kUnset)
      return;
    PolicyMap policies;
    SetPolicy(&policies, policy::key::kEventPathEnabled,
              base::Value(GetParam() == EventPathPolicyValue::kEnabled));
    provider_.UpdateChromePolicy(policies);
  }

  void AssertEventPathEnabled(bool enabled) {
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL("/empty.html"));
    ASSERT_TRUE(NavigateToUrl(url, this));

    content::DOMMessageQueue message_queue(
        chrome_test_utils::GetActiveWebContents(this));
    content::ExecuteScriptAsync(
        chrome_test_utils::GetActiveWebContents(this),
        "window.domAutomationController.send('path' in Event.prototype)");
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(enabled ? "true" : "false", message);
  }
};

IN_PROC_BROWSER_TEST_P(EventPathPolicyTest, Test) {
  bool expected_enabled;
  if (GetParam() == EventPathPolicyValue::kUnset) {
    // Otherwise, Event.path API is disabled by default.
    expected_enabled = false;
  } else {
    // If the EventPathEnabled policy is set, the Event.path API status should
    // follow the policy value.
    expected_enabled = GetParam() == EventPathPolicyValue::kEnabled;
  }
  AssertEventPathEnabled(expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    EventPathPolicyTest,
    ::testing::Values(EventPathPolicyValue::kUnset,
                      EventPathPolicyValue::kEnabled,
                      EventPathPolicyValue::kDisabled));

}  // namespace policy
