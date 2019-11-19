// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace arc {

namespace {

bool AllProfilesHaveSameArcPackageDetails() {
  return SyncArcPackageHelper::GetInstance()
      ->AllProfilesHaveSamePackageDetails();
}

}  // namespace

class SingleClientArcPackageSyncTest : public SyncTest {
 public:
  SingleClientArcPackageSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientArcPackageSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientArcPackageSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest, ArcPackageEmpty) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest,
                       ArcPackageInstallSomePackages) {
  ASSERT_TRUE(SetupSync());

  constexpr size_t kNumPackages = 5;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_arc_helper()->InstallPackageWithIndex(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

// Regression test for crbug.com/978837.
IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest, DisableAndReenable) {
  ASSERT_TRUE(SetupSync());

  sync_arc_helper()->InstallPackageWithIndex(verifier(), 0);
  sync_pb::EntitySpecifics specifics = sync_arc_helper()->GetTestSpecifics(0);

  // Disable ARC++ to verify sync resumes correctly when it gets reenabled
  // later. Note that the sync datatype itself is not disabled.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::ARC_PACKAGE));
  sync_arc_helper()->DisableArcService(GetProfile(0));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::ARC_PACKAGE));

  // Fake new sync data being uploaded by another client while ARC++ is
  // disabled.
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"",
          /*client_tag=*/specifics.arc_package().package_name(), specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));

  // Reenable ARC++.
  sync_arc_helper()->EnableArcService(GetProfile(0));
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // The problematic scenario in the regression test involves the refresh
  // happening late, after sync has started.
  sync_arc_helper()->SendRefreshPackageList(GetProfile(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

}  // namespace arc
