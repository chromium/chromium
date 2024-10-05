// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_allowlist.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kExtensionId1[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kExtensionId2[] = "hpiknbiabeeppbpihjehijgoemciehgk";
constexpr char kExtensionId3[] = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
constexpr char kInstalledCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

using ManagementPrefUpdater = ExtensionManagementPrefUpdater<
    sync_preferences::TestingPrefServiceSyncable>;

}  // namespace

// Test suite to test safe browsing allowlist enforcement.
//
// Features EnforceSafeBrowsingExtensionAllowlist and
// DisableMalwareExtensionsRemotely are enabled.
class ExtensionAllowlistUnitTestBase : public ExtensionServiceTestBase {
 protected:
  // Creates a test extension service with 3 installed extensions.
  void CreateExtensionService(bool enhanced_protection_enabled) {
    ExtensionServiceInitParams params;
    ASSERT_TRUE(
        params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
    InitializeExtensionService(std::move(params));
    extension_prefs_ = ExtensionPrefs::Get(profile());

    if (enhanced_protection_enabled) {
      safe_browsing::SetSafeBrowsingState(
          profile()->GetPrefs(),
          safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
    }
  }

  void CreateEmptyExtensionService() {
    InitializeExtensionService(ExtensionServiceInitParams());
    extension_prefs_ = ExtensionPrefs::Get(profile());
    safe_browsing::SetSafeBrowsingState(
        profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  }

  void PerformActionBasedOnOmahaAttributes(const ExtensionId& extension_id,
                                           bool is_malware,
                                           bool is_allowlisted) {
    auto attributes = base::Value::Dict().Set("_esbAllowlist", is_allowlisted);
    if (is_malware) {
      attributes.Set("_malware", true);
    }

    service()->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
  }

  bool IsEnabled(const ExtensionId& extension_id) {
    return registry()->enabled_extensions().Contains(extension_id);
  }

  bool IsDisabled(const ExtensionId& extension_id) {
    return registry()->disabled_extensions().Contains(extension_id);
  }

  bool IsBlocklisted(const ExtensionId& extension_id) {
    return registry()->blocklisted_extensions().Contains(extension_id);
  }

  ExtensionAllowlist* allowlist() { return service()->allowlist(); }

  ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  raw_ptr<ExtensionPrefs> extension_prefs_;
};

class ExtensionAllowlistUnitTest : public ExtensionAllowlistUnitTestBase {
 public:
  ExtensionAllowlistUnitTest() {
    feature_list_.InitWithFeatures(
        {extensions_features::kSafeBrowsingCrxAllowlistShowWarnings,
         extensions_features::kSafeBrowsingCrxAllowlistAutoDisable},
        {});
  }
};

TEST_F(ExtensionAllowlistUnitTest, AllowlistEnforcement) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  service()->Init();

  // On the first startup, the allowlist state for existing extensions will be
  // undefined.
  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  // A first update check will set the allowlist state. In this case, an
  // extension not in the allowlist will be disabled.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // A future update check can change the allowlist state. Here the extension is
  // now allowlisted and should be re-enabled.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  // When an extension is disabled remotely for malware and is not allowlisted,
  // it should have both disable reasons.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/true,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetExtensionBlocklistState(kExtensionId1,
                                                        extension_prefs()));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kExtensionId1, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs()));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // A blocklisted item should not be allowlisted, but if the improbable
  // happens, the item should still be blocklisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/true,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetExtensionBlocklistState(kExtensionId1,
                                                        extension_prefs()));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      kExtensionId1, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs()));
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // If the item is removed from the malware blocklist, it should stay disabled
  // if it's not allowlisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, DisabledReasonResetWhenBlocklisted) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  service()->Init();

  // The disabled reason should be set if an extension is not in the allowlist.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));

  // The extension is added to the blocklist.
  service()->BlocklistExtensionForTest(kExtensionId1);

  // A blocklisted item should not be allowlisted, but if the improbable
  // happens, the item should still be blocklisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/true);
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));
  // The disabled reason should be reset because the extension is in the
  // allowlist.
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            extension_prefs()->GetDisableReasons(kExtensionId1));
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
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION |
                disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // The extension is allowlisted, but stays disabled by user action.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
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
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);

  // During initialization, the allowlist will be enforced for extensions not
  // allowlisted.
  service()->Init();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  // The enforcement isn't done for extensions having an undefined allowlist
  // state.
  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsEnabled(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, EnhancedProtectionSettingChange) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/false);
  // Start with ESB off and one extension not allowlisted.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);

  // Since ESB is off, no enforcement will be done for extensions not
  // allowlisted.
  service()->Init();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));

  // Even if the enforcement is off, the allowlist state is still tracked when
  // receiving update check results.
  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsEnabled(kExtensionId2));

  // When ESB is enabled, the extension service will enforce all extensions with
  // `ALLOWLIST_NOT_ALLOWLISTED` state.
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));
  EXPECT_TRUE(IsDisabled(kExtensionId2));

  // If the ESB setting is turned off, the extensions are re-enabled.
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsEnabled(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, ExtensionsNotAllowlistedThenBlocklisted) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);
  TestBlocklist test_blocklist;
  test_blocklist.Attach(service()->blocklist_);

  // Start with two not allowlisted extensions, the enforcement will be done
  // during `Init`.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistState(kExtensionId2,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  service()->Init();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_TRUE(IsDisabled(kExtensionId2));

  // Then blocklist and greylist the two extensions respectively.
  test_blocklist.SetBlocklistState(kExtensionId1, BLOCKLISTED_MALWARE, true);
  test_blocklist.SetBlocklistState(kExtensionId2,
                                   BLOCKLISTED_POTENTIALLY_UNWANTED, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId2, extension_prefs()));
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
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId2, extension_prefs()));
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
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // The extension is then also disabled from allowlist enforcement.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
  // The disable reason is added even if the extension is already blocklisted.
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsBlocklisted(kExtensionId1));

  // When the extensions is unblocklisted, the allowlist enforcement will still
  // be effective if the extension is not allowlisted.
  test_blocklist.SetBlocklistState(kExtensionId1, NOT_BLOCKLISTED, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId1, extension_prefs()));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_TRUE(IsDisabled(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, MissingAttributeAreIgnored) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  // Start with one extension allowlisted and another not allowlisted.
  allowlist()->SetExtensionAllowlistState(kExtensionId1, ALLOWLIST_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistState(kExtensionId2,
                                          ALLOWLIST_NOT_ALLOWLISTED);

  // During initialization, the allowlist will be enforced for extensions not
  // allowlisted.
  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));

  // Simulate an update check with no custom attribute defined.
  base::Value::Dict attributes;
  service()->PerformActionBasedOnOmahaAttributes(kExtensionId1, attributes);
  service()->PerformActionBasedOnOmahaAttributes(kExtensionId2, attributes);

  // The undefined allowlist attributes should be ignored and the state should
  // remain unchanged.
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED,
            extension_prefs()->GetDisableReasons(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, AcknowledgeNeededOnEnforcement) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NONE,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));

  // Make the extension not allowlisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  // Expect the acknowledge state to change appropriately.
  EXPECT_TRUE(IsDisabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NEEDED,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, AcknowledgeNotNeededIfAlreadyDisabled) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  service()->Init();
  service()->DisableExtension(kExtensionId1,
                              disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(IsDisabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NONE,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));

  // Make the extension not allowlisted.
  PerformActionBasedOnOmahaAttributes(kExtensionId1,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  // There is no need for acknowledge if the extension was already disabled.
  EXPECT_TRUE(IsDisabled(kExtensionId1));
  EXPECT_EQ(disable_reason::DISABLE_NOT_ALLOWLISTED |
                disable_reason::DISABLE_USER_ACTION,
            extension_prefs()->GetDisableReasons(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NONE,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest,
       AcknowledgeStateIsSetWhenExtensionIsReenabled) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NONE,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));

  // Start with a not allowlisted extension.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);

  // The enforcement on init should disable the extension.
  service()->Init();
  EXPECT_TRUE(IsDisabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NEEDED,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));

  // Re-enable the extension.
  service()->EnableExtension(kExtensionId1);

  // The extensions should now be marked with
  // `ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER'.
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId1));
}

