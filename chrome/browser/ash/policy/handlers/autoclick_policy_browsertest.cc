// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace policy {

class AutoclickPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(AutoclickPolicyTest, AutoclickEnabled) {
  // Verifies that the autoclick accessibility feature can be controlled through
  // policy.
  ash::AccessibilityManager* accessibility_manager =
      ash::AccessibilityManager::Get();

  accessibility_manager->EnableAutoclick(false);
  // Verify that the autoclick is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  // Manually enable the autoclick.
  accessibility_manager->EnableAutoclick(true);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kAutoclickEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  // Verify that the autoclick cannot be enabled manually anymore.
  accessibility_manager->EnableAutoclick(true);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  policies.Set(key::kAutoclickEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Verify that the autoclick cannot be disabled manually anymore.
  accessibility_manager->EnableAutoclick(false);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Verify that no confirmation dialog has been shown.
  EXPECT_FALSE(accessibility_manager->IsDisableAutoclickDialogVisibleForTest());
}

}  // namespace policy
