// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace policy {

enum class WebSQLAccessValue {
  kUnset,
  kEnabled,
  kDisabled,
};

class WebSQLAccessTest : public testing::WithParamInterface<WebSQLAccessValue>,
                         public PolicyTest {
 protected:
  WebSQLAccessTest() {
    scoped_feature_list_.InitAndDisableFeature(blink::features::kWebSQLAccess);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() == WebSQLAccessValue::kUnset) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kWebSQLAccess,
              base::Value(GetParam() == WebSQLAccessValue::kEnabled));
    UpdateProviderPolicy(policies);
  }

  void AssertWebSQLAccessEnabled(bool enabled) {
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL("/empty.html"));
    ASSERT_TRUE(NavigateToUrl(url, this));

    content::DOMMessageQueue message_queue(
        chrome_test_utils::GetActiveWebContents(this));
    content::ExecuteScriptAsync(
        chrome_test_utils::GetActiveWebContents(this),
        "window.domAutomationController.send('openDatabase' in window)");
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(enabled ? "true" : "false", message);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebSQLAccessTest, Test) {
  bool expected_enabled =
      GetParam() == WebSQLAccessValue::kEnabled ? true : false;
  AssertWebSQLAccessEnabled(expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebSQLAccessTest,
    ::testing::Values(WebSQLAccessValue::kUnset,
                      WebSQLAccessValue::kEnabled,
                      WebSQLAccessValue::kDisabled));

}  // namespace policy
