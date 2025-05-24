// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/password_manager_blocklist_policy.h"

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {
IN_PROC_BROWSER_TEST_F(PolicyTest, ValidatePasswordManagerBlocklist) {
  // Validate default state.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetList(policy_prefs::kPasswordManagerBlocklist).empty());

  base::Value blocked(base::Value::Type::LIST);
  blocked.GetList().Append("s.xxx.com/a");
  PolicyMap policies;
  policies.Set(key::kPasswordManagerBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(blocked),
               nullptr);
  UpdateProviderPolicy(policies);

  // Validate after policy is updated.
  EXPECT_FALSE(prefs->GetList(policy_prefs::kPasswordManagerBlocklist).empty());
}
}  // namespace policy