TEST_F(ExtensionAllowlistUnitTest, ReenabledExtensionsAreNotReenforced) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  // Start with a not allowlisted extension that was re-enabled by user.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistAcknowledgeState(
      kExtensionId1, ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);

  // And an extension that became allowlisted after it was re-enabled by user.
  allowlist()->SetExtensionAllowlistState(kExtensionId2, ALLOWLIST_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistAcknowledgeState(
      kExtensionId2, ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);

  service()->Init();
  // Even though ExtensionId1 is not allowlisted, it should stay enabled because
  // it was re-enabled by user.
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  // Assert that ExtensionId2 is enabled before testing the allowlist state
  // change.
  EXPECT_TRUE(IsEnabled(kExtensionId2));

  // If `kExtensionId2` becomes not allowlisted again, it should stay enabled
  // because the user already chose to re-enable it in the past.
  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, TurnOffEnhancedProtection) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  // Start with 3 not allowlisted extensions.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistState(kExtensionId2,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistState(kExtensionId3,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistAcknowledgeState(
      kExtensionId3, ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);

  // They should get disabled by allowlist enforcement and have their
  // acknowledge state set (except the extension re-enabled by user).
  service()->Init();
  EXPECT_TRUE(IsDisabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NEEDED,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));

  EXPECT_TRUE(IsDisabled(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NEEDED,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId2));

  EXPECT_TRUE(IsEnabled(kExtensionId3));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId3));

  // Leave `kExtensionId1` with acknowledge needed and acknowledge
  // `kExtensionId2`.
  allowlist()->SetExtensionAllowlistAcknowledgeState(
      kExtensionId2, ALLOWLIST_ACKNOWLEDGE_DONE);

  // When turning off enhanced protection.
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // 'kExtensionId1' and 'kExtensionId2' should be re-enabled and have their
  // acknowledge state reset.
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NONE,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId1));

  EXPECT_TRUE(IsEnabled(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_NONE,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId2));

  // 'kExtensionId3' should remain enabled because it was already re-enabled by
  // user.
  EXPECT_TRUE(IsEnabled(kExtensionId3));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId3));
}

