// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/send_tab_to_self/send_tab_to_self_bridge.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class TwoClientSendTabToSelfSyncTest : public SyncTest {
 public:
  TwoClientSendTabToSelfSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientSendTabToSelfSyncTest() override {}

 private:
  base::test::ScopedFeatureList scoped_list_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientSendTabToSelfSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       AddedUrlFoundWhenBothClientsAlreadySyncing) {
  const GURL kUrl("https://www.example.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target_device");
  const base::Time kTime = base::Time::FromDoubleT(1);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(kUrl, kHistoryEntryTime, history::SOURCE_BROWSED);

  base::RunLoop run_loop;
  history_service->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  send_tab_to_self::SendTabToSelfModel* model0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(
      model0->AddEntry(kUrl, kTitle, kTime, kTargetDeviceSyncCacheGuid));

  send_tab_to_self::SendTabToSelfSyncService* service1 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1));

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service1, kUrl).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       ModelsMatchAfterAddWhenBothClientsAlreadySyncing) {
  const GURL kGurl0("https://www.example0.com");
  const std::string kTitle0("example0");
  const base::Time kTime0 = base::Time::FromDoubleT(1);
  const std::string kTargetDeviceSyncCacheGuid0("target0");

  const GURL kGurl1("https://www.example1.com");
  const std::string kTitle1("example1");
  const base::Time kTime1 = base::Time::FromDoubleT(2);
  const std::string kTargetDeviceSyncCacheGuid1("target1");

  const GURL kGurl2("https://www.example2.com");
  const std::string kTitle2("example2");
  const base::Time kTime2 = base::Time::FromDoubleT(3);
  const std::string kTargetDeviceSyncCacheGuid2("target2");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  send_tab_to_self::SendTabToSelfModel* model0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(
      model0->AddEntry(kGurl0, kTitle0, kTime0, kTargetDeviceSyncCacheGuid0));

  ASSERT_TRUE(
      model0->AddEntry(kGurl1, kTitle1, kTime1, kTargetDeviceSyncCacheGuid1));

  ASSERT_TRUE(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetSendTabToSelfModel()
          ->AddEntry(kGurl2, kTitle2, kTime2, kTargetDeviceSyncCacheGuid2));

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfModelEqualityChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1)),
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest, IsActive) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfActiveChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
  EXPECT_TRUE(send_tab_to_self::IsUserSyncTypeActive(GetProfile(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest, HasValidTargetDevice) {
  ASSERT_TRUE(SetupSync());

  static_cast<send_tab_to_self::SendTabToSelfBridge*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel())
      ->SetLocalDeviceNameForTest("device1");
  static_cast<send_tab_to_self::SendTabToSelfBridge*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetSendTabToSelfModel())
      ->SetLocalDeviceNameForTest("device2");

  EXPECT_TRUE(send_tab_to_self::HasValidTargetDevice(GetProfile(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       SendTabToSelfReceivingEnabled) {
  ASSERT_TRUE(SetupSync());

  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfMultiDeviceActiveChecker(
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
                      ->GetDeviceInfoTracker())
                  .Wait());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_infos =
      DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetDeviceInfoTracker()
          ->GetAllDeviceInfo();
  ASSERT_EQ(2u, device_infos.size());
  EXPECT_TRUE(device_infos[0]->send_tab_to_self_receiving_enabled());
  EXPECT_TRUE(device_infos[1]->send_tab_to_self_receiving_enabled());
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       SendTabToSelfReceivingDisabled) {
  ASSERT_TRUE(SetupSync());
  GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kTabs);

  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfDeviceDisabledChecker(
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
                      ->GetDeviceInfoTracker(),
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
                      ->GetLocalDeviceInfoProvider()
                      ->GetLocalDeviceInfo()
                      ->guid())
                  .Wait());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_infos =
      DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetDeviceInfoTracker()
          ->GetAllDeviceInfo();
  EXPECT_EQ(2u, device_infos.size());

  EXPECT_NE(device_infos[0]->send_tab_to_self_receiving_enabled(),
            device_infos[1]->send_tab_to_self_receiving_enabled());
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       SendTabToSelfTargetDeviceMap) {
  ASSERT_TRUE(SetupSync());

  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfMultiDeviceActiveChecker(
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
                      ->GetDeviceInfoTracker())
                  .Wait());

  // Explicitly set the two profiles to have different client names to simulate
  // them being on different devices. Otherwise their device infos will get
  // deduped.
  static_cast<send_tab_to_self::SendTabToSelfBridge*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel())
      ->SetLocalDeviceNameForTest("device1");
  static_cast<send_tab_to_self::SendTabToSelfBridge*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetSendTabToSelfModel())
      ->SetLocalDeviceNameForTest("device2");

  std::vector<send_tab_to_self::TargetDeviceInfo> profile1_target_device_map =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList();
  std::vector<send_tab_to_self::TargetDeviceInfo> profile2_target_device_map =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList();

  EXPECT_EQ(1u, profile1_target_device_map.size());
  EXPECT_EQ(1u, profile2_target_device_map.size());
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       MarkOpenedWhenBothClientsAlreadySyncing) {
  const GURL kUrl("https://www.example.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target_device");
  const base::Time kTime = base::Time::FromDoubleT(1);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(kUrl, kHistoryEntryTime, history::SOURCE_BROWSED);

  base::RunLoop run_loop;
  history_service->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  send_tab_to_self::SendTabToSelfSyncService* service0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));

  send_tab_to_self::SendTabToSelfModel* model0 =
      service0->GetSendTabToSelfModel();

  ASSERT_TRUE(
      model0->AddEntry(kUrl, kTitle, kTime, kTargetDeviceSyncCacheGuid));

  send_tab_to_self::SendTabToSelfSyncService* service1 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1));

  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service1, kUrl).Wait());

  const std::string guid = model0->GetAllGuids()[0];

  service1->GetSendTabToSelfModel()->MarkEntryOpened(guid);

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlOpenedChecker(service0, kUrl)
          .Wait());
}
