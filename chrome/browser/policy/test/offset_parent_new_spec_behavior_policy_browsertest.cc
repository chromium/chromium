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

enum class OffsetParentNewSpecBehaviorPolicyValue {
  kUnset,
  kEnabled,
  kDisabled,
};

class OffsetParentNewSpecBehaviorPolicyTest
    : public testing::WithParamInterface<
          OffsetParentNewSpecBehaviorPolicyValue>,
      public PolicyTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() == OffsetParentNewSpecBehaviorPolicyValue::kUnset)
      return;
    PolicyMap policies;
    SetPolicy(&policies, policy::key::kOffsetParentNewSpecBehaviorEnabled,
              base::Value(GetParam() ==
                          OffsetParentNewSpecBehaviorPolicyValue::kEnabled));
    provider_.UpdateChromePolicy(policies);
  }

  void AssertOffsetParentNewSpecBehaviorEnabled(bool enabled) {
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL("/offsetparent.html"));
    ASSERT_TRUE(NavigateToUrl(url, this));

    content::DOMMessageQueue message_queue(
        chrome_test_utils::GetActiveWebContents(this));
    content::ExecuteScriptAsync(
        chrome_test_utils::GetActiveWebContents(this),
        "window.domAutomationController.send(window.offsetParentId)");
    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(enabled ? "\"host\"" : "\"shadowchild\"", message);
  }
};

IN_PROC_BROWSER_TEST_P(OffsetParentNewSpecBehaviorPolicyTest, Test) {
  bool expected_enabled;
  if (GetParam() == OffsetParentNewSpecBehaviorPolicyValue::kUnset) {
    // OffsetParentNewSpecBehavior should be enabled by default.
    expected_enabled = true;
  } else {
    expected_enabled =
        GetParam() == OffsetParentNewSpecBehaviorPolicyValue::kEnabled;
  }
  AssertOffsetParentNewSpecBehaviorEnabled(expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    OffsetParentNewSpecBehaviorPolicyTest,
    ::testing::Values(OffsetParentNewSpecBehaviorPolicyValue::kUnset,
                      OffsetParentNewSpecBehaviorPolicyValue::kEnabled,
                      OffsetParentNewSpecBehaviorPolicyValue::kDisabled));

}  // namespace policy
