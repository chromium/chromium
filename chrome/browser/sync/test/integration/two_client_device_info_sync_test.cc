// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/base/features.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using device_info_helper::HasCacheGuid;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

MATCHER_P(ModelEntryHasCacheGuid, expected_cache_guid, "") {
  return arg->guid() == expected_cache_guid;
}

class DeviceInfoDeletedChecker : public StatusChangeChecker,
                                 public syncer::DeviceInfoTracker::Observer {
 public:
  DeviceInfoDeletedChecker(syncer::DeviceInfoTracker* tracker,
                           const std::string& cache_guid)
      : tracker_(tracker), cache_guid_(cache_guid) {
    observation_.Observe(tracker_.get());
  }

  ~DeviceInfoDeletedChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for DeviceInfo " << cache_guid_
        << " to be deleted locally.";
    for (const syncer::DeviceInfo* device : tracker_->GetAllDeviceInfo()) {
      if (device->guid() == cache_guid_) {
        return false;
      }
    }
    return true;
  }

  // DeviceInfoTracker::Observer implementation.
  void OnDeviceInfoChange() override { CheckExitCondition(); }

 private:
  const raw_ptr<syncer::DeviceInfoTracker> tracker_;
  const std::string cache_guid_;
  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      observation_{this};
};

#if !BUILDFLAG(IS_CHROMEOS)
class TwoClientDeviceInfoSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientDeviceInfoSyncTest() : SyncTest(TWO_CLIENT) {
    if (GetParam() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          switches::kMigrateSyncingUserToSignedIn);
    }
  }
  ~TwoClientDeviceInfoSyncTest() override = default;

  std::string GetLocalCacheGuid(int profile_index) const {
    return GetCacheGuid(profile_index);
  }

  syncer::DeviceInfoTracker* GetDeviceInfoTracker(int profile_index) const {
    return DeviceInfoSyncServiceFactory::GetForProfile(
               GetProfile(profile_index))
        ->GetDeviceInfoTracker();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TwoClientDeviceInfoSyncTest,
                       ShouldDeleteDeviceInfoOnOtherClientsWhenSignedOut) {
  ASSERT_TRUE(SetupSync());

  const std::string client0_guid = GetLocalCacheGuid(0);
  const std::string client1_guid = GetLocalCacheGuid(1);

  // Wait for both clients to have their DeviceInfo on the server.
  ASSERT_TRUE(ServerDeviceInfoMatchChecker(
                  UnorderedElementsAre(HasCacheGuid(client0_guid),
                                       HasCacheGuid(client1_guid)))
                  .Wait());

  // Both clients should see each other's DeviceInfo.
  ASSERT_THAT(GetDeviceInfoTracker(0)->GetAllDeviceInfo(),
              UnorderedElementsAre(ModelEntryHasCacheGuid(client0_guid),
                                   ModelEntryHasCacheGuid(client1_guid)));
  ASSERT_THAT(GetDeviceInfoTracker(1)->GetAllDeviceInfo(),
              UnorderedElementsAre(ModelEntryHasCacheGuid(client0_guid),
                                   ModelEntryHasCacheGuid(client1_guid)));

  // Client 0 signs out. This should trigger a SyncDisabledEvent and generate a
  // tombstone for client 0.
  GetClient(0)->SignOutPrimaryAccount();

  // The FakeServer should now only have Client 1's DeviceInfo (and a tombstone
  // for Client 0).
  ASSERT_TRUE(
      ServerDeviceInfoMatchChecker(ElementsAre(HasCacheGuid(client1_guid)))
          .Wait());

  // Client 1 should eventually receive the tombstone and remove Client 0's
  // DeviceInfo from its tracker.
  ASSERT_TRUE(
      DeviceInfoDeletedChecker(GetDeviceInfoTracker(1), client0_guid).Wait());

  EXPECT_THAT(GetDeviceInfoTracker(1)->GetAllDeviceInfo(),
              ElementsAre(ModelEntryHasCacheGuid(client1_guid)));
}

INSTANTIATE_TEST_SUITE_P(, TwoClientDeviceInfoSyncTest, GetSyncTestModes());
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
