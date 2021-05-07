// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "components/safe_browsing/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

// The interaction tests rely on the safe-browsing database.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

}  // namespace

// Test suite to test the interaction between Safe Browsing blocklist, Omaha
// attributes blocklist and user action. These tests verify that the extension
// is in the correct extension set under different circumstances.
class BlocklistStatesInteractionUnitTest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    InitializeGoodInstalledExtensionService();
    test_blocklist_.Attach(service()->blocklist_);
    service()->Init();
  }

 protected:
  void SetSafeBrowsingBlocklistStateForExtension(
      const std::string& extension_id,
      BlocklistState state) {
    // Reset cache in blocklist to make sure the latest blocklist state is
    // fetched.
    service()->blocklist_->ResetBlocklistStateCacheForTest();
    test_blocklist_.SetBlocklistState(extension_id, state, true);
    task_environment()->RunUntilIdle();
  }

  void SetOmahaBlocklistStateForExtension(const std::string& extension_id,
                                          const std::string& omaha_attribute,
                                          bool value) {
    base::Value attributes(base::Value::Type::DICTIONARY);
    attributes.SetBoolKey(omaha_attribute, value);
    service()->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
  }

 private:
  TestBlocklist test_blocklist_;
};

// 1. The extension is added to the Safe Browsing blocklist with
// BLOCKLISTED_MALWARE state.
// 2. The extension is added to the Omaha attribute blocklist with _malware
// attribute.
// 3. The extension is removed from the Safe Browsing blocklist.
// 4. The extension is removed from the Omaha attribute blocklist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingMalwareThenOmahaAttributeMalware) {
  const ExtensionSet& blocklisted_extensions =
      registry()->blocklisted_extensions();
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", true);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // kTestExtensionId should be kept in `blocklisted_extensions` because it is
  // still in the Omaha attribute blocklist.
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  // kTestExtensionId should be removed from the `blocklisted_extensions`.
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));
}

// 1. The extension is added to the Omaha attribute blocklist with _malware
// attribute.
// 2. The extension is added to the Safe Browsing blocklist with
// BLOCKLISTED_MALWARE state.
// 3. The extension is removed from the Omaha attribute blocklist.
// 4. The extension is removed from the Safe Browsing blocklist.
TEST_F(BlocklistStatesInteractionUnitTest,
       OmahaAttributeMalwareThenSafeBrowsingMalware) {
  const ExtensionSet& blocklisted_extensions =
      registry()->blocklisted_extensions();
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", true);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  // TODO(crbug.com/1193695): Ideally this should be true because the extension
  // is still in the Safe Browsing blocklist.
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));

  // The extension is added back to the blocklist after the Safe Browsing
  // blocklist is refreshed.
  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_MALWARE);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  // The extension should be kept in the `blocklisted_extensions` even if the
  // Omaha attribute is still false.
  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // kTestExtensionId should be removed from `blocklisted_extensions`.
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));
}

// 1. The extension is added to the Safe Browsing greylist with
// BLOCKLISTED_POTENTIALLY_UNWANTED state.
// 2. The extension is added to the Omaha attribute blocklist with _malware
// attribute.
// 3. The extension is removed from the Omaha attribute blocklist.
// 4. The extension is removed from the Safe Browsing greylist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingUwSThenOmahaAttributeMalware) {
  const ExtensionSet& blocklisted_extensions =
      registry()->blocklisted_extensions();
  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_POTENTIALLY_UNWANTED);
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", true);
  EXPECT_TRUE(blocklisted_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));

  SetOmahaBlocklistStateForExtension(kTestExtensionId, "_malware", false);
  EXPECT_FALSE(blocklisted_extensions.Contains(kTestExtensionId));
  // The extension should be kept disabled because it's still in the Safe
  // Browsing greylist.
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
}

// 1. The extension is added to the Safe Browsing greylist with
// BLOCKLISTED_CWS_POLICY_VIOLATION state.
// 2. The extension is added to the Omaha attribute greylist with
// _policy_violation attribute.
// 3. The extension is removed from the Safe Browsing greylist.
// 4. The extension is removed from the Omaha attribute greylist.
TEST_F(BlocklistStatesInteractionUnitTest,
       SafeBrowsingPolicyViolationThenOmahaAttributePolicyViolation) {
  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId,
                                            BLOCKLISTED_CWS_POLICY_VIOLATION);
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  // TODO(crbug.com/1180996): Call SetOmahaBlocklistStateForExtension directly
  // once we start to consume the _policy_violation attribute.
  blocklist_prefs::AddOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      ExtensionPrefs::Get(profile()));
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  SetSafeBrowsingBlocklistStateForExtension(kTestExtensionId, NOT_BLOCKLISTED);
  // The extension should be kept disabled because it's still in the Omaha
  // attribute greylist.
  EXPECT_FALSE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));

  // TODO(crbug.com/1180996): Call SetOmahaBlocklistStateForExtension directly
  // once we start to consume the _policy_violation attribute.
  blocklist_prefs::RemoveOmahaBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      ExtensionPrefs::Get(profile()));
  service()->ClearGreylistedAcknowledgedStateAndMaybeReenable(kTestExtensionId);
  EXPECT_TRUE(enabled_extensions.Contains(kTestExtensionId));
  EXPECT_FALSE(ExtensionPrefs::Get(profile())->HasDisableReason(
      kTestExtensionId, disable_reason::DISABLE_GREYLIST));
}
#endif

}  // namespace extensions
