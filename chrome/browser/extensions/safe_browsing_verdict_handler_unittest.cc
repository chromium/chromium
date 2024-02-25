// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/safe_browsing_verdict_handler.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/buildflags.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/test/extension_state_tester.h"

// The blocklist tests rely on the safe-browsing database.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#define ENABLE_BLOCKLIST_TESTS
#endif

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kGood0[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kGood1[] = "hpiknbiabeeppbpihjehijgoemciehgk";
constexpr char kGood2[] = "bjafgdebaacbbbecmhlhpofkepfkgcpa";

}  // namespace

// Test suite to test safe browsing verdict handler.
class SafeBrowsingVerdictHandlerUnitTest : public ExtensionServiceTestBase {
 protected:
  void SetBlocklistStateForExtension(const std::string& extension_id,
                                     BlocklistState state,
                                     TestBlocklist& test_blocklist) {
    // Reset cache in blocklist to make sure the latest blocklist state is
    // fetched.
    service()->blocklist_->ResetBlocklistStateCacheForTest();
    test_blocklist.SetBlocklistState(extension_id, state, true);
    task_environment()->RunUntilIdle();
  }
};

#if defined(ENABLE_BLOCKLIST_TESTS)
// Extension is added to blocklist with BLOCKLISTED_POTENTIALLY_UNWANTED state
// after it is installed. It is then successfully re-enabled by the user.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, GreylistedExtensionDisabled) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood1));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));

  // Add kGood0 and kGood1 (and an invalid extension ID) to greylist.
  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                   true);
  test_blocklist.SetBlocklistState("invalid_id", BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));

  ValidateIntegerPref(kGood0, "blacklist_state",
                      BLOCKLISTED_CWS_POLICY_VIOLATION);
  ValidateIntegerPref(kGood1, "blacklist_state",
                      BLOCKLISTED_POTENTIALLY_UNWANTED);

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);

  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));

  // Remove extensions from blocklist.
  test_blocklist.SetBlocklistState(kGood0, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood1, NOT_BLOCKLISTED, true);
  task_environment()->RunUntilIdle();

  // All extensions are enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood1));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));
}

// When extension is removed from greylist, do not re-enable it if it is
// disabled by user.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, GreylistDontEnableManuallyDisabled) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  // Manually disable.
  service()->DisableExtension(kGood0, disable_reason::DISABLE_USER_ACTION);

  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                   true);
  test_blocklist.SetBlocklistState(kGood2, BLOCKLISTED_SECURITY_VULNERABILITY,
                                   true);
  task_environment()->RunUntilIdle();

  ExtensionStateTester state_tester(profile());

  // All extensions disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithReasons(
      kGood0,
      disable_reason::DISABLE_GREYLIST | disable_reason::DISABLE_USER_ACTION));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood2, disable_reason::DISABLE_GREYLIST));

  // Greylisted extension can be enabled.
  service()->EnableExtension(kGood1);
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood1));

  // kGood1 is now manually disabled.
  service()->DisableExtension(kGood1, disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_USER_ACTION));

  // Remove extensions from blocklist.
  test_blocklist.SetBlocklistState(kGood0, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood1, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood2, NOT_BLOCKLISTED, true);
  task_environment()->RunUntilIdle();

  // kGood0 and kGood1 remain disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_USER_ACTION));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_USER_ACTION));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));
}

// Greylisted extension with unknown state are not enabled/disabled.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, GreylistUnknownDontChange) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                   true);
  task_environment()->RunUntilIdle();

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));

  test_blocklist.SetBlocklistState(kGood0, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_UNKNOWN, true);
  test_blocklist.SetBlocklistState(kGood2, BLOCKLISTED_UNKNOWN, true);
  task_environment()->RunUntilIdle();

  // kGood0 re-enabled, other remain as they were.
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood1, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood2));
}

// The extension is loaded but kept disabled when it is downgraded from
// blocklist to greylist.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       UnblocklistedExtensionStillGreylisted) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  // Add the extension to blocklist
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_MALWARE, test_blocklist);

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectBlocklisted(kGood0));

  // Remove the extension from blocklist and add it to greylist
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);
  content::RunAllTasksUntilIdle();

  // The extension is reloaded, but remains disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
}

