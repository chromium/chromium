// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/browser_test.h"

namespace arc {

namespace {

bool AllProfilesHaveSameArcPackageDetails() {
  return SyncArcPackageHelper::GetInstance()
      ->AllProfilesHaveSamePackageDetails();
}

}  // namespace

class TwoClientArcPackageSyncTest : public SyncTest {
 public:
  TwoClientArcPackageSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientArcPackageSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest, StartWithNoPackages) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest, StartWithSamePackages) {
  ASSERT_TRUE(SetupClients());

  constexpr size_t kNumPackages = 5;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// In this test, packages are installed before sync started. Client1 will have
// package 0 to 4 installed while client2 has no package installed.
IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest,
                       OneClientHasPackagesAnotherHasNone) {
  ASSERT_TRUE(SetupClients());

  constexpr size_t kNumPackages = 5;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
  }

  ASSERT_FALSE(AllProfilesHaveSameArcPackageDetails());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// In this test, packages are installed before sync started. Client1 will have
// package 0 to 9 installed and client2 will have package 0 to 4 installed.
IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest,
                       OneClientHasPackagesAnotherHasSubSet) {
  ASSERT_TRUE(SetupClients());

  constexpr size_t kNumPackages = 5;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(1), i);
  }

  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i + kNumPackages);
  }

  ASSERT_FALSE(AllProfilesHaveSameArcPackageDetails());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// In this test, packages are installed before sync started. Client1 will have
// package 0 to 4 installed and client2 will have package 1 to 5 installed.
IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest,
                       StartWithDifferentPackages) {
  ASSERT_TRUE(SetupClients());

  constexpr size_t kNumPackages = 5;
  constexpr size_t kPackageIdDiff = 1;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(1),
                                               i + kPackageIdDiff);
  }

  EXPECT_FALSE(AllProfilesHaveSameArcPackageDetails());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// Tests package installaton after sync started.
IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest, Install) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());

  sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// In this test, packages are installed after sync started. Client1 installs
// package 0 to 4 and client2 installs package 3 to 7.
IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest, InstallDifferent) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());

  constexpr size_t kNumPackages = 5;
  constexpr size_t kPackageIdDiff = 3;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(1),
                                               i + kPackageIdDiff);
  }

  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// Installs package from one client and uninstalls from another after sync
// started.
IN_PROC_BROWSER_TEST_F(TwoClientArcPackageSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());

  sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), 1);
  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllProfilesHaveSameArcPackageDetails());

  sync_arc_helper()->UninstallPackageWithIndex(GetProfile(1), 1);
  EXPECT_FALSE(AllProfilesHaveSameArcPackageDetails());
  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

}  // namespace arc
