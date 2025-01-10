// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

using glic::prefs::kGlicEnabledByPolicy;

namespace policy {

class GlicPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PrefDisabledByPolicy) {
  // By default the pref should start off unmanaged and defaulted to enabled.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(kGlicEnabledByPolicy));
  EXPECT_TRUE(prefs->GetBoolean(kGlicEnabledByPolicy));

  // Verify that policy can force-disable Glic.
  PolicyMap policies;
  policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(kGlicEnabledByPolicy));
  EXPECT_FALSE(prefs->GetBoolean(kGlicEnabledByPolicy));

  // Verify the policy value cannot be overridden.
  prefs->SetBoolean(kGlicEnabledByPolicy, true);
  EXPECT_FALSE(prefs->GetBoolean(kGlicEnabledByPolicy));
}

}  // namespace policy
