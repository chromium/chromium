// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/omaha_attributes_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/blocklist_extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_features.h"
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
    feature_list_.InitAndEnableFeature(
        extensions_features::kDisablePolicyViolationExtensionsRemotely);
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
      "Extensions.ExtensionAddDisabledRemotelyReason",
      /* sample */ ExtensionUpdateCheckDataKey::kPotentiallyUWS,
      /* expected_count */ 1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionAddDisabledRemotelyReason",
      /* sample */ ExtensionUpdateCheckDataKey::kPolicyViolation,
      /* expected_count */ 1);
}

TEST_F(OmahaAttributesHandlerUnitTest, DisableRemotelyForPolicyViolation) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry()->disabled_extensions();

  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_policy_violation", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(disabled_extensions.Contains(kTestExtensionId));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      prefs));
  EXPECT_EQ(disable_reason::DISABLE_GREYLIST,
            prefs->GetDisableReasons(kTestExtensionId));

  // Remove extensions from greylist.
  attributes.SetBoolKey("_policy_violation", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      ExtensionPrefs::Get(profile())));
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            prefs->GetDisableReasons(kTestExtensionId));

  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      prefs));

  // The extension is re-enabled.
  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(disabled_extensions.Contains(kTestExtensionId));
}

TEST_F(OmahaAttributesHandlerUnitTest, KeepDisabledWhenMalwareRemoved) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& blocklisted_extensions =
      registry()->blocklisted_extensions();

  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_malware", true);
  attributes.SetBoolKey("_policy_violation", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));
  EXPECT_EQ(disable_reason::DISABLE_REMOTELY_FOR_MALWARE |
                disable_reason::DISABLE_GREYLIST,
            prefs->GetDisableReasons(kTestExtensionId));

  // Remove malware.
  attributes.SetBoolKey("_malware", false);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // The extension is not enabled because the policy violation bit is not
  // cleared.
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));
  EXPECT_EQ(disable_reason::DISABLE_GREYLIST,
            prefs->GetDisableReasons(kTestExtensionId));
}

// Test suite to test Omaha attribute handler when features are disabled.
class OmahaAttributesHandlerWithFeatureDisabledUnitTest
    : public ExtensionServiceTestBase {
 public:
  OmahaAttributesHandlerWithFeatureDisabledUnitTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kDisablePolicyViolationExtensionsRemotely);
  }
};

TEST_F(OmahaAttributesHandlerWithFeatureDisabledUnitTest,
       DoNotDisableRemotelyWhenPolicyViolationFlagDisabled) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry()->disabled_extensions();

  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetBoolKey("_policy_violation", true);
  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  // Since the flag is disabled, we don't expect the extension to be affected.
  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(disabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      ExtensionPrefs::Get(profile())));
}

}  // namespace extensions
