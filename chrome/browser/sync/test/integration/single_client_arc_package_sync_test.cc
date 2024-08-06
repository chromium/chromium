// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace arc {

namespace {

using testing::IsEmpty;
using testing::SizeIs;

class ArcPackagesCountChecker : public SingleClientStatusChangeChecker {
 public:
  ArcPackagesCountChecker(Profile* profile,
                          syncer::SyncServiceImpl* service,
                          size_t expected_count)
      : SingleClientStatusChangeChecker(service),
        profile_(profile),
        expected_count_(expected_count) {}
  ~ArcPackagesCountChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    size_t current_count =
        ArcAppListPrefs::Get(profile_)->GetPackagesFromPrefs().size();
    *os << "Waiting for " << expected_count_ << " Arc packages, currently have "
        << current_count;
    return current_count == expected_count_;
  }

 private:
  const raw_ptr<Profile> profile_;
  const size_t expected_count_;
};

class FakeServerArcPackageMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit FakeServerArcPackageMatchChecker(
      const std::vector<sync_pb::EntitySpecifics>& expected_entities)
      : expected_entities_(expected_entities) {}
  ~FakeServerArcPackageMatchChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for server-side Arc packages to match expected.";

    std::vector<sync_pb::SyncEntity> server_entities =
        fake_server()->GetSyncEntitiesByDataType(syncer::ARC_PACKAGE);
    if (server_entities.size() != expected_entities_.size()) {
      return false;
    }

    for (const auto& server_entity : server_entities) {
      const sync_pb::ArcPackageSpecifics& server_specifics =
          server_entity.specifics().arc_package();

      bool matched = false;
      for (const auto& expected_entity : expected_entities_) {
        const sync_pb::ArcPackageSpecifics& expected_specifics =
            expected_entity.arc_package();

        if (server_specifics.SerializeAsString() ==
            expected_specifics.SerializeAsString()) {
          matched = true;
          break;
        }
      }
      if (!matched) {
        return false;
      }
    }

    return true;
  }

 private:
  const std::vector<sync_pb::EntitySpecifics> expected_entities_;
};

class SingleClientArcPackageSyncTest : public SyncTest {
 public:
  SingleClientArcPackageSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientArcPackageSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest, ArcPackageEmpty) {
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(ArcAppListPrefs::Get(GetProfile(0))->GetPackagesFromPrefs(),
              IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest,
                       ArcPackageInstallSomePackages) {
  ASSERT_TRUE(SetupSync());

  constexpr size_t kNumPackages = 5;
  std::vector<sync_pb::EntitySpecifics> expected_specifics;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    expected_specifics.push_back(sync_arc_helper()->GetTestSpecifics(i));
  }

  ASSERT_THAT(ArcAppListPrefs::Get(GetProfile(0))->GetPackagesFromPrefs(),
              SizeIs(kNumPackages));
  EXPECT_TRUE(FakeServerArcPackageMatchChecker(expected_specifics).Wait());
}

// Regression test for crbug.com/978837.
IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest, DisableAndReenable) {
  ASSERT_TRUE(SetupSync());

  const size_t kTestPackageId = 0;
  sync_pb::EntitySpecifics specifics =
      sync_arc_helper()->GetTestSpecifics(kTestPackageId);

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

  EXPECT_TRUE(ArcPackagesCountChecker(GetProfile(0), GetSyncService(0),
                                      /*expected_count=*/1)
                  .Wait());
  EXPECT_TRUE(
      sync_arc_helper()->HasOnlyTestPackages(GetProfile(0), {kTestPackageId}));
}

}  // namespace
}  // namespace arc
