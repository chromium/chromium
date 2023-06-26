// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_prefs.h"

namespace policy {
class AccessibilityAndroidPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(AccessibilityAndroidPolicyTest,
                       AccessibilityPerformanceFilteringNotSet) {
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kAccessibilityPerformanceFilteringAllowed));

  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();
  EXPECT_TRUE(accessibility_state->IsPerformanceFilteringAllowed());
}

IN_PROC_BROWSER_TEST_F(AccessibilityAndroidPolicyTest,
                       AccessibilityPerformanceFilteringAllowed) {
  PolicyMap policies;
  policies.Set(key::kAccessibilityPerformanceFilteringAllowed,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      prefs::kAccessibilityPerformanceFilteringAllowed));

  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();
  EXPECT_FALSE(accessibility_state->IsPerformanceFilteringAllowed());
}

}  // namespace policy