// When extension is on the greylist, do not disable it if it is re-enabled by
// user.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       GreylistedExtensionDoesNotDisableAgain) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  ExtensionStateTester state_tester(profile());

  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  // The acknowledged state should not be cleared when the extension is
  // re-enabled.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Set the blocklist to the same greylist state.
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  // kGood0 should still be enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  // The acknowledged state should not be cleared.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));
}

// When extension is removed from the greylist and re-added, disable the
// extension again.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       GreylistedExtensionDisableAgainIfReAdded) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  ExtensionStateTester state_tester(profile());

  // kGood0 is disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  // The acknowledged state should not be cleared when the extension is
  // re-enabled.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Remove kGood0 from blocklist.
  SetBlocklistStateForExtension(kGood0, NOT_BLOCKLISTED, test_blocklist);

  // kGood0 should still be enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
  // The acknowledged state should be cleared when the extension is removed from
  // the blocklist.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Set the blocklist to the same greylist state.
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  // kGood0 is disabled again.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  // The acknowledged state should be set again.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));
}

// When extension is on the greylist, disable it again if the greylist state
// changes, even if the user has re-enabled it.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       DisableExtensionForDifferentGreylistState) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  ExtensionStateTester state_tester(profile());

  // kGood0 is disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));

  // Set the blocklist to another greylist state.
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                test_blocklist);

  // The extension should be disabled again.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  // The old acknowledged state should be cleared and the new one should be set.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs));
}

// Add the extension to greylist state1, and then switch to greylist state2, and
// then the user re-enables the extension, and then the extension is switched
// back to greylist state1, the extension should be disabled again.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       DisableExtensionWhenSwitchingBetweenGreylistStates) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  // Set the blocklist to another greylist state.
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                test_blocklist);

  ExtensionStateTester state_tester(profile());

  // kGood0 is disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs));

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));

  // Set the blocklist to the original blocklist state.
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  // The extension should be disabled again.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));
  // The acknowledged state should be set to the current state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs));
}

// Old greylisted extensions are not re-enabled.
// This test is for checking backward compatibility.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, AcknowledgedStateBackFilled) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  ExtensionStateTester state_tester(profile());

  // kGood0 is disabled.
  EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
      kGood0, disable_reason::DISABLE_GREYLIST));

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));

  // To simulate an old Chrome version, the acknowledged state is cleared.
  blocklist_prefs::ClearAcknowledgedGreylistStates(
      kGood0, ExtensionPrefs::Get(profile()));
  // The browser is restarted.
  service()->safe_browsing_verdict_handler_.Init();

  // The acknowledged state should be restored.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kGood0, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs));

  // Set the blocklist to the same greylist state.
  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                test_blocklist);

  // kGood0 should remain enabled.
  EXPECT_TRUE(state_tester.ExpectEnabled(kGood0));
}

// Regression test for https://crbug.com/1267860. It should not crash if the
// extension is uninstalled before it is removed from the blocklist.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       ExtensionUninstalledWhenBlocklisted) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_MALWARE, test_blocklist);

  ExtensionStateTester state_tester(profile());

  // kGood0 is blocklisted.
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kGood0));

  // Now uninstall kGood0.
  service()->UninstallExtension(kGood0, UNINSTALL_REASON_FOR_TESTING, nullptr);
  // kGood0 should be removed from the blocklist.
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());

  // Should not crash.
  SetBlocklistStateForExtension(kGood0, NOT_BLOCKLISTED, test_blocklist);
}

// Regression test for https://crbug.com/1267860. It should not crash if the
// extension is uninstalled during blocklist fetching.
TEST_F(SafeBrowsingVerdictHandlerUnitTest,
       ExtensionUninstalledWhenBlocklistFetching) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  SetBlocklistStateForExtension(kGood0, BLOCKLISTED_MALWARE, test_blocklist);

  ExtensionStateTester state_tester(profile());

  // kGood0 is blocklisted.
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kGood0));

  service()->blocklist_->ResetBlocklistStateCacheForTest();
  // Use TestBlocklist::SetBlocklistState() here instead of
  // SetBlocklistStateForExtension(). This makes the blocklisting process
  // asynchronous, so that we can simulate uninstalling the extension
  // during a blocklist state fetch.
  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_MALWARE, true);

  // Uninstalled the extension in the middle of the update.
  service()->UninstallExtension(kGood0, UNINSTALL_REASON_FOR_TESTING, nullptr);
  // Should not crash when the update finishes.
  task_environment()->RunUntilIdle();
}

#endif  // defined(ENABLE_BLOCKLIST_TESTS)

}  // namespace extensions
