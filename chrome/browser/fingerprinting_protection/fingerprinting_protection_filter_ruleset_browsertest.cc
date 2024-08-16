// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter {

class
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionDisabled
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionDisabled() {
    feature_list_.InitWithFeatureStates(
        {{fingerprinting_protection_filter::features::
              kEnableFingerprintingProtectionFilterInIncognito,
          false},
         {fingerprinting_protection_filter::features::
              kEnableFingerprintingProtectionFilter,
          false}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionDisabled,
    RulesetServiceNotCreated) {
  EXPECT_EQ(g_browser_process->fingerprinting_protection_ruleset_service(),
            nullptr);
}

class
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionEnabled
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionEnabled() {
    feature_list_.InitAndEnableFeature(
        fingerprinting_protection_filter::features::
            kEnableFingerprintingProtectionFilter);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionEnabled,
    RulesetServiceCreated) {
  subresource_filter::RulesetService* service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetRulesetDealer(), nullptr);
}

class
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionDisabledInIncognito
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionDisabledInIncognito() {
    feature_list_.InitWithFeatureStates(
        {{fingerprinting_protection_filter::features::
              kEnableFingerprintingProtectionFilterInIncognito,
          false},
         {fingerprinting_protection_filter::features::
              kEnableFingerprintingProtectionFilter,
          false}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionDisabledInIncognito,
    RulesetServiceNotCreated) {
  EXPECT_EQ(g_browser_process->fingerprinting_protection_ruleset_service(),
            nullptr);
}

class
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionEnabledInIncognito
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionEnabledInIncognito() {
    feature_list_.InitAndEnableFeature(
        fingerprinting_protection_filter::features::
            kEnableFingerprintingProtectionFilterInIncognito);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterRulesetBrowserTestFingerprintingProtectionEnabledInIncognito,
    RulesetServiceCreated) {
  subresource_filter::RulesetService* service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetRulesetDealer(), nullptr);
}

}  // namespace fingerprinting_protection_filter
