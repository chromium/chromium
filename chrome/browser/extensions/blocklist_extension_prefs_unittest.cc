// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist_extension_prefs.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

}  // namespace

// Test suite to test blocklist extension prefs.
class BlocklistExtensionPrefsUnitTest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    InitializeEmptyExtensionService();
    extension_prefs_ = ExtensionPrefs::Get(profile());
  }

  ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  raw_ptr<ExtensionPrefs> extension_prefs_;
};

TEST_F(BlocklistExtensionPrefsUnitTest, OmahaBlocklistState) {
  BitMapBlocklistState state1 =
      BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED;
  BitMapBlocklistState state2 =
      BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY;
  BitMapBlocklistState state3 =
      BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION;
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                       extension_prefs()));
  EXPECT_FALSE(blocklist_prefs::HasAnyOmahaGreylistState(kExtensionId,
                                                         extension_prefs()));

  blocklist_prefs::AddOmahaBlocklistState(kExtensionId, state1,
                                          extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                      extension_prefs()));
  EXPECT_TRUE(blocklist_prefs::HasAnyOmahaGreylistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::AddOmahaBlocklistState(kExtensionId, state2,
                                          extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state2,
                                                      extension_prefs()));
  // Doesn't clear the other blocklist state.
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                      extension_prefs()));
  EXPECT_TRUE(blocklist_prefs::HasAnyOmahaGreylistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::RemoveOmahaBlocklistState(kExtensionId, state1,
                                             extension_prefs());
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                       extension_prefs()));
  // Doesn't remove the other blocklist state.
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state2,
                                                      extension_prefs()));
  EXPECT_TRUE(blocklist_prefs::HasAnyOmahaGreylistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::AddOmahaBlocklistState(kExtensionId, state3,
                                          extension_prefs());
  blocklist_prefs::RemoveOmahaBlocklistState(kExtensionId, state2,
                                             extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasAnyOmahaGreylistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::RemoveOmahaBlocklistState(kExtensionId, state3,
                                             extension_prefs());
  EXPECT_FALSE(blocklist_prefs::HasAnyOmahaGreylistState(kExtensionId,
                                                         extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest, AcknowledgedBlocklistState) {
  BitMapBlocklistState state1 =
      BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED;
  BitMapBlocklistState state2 =
      BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY;
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state1, extension_prefs()));

  blocklist_prefs::AddAcknowledgedBlocklistState(kExtensionId, state1,
                                                 extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state1, extension_prefs()));

  blocklist_prefs::AddAcknowledgedBlocklistState(kExtensionId, state2,
                                                 extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state2, extension_prefs()));
  // Doesn't clear the other blocklist state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state1, extension_prefs()));

  blocklist_prefs::AddAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  blocklist_prefs::ClearAcknowledgedGreylistStates(kExtensionId,
                                                   extension_prefs());
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state1, extension_prefs()));
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state2, extension_prefs()));
  // The malware acknowledged state should not be cleared.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest,
       UpdateCurrentGreylistStatesAsAcknowledged) {
  blocklist_prefs::AddAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  blocklist_prefs::AddAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
      extension_prefs());
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs());
  blocklist_prefs::AddOmahaBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs());

  blocklist_prefs::UpdateCurrentGreylistStatesAsAcknowledged(kExtensionId,
                                                             extension_prefs());

  // The BLOCKLISTED_SECURITY_VULNERABILITY should be cleared because it is not
  // in any greylist state.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
      extension_prefs()));
  // BLOCKLISTED_POTENTIALLY_UNWANTED should be acknowledged because it is in
  // the Safe Browsing greylist state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs()));
  // BLOCKLISTED_CWS_POLICY_VIOLATION should be acknowledged because it is in
  // the Omaha greylist state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));
  // BLOCKLISTED_MALWARE should not be cleared because it is not a greylist
  // state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
      extension_prefs());
  blocklist_prefs::UpdateCurrentGreylistStatesAsAcknowledged(kExtensionId,
                                                             extension_prefs());

  // The BLOCKLISTED_SECURITY_VULNERABILITY should be acknowledged because it is
  // in the Safe Browsing greylist state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
      extension_prefs()));
  // BLOCKLISTED_POTENTIALLY_UNWANTED should be cleared because it is not in any
  // greylist state.
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs()));
  // BLOCKLISTED_CWS_POLICY_VIOLATION should be acknowledged because it is in
  // the Omaha greylist state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest, GetExtensionBlocklistState) {
  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetExtensionBlocklistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs());
  blocklist_prefs::AddOmahaBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
      extension_prefs());
  // BLOCKLISTED_POTENTIALLY_UNWANTED has a higher precedence than
  // BLOCKLISTED_SECURITY_VULNERABILITY.
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
            blocklist_prefs::GetExtensionBlocklistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::AddOmahaBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs());
  // BLOCKLISTED_CWS_POLICY_VIOLATION has a higher precedence than
  // BLOCKLISTED_POTENTIALLY_UNWANTED.
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            blocklist_prefs::GetExtensionBlocklistState(kExtensionId,
                                                        extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  // BLOCKLISTED_MALWARE has the highest precedence.
  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetExtensionBlocklistState(kExtensionId,
                                                        extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest, SafeBrowsingExtensionBlocklistState) {
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());

  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId, extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::NOT_BLOCKLISTED, extension_prefs());

  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                kExtensionId, extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest, IsExtensionBlocklisted) {
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  EXPECT_TRUE(
      blocklist_prefs::IsExtensionBlocklisted(kExtensionId, extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      extension_prefs());
  EXPECT_FALSE(
      blocklist_prefs::IsExtensionBlocklisted(kExtensionId, extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  EXPECT_TRUE(
      blocklist_prefs::IsExtensionBlocklisted(kExtensionId, extension_prefs()));

  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      kExtensionId, BitMapBlocklistState::NOT_BLOCKLISTED, extension_prefs());
  EXPECT_FALSE(
      blocklist_prefs::IsExtensionBlocklisted(kExtensionId, extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest,
       ExtensionTelemetryServiceBlocklistState) {
  blocklist_prefs::SetExtensionTelemetryServiceBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());

  EXPECT_EQ(BitMapBlocklistState::BLOCKLISTED_MALWARE,
            blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kExtensionId, extension_prefs()));

  blocklist_prefs::SetExtensionTelemetryServiceBlocklistState(
      kExtensionId, BitMapBlocklistState::NOT_BLOCKLISTED, extension_prefs());

  EXPECT_EQ(BitMapBlocklistState::NOT_BLOCKLISTED,
            blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                kExtensionId, extension_prefs()));
}

TEST_F(BlocklistExtensionPrefsUnitTest,
       IsExtensionBlocklisted_ExtensionTelemetryService) {
  blocklist_prefs::SetExtensionTelemetryServiceBlocklistState(
      kExtensionId, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  EXPECT_TRUE(
      blocklist_prefs::IsExtensionBlocklisted(kExtensionId, extension_prefs()));

  blocklist_prefs::SetExtensionTelemetryServiceBlocklistState(
      kExtensionId, BitMapBlocklistState::NOT_BLOCKLISTED, extension_prefs());
  EXPECT_FALSE(
      blocklist_prefs::IsExtensionBlocklisted(kExtensionId, extension_prefs()));
}

}  // namespace extensions
