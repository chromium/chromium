// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_allowlist.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kExtensionId1[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kExtensionId2[] = "hpiknbiabeeppbpihjehijgoemciehgk";

}  // namespace

// Test suite to test safe browsing allowlist enforcement.
//
// Features EnforceSafeBrowsingExtensionAllowlist and
// DisableMalwareExtensionsRemotely are enabled.
class ExtensionAllowlistUnitTest : public ExtensionServiceTestBase {
 public:
  ExtensionAllowlistUnitTest() {
    feature_list_.InitWithFeatures(
        {extensions_features::kEnforceSafeBrowsingExtensionAllowlist,
         extensions_features::kDisableMalwareExtensionsRemotely},
        {});
  }

 protected:
  // Creates a test extension service with 3 installed extensions.
  void CreateExtensionService(bool enhanced_protection_enabled) {
    InitializeGoodInstalledExtensionService();
    extension_prefs_ = ExtensionPrefs::Get(profile());

    if (enhanced_protection_enabled) {
      safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                          safe_browsing::ENHANCED_PROTECTION);
    }
  }

  void PerformActionBasedOnOmahaAttributes(const std::string& extension_id,
                                           bool is_malware,
                                           bool is_allowlisted) {
    base::Value attributes(base::Value::Type::DICTIONARY);
    if (is_malware)
      attributes.SetBoolKey("_malware", true);

    attributes.SetBoolKey("_esbAllowlist", is_allowlisted);

    service()->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
  }

  bool IsEnabled(const std::string& extension_id) {
    return registry()->enabled_extensions().Contains(extension_id);
  }

  bool IsDisabled(const std::string& extension_id) {
    return registry()->disabled_extensions().Contains(extension_id);
  }

  bool IsBlocklisted(const std::string& extension_id) {
    return registry()->blocklisted_extensions().Contains(extension_id);
  }

  ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  ExtensionPrefs* extension_prefs_;
};

