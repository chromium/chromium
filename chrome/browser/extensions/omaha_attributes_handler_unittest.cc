// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/omaha_attributes_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_state_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

}  // namespace

// Test suite to test Omaha attribute handler.
class OmahaAttributesHandlerUnitTest : public ExtensionServiceTestBase {
 public:
  OmahaAttributesHandlerUnitTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {extensions_features::kDisablePolicyViolationExtensionsRemotely,
         extensions_features::kDisablePotentiallyUwsExtensionsRemotely},
        /*disabled_features=*/{});
  }
};

TEST_F(OmahaAttributesHandlerUnitTest, LogPolicyViolationUWSMetrics) {
  base::HistogramTester histograms;
  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_policy_violation", true);
  attributes.SetBoolKey("_potentially_uws", true);
  InitializeEmptyExtensionService();

  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  histograms.ExpectBucketCount(
      "Extensions.ExtensionDisabledRemotely2",
      /*sample=*/ExtensionUpdateCheckDataKey::kPotentiallyUWS,
      /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionAddDisabledRemotelyReason2",
      /*sample=*/ExtensionUpdateCheckDataKey::kPotentiallyUWS,
      /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionDisabledRemotely2",
      /*sample=*/ExtensionUpdateCheckDataKey::kPolicyViolation,
      /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionAddDisabledRemotelyReason2",
      /*sample=*/ExtensionUpdateCheckDataKey::kPolicyViolation,
      /*expected_count=*/1);
}

TEST_F(OmahaAttributesHandlerUnitTest, DisableRemotelyForPolicyViolation) {
  base::HistogramTester histograms;
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_policy_violation", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      prefs));

  // Remove extensions from greylist.
  attributes.SetBoolKey("_policy_violation", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension is re-enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      prefs));
  histograms.ExpectBucketCount(
      "Extensions.ExtensionReenabledRemotelyForPolicyViolation",
      /*sample=*/1,
      /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionReenabledRemotelyForPotentiallyUWS",
      /*sample=*/1,
      /*expected_count=*/0);
}

TEST_F(OmahaAttributesHandlerUnitTest, DisableRemotelyForPotentiallyUws) {
  base::HistogramTester histograms;
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_potentially_uws", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      prefs));

  // Remove extensions from greylist.
  attributes.SetBoolKey("_potentially_uws", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension is re-enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      prefs));
  histograms.ExpectBucketCount(
      "Extensions.ExtensionReenabledRemotelyForPotentiallyUWS",
      /*sample=*/1,
      /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionReenabledRemotelyForPolicyViolation",
      /*sample=*/1,
      /*expected_count=*/0);
}

TEST_F(OmahaAttributesHandlerUnitTest, MultipleGreylistStates) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_policy_violation", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  // Now user enables kTestExtensionId.
  service()->EnableExtension(kTestExtensionId);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  // Another greylist state is added to Omaha attribute.
  attributes.SetBoolKey("_potentially_uws", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension should be disabled again.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  // Remove extensions from the first greylist state.
  attributes.SetBoolKey("_policy_violation", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension should still be disabled, because it is still in the
  // potentially unwanted state.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      prefs));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      prefs));

  // Remove the other greylist state.
  attributes.SetBoolKey("_potentially_uws", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension is re-enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      prefs));
}

TEST_F(OmahaAttributesHandlerUnitTest, KeepDisabledWhenMalwareRemoved) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_malware", true);
  attributes.SetBoolKey("_policy_violation", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));
  EXPECT_EQ(disable_reason::DISABLE_REMOTELY_FOR_MALWARE |
                disable_reason::DISABLE_GREYLIST,
            prefs->GetDisableReasons(kTestExtensionId));

  // Remove malware.
  attributes.SetBoolKey("_malware", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension is not enabled because the policy violation bit is not
  // cleared, but it is no longer blocklisted (instead just disabled).
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
}

// Test suite to test Omaha attribute handler when features are disabled.
class OmahaAttributesHandlerWithFeatureDisabledUnitTest
    : public ExtensionServiceTestBase {
 public:
  OmahaAttributesHandlerWithFeatureDisabledUnitTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/{
            extensions_features::kDisablePolicyViolationExtensionsRemotely,
            extensions_features::kDisablePotentiallyUwsExtensionsRemotely});
  }
};

TEST_F(OmahaAttributesHandlerWithFeatureDisabledUnitTest,
       DoNotDisableRemotelyWhenFlagsDisabled) {
  base::HistogramTester histograms;
  InitializeGoodInstalledExtensionService();
  service()->Init();

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_policy_violation", true);
  attributes.SetBoolKey("_potentially_uws", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // Since the flag is disabled, we don't expect the extension to be affected.
  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      ExtensionPrefs::Get(profile())));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      ExtensionPrefs::Get(profile())));
  // Histograms should not be logged when the flag is disabled.
  histograms.ExpectTotalCount("Extensions.ExtensionAddDisabledRemotelyReason2",
                              /*expected_count=*/0);
}

}  // namespace extensions
