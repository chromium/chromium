// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/safe_browsing_verdict_handler.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "components/safe_browsing/buildflags.h"

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
using SafeBrowsingVerdictHandlerUnitTest = ExtensionServiceTestBase;

#if defined(ENABLE_BLOCKLIST_TESTS)
// Extension is added to blocklist with BLOCKLISTED_POTENTIALLY_UNWANTED state
// after it is installed. It is then successfully re-enabled by the user.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, GreylistedExtensionDisabled) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry()->disabled_extensions();

  EXPECT_TRUE(enabled_extensions.Contains(kGood0));
  EXPECT_TRUE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(enabled_extensions.Contains(kGood2));

  // Add kGood0 and kGood1 (and an invalid extension ID) to greylist.
  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                   true);
  test_blocklist.SetBlocklistState("invalid_id", BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(enabled_extensions.Contains(kGood0));
  EXPECT_TRUE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));
  EXPECT_TRUE(enabled_extensions.Contains(kGood2));
  EXPECT_FALSE(disabled_extensions.Contains(kGood2));

  ValidateIntegerPref(kGood0, "blacklist_state",
                      BLOCKLISTED_CWS_POLICY_VIOLATION);
  ValidateIntegerPref(kGood1, "blacklist_state",
                      BLOCKLISTED_POTENTIALLY_UNWANTED);

  // Now user enables kGood0.
  service()->EnableExtension(kGood0);

  EXPECT_TRUE(enabled_extensions.Contains(kGood0));
  EXPECT_FALSE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));

  // Remove extensions from blocklist.
  test_blocklist.SetBlocklistState(kGood0, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood1, NOT_BLOCKLISTED, true);
  task_environment()->RunUntilIdle();

  // All extensions are enabled.
  EXPECT_TRUE(enabled_extensions.Contains(kGood0));
  EXPECT_FALSE(disabled_extensions.Contains(kGood0));
  EXPECT_TRUE(enabled_extensions.Contains(kGood1));
  EXPECT_FALSE(disabled_extensions.Contains(kGood1));
  EXPECT_TRUE(enabled_extensions.Contains(kGood2));
  EXPECT_FALSE(disabled_extensions.Contains(kGood2));
}

// When extension is removed from greylist, do not re-enable it if it is
// disabled by user.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, GreylistDontEnableManuallyDisabled) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry()->disabled_extensions();

  // Manually disable.
  service()->DisableExtension(kGood0, disable_reason::DISABLE_USER_ACTION);

  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                   true);
  test_blocklist.SetBlocklistState(kGood2, BLOCKLISTED_SECURITY_VULNERABILITY,
                                   true);
  task_environment()->RunUntilIdle();

  // All extensions disabled.
  EXPECT_FALSE(enabled_extensions.Contains(kGood0));
  EXPECT_TRUE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));
  EXPECT_FALSE(enabled_extensions.Contains(kGood2));
  EXPECT_TRUE(disabled_extensions.Contains(kGood2));

  // Greylisted extension can be enabled.
  service()->EnableExtension(kGood1);
  EXPECT_TRUE(enabled_extensions.Contains(kGood1));
  EXPECT_FALSE(disabled_extensions.Contains(kGood1));

  // kGood1 is now manually disabled.
  service()->DisableExtension(kGood1, disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));

  // Remove extensions from blocklist.
  test_blocklist.SetBlocklistState(kGood0, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood1, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood2, NOT_BLOCKLISTED, true);
  task_environment()->RunUntilIdle();

  // kGood0 and kGood1 remain disabled.
  EXPECT_FALSE(enabled_extensions.Contains(kGood0));
  EXPECT_TRUE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));
  EXPECT_TRUE(enabled_extensions.Contains(kGood2));
  EXPECT_FALSE(disabled_extensions.Contains(kGood2));
}

// Greylisted extension with unknown state are not enabled/disabled.
TEST_F(SafeBrowsingVerdictHandlerUnitTest, GreylistUnknownDontChange) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: kGood0, kGood1, and kGood2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry()->disabled_extensions();

  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_POTENTIALLY_UNWANTED,
                                   true);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(enabled_extensions.Contains(kGood0));
  EXPECT_TRUE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));
  EXPECT_TRUE(enabled_extensions.Contains(kGood2));
  EXPECT_FALSE(disabled_extensions.Contains(kGood2));

  test_blocklist.SetBlocklistState(kGood0, NOT_BLOCKLISTED, true);
  test_blocklist.SetBlocklistState(kGood1, BLOCKLISTED_UNKNOWN, true);
  test_blocklist.SetBlocklistState(kGood2, BLOCKLISTED_UNKNOWN, true);
  task_environment()->RunUntilIdle();

  // kGood0 re-enabled, other remain as they were.
  EXPECT_TRUE(enabled_extensions.Contains(kGood0));
  EXPECT_FALSE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(enabled_extensions.Contains(kGood1));
  EXPECT_TRUE(disabled_extensions.Contains(kGood1));
  EXPECT_TRUE(enabled_extensions.Contains(kGood2));
  EXPECT_FALSE(disabled_extensions.Contains(kGood2));
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

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry()->disabled_extensions();
  const ExtensionSet& blocklisted_extensions =
      registry()->blocklisted_extensions();

  // Add the extension to blocklist
  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(enabled_extensions.Contains(kGood0));
  // False because the extension should be unloaded.
  EXPECT_FALSE(disabled_extensions.Contains(kGood0));
  EXPECT_TRUE(blocklisted_extensions.Contains(kGood0));

  // Reset cache in blocklist to make sure the latest blocklist state is
  // fetched.
  service()->blocklist_->ResetBlocklistStateCacheForTest();
  // Remove the extension from blocklist and add it to greylist
  test_blocklist.SetBlocklistState(kGood0, BLOCKLISTED_CWS_POLICY_VIOLATION,
                                   true);
  content::RunAllTasksUntilIdle();

  // The extension is reloaded, but remains disabled.
  EXPECT_FALSE(enabled_extensions.Contains(kGood0));
  EXPECT_TRUE(disabled_extensions.Contains(kGood0));
  EXPECT_FALSE(blocklisted_extensions.Contains(kGood0));
}

#endif  // defined(ENABLE_BLOCKLIST_TESTS)

}  // namespace extensions
