// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/pref_names.h"
#include "content/public/test/browser_test.h"

namespace policy {

class SharedClipboardPolicyTest : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(policy::key::kSharedClipboardEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(SharedClipboardPolicyTest, SharedClipboardEnabled) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kSharedClipboardEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSharedClipboardEnabled));
}

}  // namespace policy
