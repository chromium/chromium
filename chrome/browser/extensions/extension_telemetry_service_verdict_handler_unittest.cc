// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_telemetry_service_verdict_handler.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/test/extension_state_tester.h"

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kUninstalledExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

// Test suite to test Extension Telemetry service verdict handler.
class ExtensionTelemetryServiceVerdictHandlerTest
    : public ExtensionServiceTestBase {
 public:
  ExtensionTelemetryServiceVerdictHandlerTest() {
    // Set to true so the acknowledged state is not automatically set by the
    // extension error controller on the first run.
    ExtensionPrefs::SetRunAlertsInFirstRunForTest();
  }
};

TEST_F(ExtensionTelemetryServiceVerdictHandlerTest, HandlesMalwareExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  Blocklist::BlocklistStateMap state_map;
  state_map[kTestExtensionId] = BlocklistState::BLOCKLISTED_MALWARE;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));
  EXPECT_EQ(blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kTestExtensionId, prefs),
            BitMapBlocklistState::BLOCKLISTED_MALWARE);
}

TEST_F(ExtensionTelemetryServiceVerdictHandlerTest,
       ReenablesUnblocklistedExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  Blocklist::BlocklistStateMap state_map;
  state_map[kTestExtensionId] = BlocklistState::BLOCKLISTED_MALWARE;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectBlocklisted(kTestExtensionId));
  EXPECT_EQ(blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kTestExtensionId, prefs),
            BitMapBlocklistState::BLOCKLISTED_MALWARE);
  // Acknowledged state is false since user hasn't acknowledged.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));

  // User acknowledges.
  blocklist_prefs::AddAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs);
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));

  // Unblocklists kTestExtensionId.
  state_map[kTestExtensionId] = BlocklistState::NOT_BLOCKLISTED;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  EXPECT_EQ(blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kTestExtensionId, prefs),
            BitMapBlocklistState::NOT_BLOCKLISTED);
  // Acknowledged state is cleared since the extension is removed from the
  // blocklist.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kTestExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));
}

TEST_F(ExtensionTelemetryServiceVerdictHandlerTest,
       IgnoresUninstalledExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  Blocklist::BlocklistStateMap state_map;
  state_map[kUninstalledExtensionId] = BlocklistState::BLOCKLISTED_MALWARE;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kUninstalledExtensionId, prefs),
            BitMapBlocklistState::NOT_BLOCKLISTED);
}

TEST_F(ExtensionTelemetryServiceVerdictHandlerTest,
       IgnoresUnknownBlocklistState) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  Blocklist::BlocklistStateMap state_map;
  state_map[kTestExtensionId] = BlocklistState::BLOCKLISTED_UNKNOWN;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));
  EXPECT_EQ(blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kTestExtensionId, prefs),
            BitMapBlocklistState::NOT_BLOCKLISTED);
}

TEST_F(ExtensionTelemetryServiceVerdictHandlerTest,
       ExtensionAlreadyUninstalled) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  ExtensionStateTester state_tester(profile());
  EXPECT_TRUE(state_tester.ExpectEnabled(kTestExtensionId));

  service()->UninstallExtension(kTestExtensionId, UNINSTALL_REASON_FOR_TESTING,
                                nullptr);

  Blocklist::BlocklistStateMap state_map;
  state_map[kTestExtensionId] = BlocklistState::BLOCKLISTED_MALWARE;
  // kTestExtensionId is already uninstalled. Performing action on it should
  // not crash. Regression test for https://crbug.com/1305490.
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);
}

}  // namespace extensions