TEST_F(ExtensionAllowlistUnitTest, BypassFrictionSetAckowledgeEnabledByUser) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  installer->set_bypassed_safebrowsing_friction_for_testing(true);

  base::RunLoop run_loop;
  installer->AddInstallerCallback(base::BindOnce(
      [](base::OnceClosure quit_closure,
         const std::optional<CrxInstallError>& error) {
        ASSERT_FALSE(error) << error->message();
        std::move(quit_closure).Run();
      },
      run_loop.QuitWhenIdleClosure()));

  installer->InstallCrx(data_dir().AppendASCII("good.crx"));
  run_loop.Run();

  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kInstalledCrx));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kInstalledCrx));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kInstalledCrx));
}

TEST_F(ExtensionAllowlistUnitTest, NoEnforcementOnPolicyForceInstall) {
  // Mark as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  CreateEmptyExtensionService();
  service()->Init();

  // Add a policy installed extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("policy_installed")
          .SetPath(data_dir().AppendASCII("good.crx"))
          .SetLocation(mojom::ManifestLocation::kExternalPolicyDownload)
          .Build();
  service()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionAutoInstalled(
        extension->id(), "http://example.com/update_url", true);
  }

  EXPECT_TRUE(IsEnabled(extension->id()));

  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // A policy installed extension is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
  // No warnings are shown for policy installed extensions.
  EXPECT_FALSE(allowlist()->ShouldDisplayWarning(extension->id()));
}

class ExtensionAllowlistWithFeatureDisabledUnitTest
    : public ExtensionAllowlistUnitTestBase {
 public:
  ExtensionAllowlistWithFeatureDisabledUnitTest() {
    // Test with warnings enabled but auto disable disabled.
    feature_list_.InitWithFeatures(
        {extensions_features::kSafeBrowsingCrxAllowlistShowWarnings},
        {extensions_features::kSafeBrowsingCrxAllowlistAutoDisable});
  }
};

TEST_F(ExtensionAllowlistWithFeatureDisabledUnitTest,
       NoEnforcementWhenFeatureDisabled) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId1));
}

// TODO(jeffcyr): Test with auto-disablement enabled when the enforcement is
// skipped for policy recommended and policy allowed extensions.
TEST_F(ExtensionAllowlistWithFeatureDisabledUnitTest,
       NoEnforcementOnPolicyRecommendedInstall) {
  CreateEmptyExtensionService();
  service()->Init();

  // Add a policy installed extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("policy_installed")
          .SetPath(data_dir().AppendASCII("good.crx"))
          .SetLocation(mojom::ManifestLocation::kExternalPrefDownload)
          .Build();
  service()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionAutoInstalled(
        extension->id(), "http://example.com/update_url", false);
  }

  EXPECT_TRUE(IsEnabled(extension->id()));

  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // A policy installed extension is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
  // No warnings are shown for policy installed extensions.
  EXPECT_FALSE(allowlist()->ShouldDisplayWarning(extension->id()));
}

TEST_F(ExtensionAllowlistWithFeatureDisabledUnitTest,
       NoEnforcementOnPolicyAllowedInstall) {
  CreateEmptyExtensionService();
  service()->Init();

  // Add a policy allowed extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("policy_allowed")
          .SetPath(data_dir().AppendASCII("good.crx"))
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  service()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(extension->id(), true);
  }

  EXPECT_TRUE(IsEnabled(extension->id()));

  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // An extension allowed by policy is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
  // No warnings are shown for policy allowed extensions.
  EXPECT_FALSE(allowlist()->ShouldDisplayWarning(extension->id()));
}

// TODO(crbug.com/40175473): Add more ExtensionAllowlist::Observer coverage

}  // namespace extensions
