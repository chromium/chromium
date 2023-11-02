// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace policy {

enum class PrefixedStorageInfoValue {
  kUnset,
  kEnabled,
  kDisabled,
};

class PrefixedStorageInfoTest
    : public testing::WithParamInterface<PrefixedStorageInfoValue>,
      public PolicyTest {
 protected:
  PrefixedStorageInfoTest() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kPrefixedStorageInfo);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() == PrefixedStorageInfoValue::kUnset)
      return;
    PolicyMap policies;
    SetPolicy(&policies, key::kPrefixedStorageInfoEnabled,
              base::Value(GetParam() == PrefixedStorageInfoValue::kEnabled));
    UpdateProviderPolicy(policies);
  }

  void AssertPrefixedStorageInfoEnabled(bool enabled) {
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL url(embedded_test_server()->GetURL("/empty.html"));
    ASSERT_TRUE(NavigateToUrl(url, this));

    content::DOMMessageQueue message_queue(
        chrome_test_utils::GetActiveWebContents(this));
    content::ExecuteScriptAsync(
        chrome_test_utils::GetActiveWebContents(this),
        "window.domAutomationController.send('webkitStorageInfo' in window)");
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(enabled ? "true" : "false", message);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PrefixedStorageInfoTest, Test) {
  bool expected_enabled =
      GetParam() == PrefixedStorageInfoValue::kEnabled ? true : false;
  AssertPrefixedStorageInfoEnabled(expected_enabled);
}

IN_PROC_BROWSER_TEST_P(PrefixedStorageInfoTest, TestDynamicRefresh) {
  // Reset policy that is already set in `SetUpInProcessBrowserTestFixture` and
  // verify policy is reflected without browser restart
  PolicyMap policies;
  SetPolicy(&policies, key::kPrefixedStorageInfoEnabled, base::Value(true));
  UpdateProviderPolicy(policies);

  // Crash to start new renderer process.
  auto* tab = chrome_test_utils::GetActiveWebContents(this);
  content::RenderProcessHost* process =
      tab->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);

  // Reload tab.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));

  AssertPrefixedStorageInfoEnabled(true);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PrefixedStorageInfoTest,
    ::testing::Values(PrefixedStorageInfoValue::kUnset,
                      PrefixedStorageInfoValue::kEnabled,
                      PrefixedStorageInfoValue::kDisabled));

}  // namespace policy