TEST_F(ExtensionAllowlistUnitTest, AllowlistEnforcement) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  service()->Init();

  // On the first startup, the allowlist state for existing extensions will be
  // undefined.
  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  // A first update check will set the allowlist state. In this case, an
  // extension not in the allowlist will be disabled.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // A future update check can change the allowlist state. Here the extension is
  // now allowlisted and should be re-enabled.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  // When an extension is disabled remotely for malware and is not allowlisted,
  // it should have both disable reasons.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/true,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_REMOTELY_FOR_MALWARE |
                disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // A blocklisted item should not be allowlisted, but if the improbable
  // happens, the item should still be blocklisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/true,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_REMOTELY_FOR_MALWARE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // If the item is removed from the malware blocklist, it should stay disabled
  // if it's not allowlisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, DisabledItemStaysDisabledWhenAllowlisted) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  service()->Init();

  // Start with an extension disabled by user.
  service()->DisableExtension(kExtensionId1,
                              disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // Disable the extension with allowlist enforcement.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION |
                disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // The extension is allowlisted, but stays disabled by user action.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, EnforcementOnInit) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  // Start an extension not allowlisted and in an unenforced state, this can
  // happen if the 'EnforceSafeBrowsingExtensionAllowlist' feature was
  // previously disabled for this profile.
  extension_prefs()->SetExtensionAllowlistState(kExtensionId1,
                                                ALLOWLIST_NOT_ALLOWLISTED);

  // During initialization, the allowlist will be enforced for extensions not
  // allowlisted.
  service()->Init();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // The enforcement isn't done for extensions having an undefined allowlist
  // state.
  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsEnabled(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, EnhancedProtectionSettingChange) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/false);
  // Start with ESB off and one extension not allowlisted.
  extension_prefs()->SetExtensionAllowlistState(kExtensionId1,
                                                ALLOWLIST_NOT_ALLOWLISTED);

  // Since ESB is off, no enforcement will be done for extensions not
  // allowlisted.
  service()->Init();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));

  // Even if the enforcement is off, the allowlist state is still tracked when
  // receiving update check results.
  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsEnabled(kExtensionId2));

  // When ESB is enabled, the extension service will enforce all extensions with
  // `ALLOWLIST_NOT_ALLOWLISTED` state.
  safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                      safe_browsing::ENHANCED_PROTECTION);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));
  EXPECT_TRUE(IsDisabled(kExtensionId2));

  // If the ESB setting is turned off, the extensions are re-enabled.
  safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                      safe_browsing::STANDARD_PROTECTION);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsEnabled(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, ExtensionsNotAllowlistedThenBlocklisted) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  TestBlocklist test_blocklist;
  test_blocklist.Attach(service()->blocklist_);

  // Start with two not allowlisted extensions, the enforcement will be done
  // during `Init`.
  extension_prefs()->SetExtensionAllowlistState(kExtensionId1,
                                                ALLOWLIST_NOT_ALLOWLISTED);
  extension_prefs()->SetExtensionAllowlistState(kExtensionId2,
                                                ALLOWLIST_NOT_ALLOWLISTED);
  service()->Init();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsDisabled(kExtensionId2));

  // Then blocklist and greylist the two extensions respectively.
  test_blocklist.SetBlocklistState(kExtensionId1, BLOCKLISTED_MALWARE, true);
  test_blocklist.SetBlocklistState(kExtensionId2,
                                   BLOCKLISTED_POTENTIALLY_UNWANTED, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId2));
  EXPECT_EQ(disable_reason::DISABLE_GREYLIST |
                disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));
  EXPECT_TRUE(IsDisabled(kExtensionId2));

  // When the extensions are unblocklisted, the allowlist enforcement will still
  // be effective if the extensions are not allowlisted.
  test_blocklist.SetBlocklistState(kExtensionId1, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kExtensionId2, NOT_BLOCKLISTED, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId2));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));
  EXPECT_TRUE(IsDisabled(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, ExtensionsBlocklistedThenNotAllowlisted) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  TestBlocklist test_blocklist;
  test_blocklist.Attach(service()->blocklist_);

  service()->Init();

  // Blocklist and greylist the two extensions respectively.
  test_blocklist.SetBlocklistState(kExtensionId1, BLOCKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // The extension is then also disabled from allowlist enforcement.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BLOCKLISTED_MALWARE,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  // The disable reason is added even if the extension is already blocklisted.
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // When the extensions is unblocklisted, the allowlist enforcement will still
  // be effective if the extension is not allowlisted.
  test_blocklist.SetBlocklistState(kExtensionId1, NOT_BLOCKLISTED, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(NOT_BLOCKLISTED,
            extension_prefs()->GetExtensionBlocklistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, MissingAttributeAreIgnored) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  // Start with one extension allowlisted and another not allowlisted.
  extension_prefs()->SetExtensionAllowlistState(kExtensionId1,
                                                ALLOWLIST_ALLOWLISTED);
  extension_prefs()->SetExtensionAllowlistState(kExtensionId2,
                                                ALLOWLIST_NOT_ALLOWLISTED);

  // During initialization, the allowlist will be enforced for extensions not
  // allowlisted.
  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));

  // Simulate an update check with no custom attribute defined.
  base::Value attributes(base::Value::Type::DICTIONARY);
  service()->PerformActionBasedOnOmahaAttributes(kExtensionId1, attributes);
  service()->PerformActionBasedOnOmahaAttributes(kExtensionId2, attributes);

  // The undefined allowlist attributes should be ignored and the state should
  // remain unchanged.
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_prefs()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, NoEnforcementWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kEnforceSafeBrowsingExtensionAllowlist);

  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  extension_prefs()->SetExtensionAllowlistState(kExtensionId1,
                                                ALLOWLIST_NOT_ALLOWLISTED);
  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId1));
}

}  // namespace extensions
