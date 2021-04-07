// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_extension_prefs.h"

#include "chrome/browser/extensions/blocklist_extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
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
  ExtensionPrefs* extension_prefs_;
};

TEST_F(BlocklistExtensionPrefsUnitTest, OmahaBlocklistState) {
  BitMapBlocklistState state1 =
      BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED;
  BitMapBlocklistState state2 =
      BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY;
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                       extension_prefs()));

  blocklist_prefs::AddOmahaBlocklistState(kExtensionId, state1,
                                          extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                      extension_prefs()));

  blocklist_prefs::AddOmahaBlocklistState(kExtensionId, state2,
                                          extension_prefs());
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state2,
                                                      extension_prefs()));
  // Doesn't clear the other blocklist state.
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                      extension_prefs()));

  blocklist_prefs::RemoveOmahaBlocklistState(kExtensionId, state1,
                                             extension_prefs());
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state1,
                                                       extension_prefs()));
  // Doesn't remove the other blocklist state.
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(kExtensionId, state2,
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

  blocklist_prefs::RemoveAcknowledgedBlocklistState(kExtensionId, state1,
                                                    extension_prefs());
  EXPECT_FALSE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state1, extension_prefs()));
  // Doesn't remove the other blocklist state.
  EXPECT_TRUE(blocklist_prefs::HasAcknowledgedBlocklistState(
      kExtensionId, state2, extension_prefs()));
}

}  // namespace extensions
