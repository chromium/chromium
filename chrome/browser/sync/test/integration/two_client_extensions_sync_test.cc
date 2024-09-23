// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/browser_test.h"

namespace {

using extensions_helper::AllProfilesHaveSameExtensions;
using extensions_helper::DisableExtension;
using extensions_helper::EnableExtension;
using extensions_helper::GetInstalledExtensions;
using extensions_helper::HasSameExtensions;
using extensions_helper::IncognitoDisableExtension;
using extensions_helper::IncognitoEnableExtension;
using extensions_helper::InstallExtension;
using extensions_helper::UninstallExtension;

class TwoClientExtensionsSyncTest : public SyncTest {
 public:
  TwoClientExtensionsSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientExtensionsSyncTest() override = default;

  bool TestUsesSelfNotifications() override { return false; }

 private:
  // TODO(https://crbug.com/40804030): Remove when these tests use only MV3
  // extensions.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(StartWithNoExtensions)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
}

// Flaky on Mac: http://crbug.com/535996
#if BUILDFLAG(IS_MAC)
#define MAYBE_StartWithSameExtensions DISABLED_StartWithSameExtensions
#else
#define MAYBE_StartWithSameExtensions StartWithSameExtensions
#endif
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(MAYBE_StartWithSameExtensions)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  EXPECT_EQ(kNumExtensions,
            static_cast<int>(GetInstalledExtensions(GetProfile(0)).size()));
}

// Flaky on Mac: http://crbug.com/535996
#if BUILDFLAG(IS_MAC)
#define MAYBE_StartWithDifferentExtensions DISABLED_StartWithDifferentExtensions
#else
#define MAYBE_StartWithDifferentExtensions StartWithDifferentExtensions
#endif
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(MAYBE_StartWithDifferentExtensions)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients());

  int extension_index = 0;

  const int kNumCommonExtensions = 5;
  for (int i = 0; i < kNumCommonExtensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
    InstallExtension(GetProfile(1), extension_index);
  }

  const int kNumProfile0Extensions = 10;
  for (int i = 0; i < kNumProfile0Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
  }

  const int kNumProfile1Extensions = 10;
  for (int i = 0; i < kNumProfile1Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(1), extension_index);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  EXPECT_EQ(
      kNumCommonExtensions + kNumProfile0Extensions + kNumProfile1Extensions,
      static_cast<int>(GetInstalledExtensions(GetProfile(0)).size()));
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(InstallDifferentExtensions)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients());

  int extension_index = 0;

  const int kNumCommonExtensions = 5;
  for (int i = 0; i < kNumCommonExtensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
    InstallExtension(GetProfile(1), extension_index);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());

  const int kNumProfile0Extensions = 10;
  for (int i = 0; i < kNumProfile0Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
  }

  const int kNumProfile1Extensions = 10;
  for (int i = 0; i < kNumProfile1Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(1), extension_index);
  }

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  EXPECT_EQ(
      kNumCommonExtensions + kNumProfile0Extensions + kNumProfile1Extensions,
      static_cast<int>(GetInstalledExtensions(GetProfile(0)).size()));
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, E2E_ENABLED(Add)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  EXPECT_EQ(1u, GetInstalledExtensions(GetProfile(0)).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, E2E_ENABLED(Uninstall)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());

  UninstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  EXPECT_TRUE(GetInstalledExtensions(GetProfile(0)).empty());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(UpdateEnableDisableExtension)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());

  DisableExtension(GetProfile(0), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());

  EnableExtension(GetProfile(1), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(UpdateIncognitoEnableDisable)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(ExtensionsMatchChecker().Wait());

  IncognitoEnableExtension(GetProfile(0), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());

  IncognitoDisableExtension(GetProfile(1), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
}

// Regression test for bug 104399: ensure that an extension installed prior to
// setting up sync, when uninstalled, is also uninstalled from sync.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(UninstallPreinstalledExtensions)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  ASSERT_EQ(1u, GetInstalledExtensions(GetProfile(0)).size());

  UninstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(ExtensionsMatchChecker().Wait());
  EXPECT_TRUE(GetInstalledExtensions(GetProfile(0)).empty());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior

}  // namespace
